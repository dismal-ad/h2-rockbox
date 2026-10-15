/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 by Jonathan Gordon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* This file contains the code to draw the list widget on BITMAP LCDs. */

#include "config.h"
#include "system.h"
#include "lcd.h"
#include "font.h"
#include "button.h"
#include "string.h"
#include "settings.h"
#include "kernel.h"
#include "file.h"

#include "action.h"
#include "screen_access.h"
#include "list.h"
#include "scrollbar.h"
#include "lang.h"
#include "sound.h"
#include "misc.h"
#include "viewport.h"
#include "statusbar-skinned.h"
#include "debug.h"
#include "line.h"
#include "fixedpoint.h"
#include "string-extra.h"
#include "newui/ui.h"

/* these are static to make scrolling work */
static struct viewport list_text[NB_SCREENS], title_text[NB_SCREENS];
/* the selected row's text, for the scroll engine */
static struct viewport sel_text[NB_SCREENS];

/* list-private helpers from the generic list.c (move to header?) */
int gui_list_get_item_offset(struct gui_synclist * gui_list, int item_width,
                             int text_pos, struct screen * display,
                             struct viewport *vp);
bool list_display_title(struct gui_synclist *list, enum screen_type screen);
int list_get_nb_lines(struct gui_synclist *list, enum screen_type screen);

void gui_synclist_scroll_stop(struct gui_synclist *lists)
{
    FOR_NB_SCREENS(i)
    {
        screens[i].scroll_stop_viewport(&list_text[i]);
        screens[i].scroll_stop_viewport(&title_text[i]);
        screens[i].scroll_stop_viewport(&sel_text[i]);
        screens[i].scroll_stop_viewport(lists->parent[i]);
    }
}

/* The list is drawn by the new UI's rules (docs/newui/PLAN.md §2):
 * black rows of list->line_height (UI_ROW_H), an optional title row with
 * a hairline under it, the selection as a full-width accent bar (Classic),
 * a chevron on rows that open a submenu or folder, and a thin scrollbar.
 * Unselected text is shortened with an ellipsis; the selected row's text
 * scrolls when it doesn't fit. Item icons are not drawn. */

#define CHEVRON       "\xe2\x80\xba" /* › */
#define CHEVRON_SPACE 14
#define SCROLLBAR_W   3
#define TAB_INDENT    16

static bool item_drills(enum themable_icons icon)
{
    return icon == Icon_Submenu || icon == Icon_Submenu_Entered ||
           icon == Icon_Folder;
}

static void draw_title(struct screen *display, struct gui_synclist *list,
                       struct viewport *vp, int row_h)
{
    ui_text_vcenter(UI_MARGIN, 0, row_h, ui_font(UI_FONT_TITLE), UI_COL_TEXT,
                    list->title, vp->width - 2 * UI_MARGIN, -1);
    ui_fill_rect(0, row_h - 1, vp->width, 1, UI_COL_TRACK);
    (void)display;
}

