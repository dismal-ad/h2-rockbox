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
int list_title_height(struct gui_synclist *list);

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

/* A row with a cover on the left, the name and a grey second line */
static void draw_art_row(struct viewport *vp, int y, int row_h,
                         const char *name, char *line2,
                         const fb_data *thumb, bool selected, bool drills,
                         int text_right, bool scrollbar)
{
    const int ax = UI_MARGIN, ay = y + (row_h - UI_THUMB) / 2;
    const int tx = ax + UI_THUMB + 10;
    const int tw = text_right - tx - (drills ? CHEVRON_SPACE : 0);
    unsigned bg = selected ? ui_sel_bg() : UI_COL_BG;

    if (selected)
        ui_selection(0, y, vp->width - (scrollbar ? SCROLLBAR_W + 3 : 0), row_h);
    if (thumb)
    {
        lcd_bitmap(thumb, ax, ay, UI_THUMB, UI_THUMB);
        ui_round_corners(ax, ay, UI_THUMB, UI_THUMB, 6, bg);
    }
    else
        ui_art_placeholder(ax, ay, UI_THUMB, 6);

    int row = ui_font(UI_FONT_ROW), body = ui_font(UI_FONT_BODY);
    if (line2[0])
    {
        int top = y + (row_h - font_get(row)->height - font_get(body)->height) / 2;
        ui_text(tx, top, row, selected ? ui_sel_text() : UI_COL_TEXT, name, tw, -1);
        /* a TAB splits the second line: the part after it stays whole,
         * on the right */
        unsigned col2 = selected ? ui_sel_text2() : UI_COL_TEXT2;
        int y2 = top + font_get(row)->height, lw = tw;
        char *tab = strchr(line2, '\t');
        if (tab)
        {
            *tab++ = '\0';
            lw -= ui_text_width(body, tab) + UI_GAP;
            ui_text(tx, y2, body, col2, tab, tw, 1);
        }
        ui_text(tx, y2, body, col2, line2, lw, -1);
    }
    else
        ui_text_vcenter(tx, y, row_h, row, selected ? ui_sel_text() : UI_COL_TEXT,
                        name, tw, -1);

    if (drills)
        ui_text_vcenter(text_right - 7, y, row_h, row,
                        selected ? ui_sel_text() : UI_COL_TEXT2, CHEVRON, 0, -1);
}

static void draw_title(struct screen *display, struct gui_synclist *list,
                       struct viewport *vp, int row_h)
{
    const fb_data *thumb = NULL;
    char line2[96];
    if (list->callback_get_header &&
        list->callback_get_header(list->data, &thumb, line2, sizeof(line2)))
    {
        draw_art_row(vp, 0, row_h, list->title, line2, thumb, false, false,
                     vp->width - UI_MARGIN, false);
        ui_fill_rect(0, row_h - 1, vp->width, 1, UI_COL_TRACK);
        return;
    }
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
    /* only lists under the status bar name it */
    if (parent->y >= UI_STATUS_H)
        ui_status_title(list->title);
    if (list_display_title(list, screen))
    {
        const int title_h = list_title_height(list);
        draw_title(display, list, parent, title_h);
        vp->y += title_h;
        vp->height -= title_h;
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

        /* a row with a cover: art, name, second line (album lists) */
        const fb_data *thumb = NULL;
        char line2[64] = "";
        if (list->callback_get_art_row &&
            list->callback_get_art_row(i, list->data, &thumb, line2, sizeof(line2)))
        {
            if (selected)
                strmemccpy(sel_name, name, sizeof(sel_name));
            draw_art_row(vp, y, row_h, name, line2, thumb, selected, drills,
                         text_right, scrollbar);
            continue;
        }

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
            ui_selection(0, y, vp->width - (scrollbar ? SCROLLBAR_W + 3 : 0), row_h);
            if (ui_text_width(font, name) <= text_w)
            {
                ui_text_vcenter(text_x, y, row_h, font, ui_sel_text(), name, text_w, -1);
            }
            else
            {
                /* too long: let the scroll engine scroll it */
                struct viewport *sv = &sel_text[screen];
                struct line_desc ld = LINE_DESC_DEFINIT;
                /* the scroller centres the line box: move it so the text
                   sits where ui_text_vcenter() puts it */
                int shift = ui_text_y(font, 0, row_h) -
                            (row_h - (int)font_get(ui_resolve_font(font))->height) / 2;
                *sv = *vp;
                sv->x = vp->x + text_x;
                sv->y = vp->y + y + shift;
                sv->width = text_w;
                sv->height = row_h - shift;
                ld.height = row_h - shift;
                ld.style = ui_light() && !ui_minimal() ? STYLE_GRADIENT : STYLE_COLORBAR;
                ld.line_color = ld.style == STYLE_GRADIENT ? ui_sel_top() : ui_sel_bg();
                ld.line_end_color = ui_sel_bg();
                ld.text_color = ui_sel_text();
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
                            selected ? ui_sel_text2() : UI_COL_TEXT2, value,
                            value_w, 1);

        if (drills)
            ui_text_vcenter(text_right - 7, y, row_h, font,
                            selected ? ui_sel_text() : UI_COL_TEXT2, CHEVRON, 0, -1);
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