void list_draw(struct screen *display, struct gui_synclist *list)
{
    const int screen = display->screen_type;
    struct viewport *parent = list->parent[screen];
    struct viewport *vp = &list_text[screen];
    const int row_h = list->line_height[screen];
    const int font = parent->font;

    struct viewport *last_vp = display->set_viewport(parent);
    display->scroll_stop_viewport(vp);
    display->scroll_stop_viewport(&sel_text[screen]);
    display->scroll_stop_viewport(&title_text[screen]);
    ui_fill_rect(0, 0, parent->width, parent->height, UI_COL_BG);

    *vp = *parent;
    if (list_display_title(list, screen))
    {
        draw_title(display, list, parent, row_h);
        vp->y += row_h;
        vp->height -= row_h;
    }

    const int nb_lines = list_get_nb_lines(list, screen);
    const int start = list->start_item[screen];
    const bool scrollbar = list->scrollbar != SCROLLBAR_OFF &&
                           list->nb_items > nb_lines;
    const int text_right = vp->width - UI_MARGIN - (scrollbar ? SCROLLBAR_W + 3 : 0);

    char sel_name[64] = "";
    display->set_viewport(vp);
    for (int i = start; i < start + nb_lines && i < list->nb_items; i++)
    {
        extern char simplelist_buffer[SIMPLELIST_MAX_LINES * SIMPLELIST_MAX_LINELENGTH];
        const unsigned char *s = list->callback_get_item_name(i, list->data,
                                        simplelist_buffer, sizeof(simplelist_buffer));
        const char *name = P2ID((unsigned char *)s) > VOICEONLY_DELIMITER ?
                           "" : (const char *)P2STR(s);
        int indent = 0;
        while (*name == '\t')
        {
            indent++;
            name++;
        }

        enum themable_icons icon = list->callback_get_item_icon ?
                list->callback_get_item_icon(i, list->data) : Icon_NOICON;
        const bool drills = list->callback_item_drills ?
                list->callback_item_drills(i, list->data) : item_drills(icon);
        const bool selected = i >= list->selected_item &&
                              i < list->selected_item + list->selected_size;
        const int y = (i - start) * row_h;
        const int text_x = UI_MARGIN + indent * TAB_INDENT;
        int text_w = text_right - text_x - (drills ? CHEVRON_SPACE : 0);

        /* a value on the right takes up to half of the row: from the
           list's value callback, or the text after a TAB in the name */
        char valbuf[64], namebuf[MAX_PATH];
        const char *value = list->callback_get_item_value ?
                list->callback_get_item_value(i, list->data, valbuf, sizeof(valbuf)) : NULL;
        const char *tab = strchr(name, '\t');
        if (tab)
        {
            size_t n = MIN((size_t)(tab - name), sizeof(namebuf) - 1);
            memcpy(namebuf, name, n);
            namebuf[n] = '\0';
            if (!value)
                value = tab + 1;
            name = namebuf;
        }
        int value_w = 0;
        if (value && *value)
        {
            value_w = MIN(ui_text_width(ui_font(UI_FONT_BODY), value), text_w / 2);
            text_w -= value_w + UI_GAP;
        }

        if (selected)
        {
            strmemccpy(sel_name, name, sizeof(sel_name));
            ui_fill_rect(0, y, vp->width - (scrollbar ? SCROLLBAR_W + 3 : 0),
                         row_h, UI_COL_ACCENT);
            if (ui_text_width(font, name) <= text_w)
            {
                ui_text_vcenter(text_x, y, row_h, font, UI_COL_TEXT, name, text_w, -1);
            }
            else
            {
                /* too long: let the scroll engine scroll it */
                struct viewport *sv = &sel_text[screen];
                struct line_desc ld = LINE_DESC_DEFINIT;
                *sv = *vp;
                sv->x = vp->x + text_x;
                sv->y = vp->y + y;
                sv->width = text_w;
                sv->height = row_h;
                ld.height = row_h;
                ld.style = STYLE_COLORBAR;
                ld.line_color = UI_COL_ACCENT;
                ld.text_color = UI_COL_TEXT;
                ld.scroll = true;
                display->set_viewport(sv);
                display->put_line(0, 0, &ld, "$t", name);
                display->set_viewport(vp);
            }
        }
        else
        {
            unsigned color = UI_COL_TEXT;
            if (list->callback_get_item_color)
            {
                int c = list->callback_get_item_color(i, list->data);
                if (c >= 0)
                    color = c;
            }
            ui_text_vcenter(text_x, y, row_h, font, color, name, text_w, -1);
        }

        if (value_w)
            ui_text_vcenter(text_x + text_w + UI_GAP, y, row_h, ui_font(UI_FONT_BODY),
                            selected ? UI_COL_TEXT : UI_COL_TEXT2, value,
                            value_w, 1);

        if (drills)
            ui_text_vcenter(text_right - 7, y, row_h, font,
                            selected ? UI_COL_TEXT : UI_COL_TEXT2, CHEVRON, 0, -1);
    }

    if (scrollbar)
    {
        int track_h = nb_lines * row_h;
        int thumb_h = track_h * nb_lines / list->nb_items;
        if (thumb_h < 12)
            thumb_h = 12;
        int thumb_y = (track_h - thumb_h) * start /
                      (list->nb_items - nb_lines > 0 ? list->nb_items - nb_lines : 1);
        ui_fill_round_rect(vp->width - SCROLLBAR_W - 2, thumb_y + 2,
                           SCROLLBAR_W, thumb_h - 4, 1, UI_COL_MUTED);
    }

    if (ui_wheel_letter_visible() && sel_name[0])
        ui_wheel_draw_letter(sel_name);

    display->set_viewport(last_vp);
}
