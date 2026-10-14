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

#define ICON_PADDING 1
#define ICON_PADDING_S "1"

/* these are static to make scrolling work */
static struct viewport list_text[NB_SCREENS], title_text[NB_SCREENS];

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
        screens[i].scroll_stop_viewport(lists->parent[i]);
    }
}

/* Draw the list...
    internal screen layout:
        -----------------
        |TI|  title     |   TI is title icon
        -----------------
        | | |            |
        |S|I|            |   S - scrollbar
        | | | items      |   I - icons
        | | |            |
        ------------------

        Note: This image is flipped horizontally when the language is a
        right-to-left one (Hebrew, Arabic)
*/

static int list_icon_width(enum screen_type screen)
{
    return get_icon_width(screen) + ICON_PADDING * 2;
}

static void _default_listdraw_fn(struct list_putlineinfo_t *list_info)
{
    struct screen *display = list_info->display; 
    int x = list_info->x;
    int y = list_info->y;
    int item_indent = list_info->item_indent;
    int item_offset = list_info->item_offset;
    int icon = list_info->icon;
    bool is_selected = list_info->is_selected;
    bool is_title = list_info->is_title;
    bool show_cursor = list_info->show_cursor;
    bool have_icons = list_info->have_icons;
    struct line_desc *linedes = list_info->linedes;
    const char *dsp_text = list_info->dsp_text;

    if (is_title)
    {
        if (have_icons)
            display->put_line(x, y, linedes, "$"ICON_PADDING_S"I$t",
                    icon, dsp_text);
        else
            display->put_line(x, y, linedes, "$t", dsp_text);
    }
    else if (show_cursor && have_icons)
    {
    /* the list can have both, one of or neither of cursor and item icons,
     * if both don't apply icon padding twice between the icons */
        display->put_line(x, y, 
                linedes, "$*s$"ICON_PADDING_S"I$i$"ICON_PADDING_S"s$*t",
                item_indent, is_selected ? Icon_Cursor : Icon_NOICON,
                icon, item_offset, dsp_text);
    }
    else if (show_cursor || have_icons)
    {
        display->put_line(x, y, linedes, "$*s$"ICON_PADDING_S"I$*t", item_indent,
                show_cursor ? (is_selected ? Icon_Cursor:Icon_NOICON):icon,
                item_offset, dsp_text);
    }
    else
    {
        display->put_line(x, y, linedes, "$*s$*t", item_indent, item_offset, dsp_text);
    }
}

static bool draw_title(struct screen *display,
                       struct gui_synclist *list,
                       list_draw_item *callback_draw_item)
{
    const int screen = display->screen_type;
    struct viewport *title_text_vp = &title_text[screen];
    struct line_desc linedes = LINE_DESC_DEFINIT;

    if (sb_set_title_text(list->title, list->title_icon, screen))
        return false; /* the sbs is handling the title */
    display->scroll_stop_viewport(title_text_vp);
    if (!list_display_title(list, screen))
        return false;
    *title_text_vp = *(list->parent[screen]);
    linedes.height = list->line_height[screen];
    title_text_vp->height = linedes.height;

    /* XXX: Do we want to support the separator on remote displays? */
    if (display->screen_type == SCREEN_MAIN && global_settings.list_separator_height != 0)
        linedes.separator_height = abs(global_settings.list_separator_height)
                                + (lcd_get_dpi() > 200 ? 2 : 1);

    if (list->title_color >= 0)
        linedes.style |= (STYLE_COLORED|list->title_color);
    linedes.scroll = true;

    display->set_viewport(title_text_vp);
    int icon = list->title_icon;
    int icon_w = list_icon_width(display->screen_type);
    bool have_icons = false;
    if (icon != Icon_NOICON && list->show_icons)
    {
        have_icons = true;
    }

    struct list_putlineinfo_t list_info =
    {
        .x = 0, .y = 0, .item_indent = 0, .item_offset = 0,
         .line = -1, .icon = icon, .icon_width = icon_w,
        .display = display, .vp = title_text_vp, .linedes = &linedes, .list = list,
        .dsp_text = list->title,
        .is_selected = false, .is_title = true, .show_cursor = false,
        .have_icons = have_icons
    };
    callback_draw_item(&list_info);

    return true;
}

void list_draw(struct screen *display, struct gui_synclist *list)
{
    int start, end, item_offset, i;
    const int screen = display->screen_type;
    list_draw_item *callback_draw_item;

    const int list_start_item = list->start_item[screen];
    const bool scrollbar_in_left = (list->scrollbar == SCROLLBAR_LEFT);
    const bool scrollbar_in_right = (list->scrollbar == SCROLLBAR_RIGHT);
    const bool show_cursor = (list->cursor_style == SYNCLIST_CURSOR_NOSTYLE);
    const bool have_icons = list->callback_get_item_icon && list->show_icons;

    struct viewport *parent = (list->parent[screen]);
    struct line_desc linedes = LINE_DESC_DEFINIT;
    bool show_title;
    struct viewport *list_text_vp = &list_text[screen];
    int indent = 0;

    if (list->callback_draw_item != NULL)
        callback_draw_item = list->callback_draw_item;
    else
        callback_draw_item = _default_listdraw_fn;

    struct viewport * last_vp = display->set_viewport(parent);
    display->clear_viewport();
    if (!list->scroll_all)
        display->scroll_stop_viewport(list_text_vp);
    *list_text_vp = *parent;
    if ((show_title = draw_title(display, list, callback_draw_item)))
    {
        int title_height = title_text[screen].height;
        list_text_vp->y += title_height;
        list_text_vp->height -= title_height;
    }

    const int nb_lines = list_get_nb_lines(list, screen);

    linedes.height = list->line_height[screen];
    linedes.nlines = list->selected_size;
    /* XXX: Do we want to support the separator on remote displays? */
    if (display->screen_type == SCREEN_MAIN)
        linedes.separator_height = abs(global_settings.list_separator_height);
    start = list_start_item;
    end = start + nb_lines;

    #define draw_offset 0

    /* draw the scrollbar if its needed */
    if (list->scrollbar != SCROLLBAR_OFF)
    {
        /* if the scrollbar is shown the text viewport needs to shrink */
        if (nb_lines < list->nb_items)
        {
            struct viewport vp = *list_text_vp;
            vp.width = SCROLLBAR_WIDTH;
            /* touchscreens must use full viewport height
             * due to pixelwise rendering */
            vp.height = linedes.height * nb_lines;
            list_text_vp->width -= SCROLLBAR_WIDTH;
            if (scrollbar_in_right)
                vp.x += list_text_vp->width;
            else /* left */
                list_text_vp->x += SCROLLBAR_WIDTH;
            struct viewport *last = display->set_viewport(&vp);

            /* button targets go itemwise */
            int scrollbar_items = list->nb_items;
            int scrollbar_min = list_start_item;
            int scrollbar_max = list_start_item + nb_lines;
            gui_scrollbar_draw(display,
                    (scrollbar_in_left? 0: 1), 0, SCROLLBAR_WIDTH-1, vp.height,
                    scrollbar_items, scrollbar_min, scrollbar_max, VERTICAL);
            display->set_viewport(last);
        }
        /* shift everything a bit in relation to the title */
        else if (!VP_IS_RTL(list_text_vp) && scrollbar_in_left)
            indent += SCROLLBAR_WIDTH;
        else if (VP_IS_RTL(list_text_vp) && scrollbar_in_right)
            indent += SCROLLBAR_WIDTH;
    }

    display->set_viewport(list_text_vp);
    int icon_w = list_icon_width(screen);
    int character_width = display->getcharwidth();

    struct list_putlineinfo_t list_info =
    {
        .x = 0, .y = 0, .vp = list_text_vp, .list = list,
        .icon_width = icon_w, .is_title = false, .show_cursor = show_cursor,
        .have_icons = have_icons, .linedes = &linedes, .display = display
    };

    for (i=start; i<end && i<list->nb_items; i++)
    {
        /* do the text */
        enum themable_icons icon;
        unsigned const char *s;
        extern char simplelist_buffer[SIMPLELIST_MAX_LINES * SIMPLELIST_MAX_LINELENGTH];
        /*char entry_buffer[MAX_PATH]; use the buffer from gui/list.c instead */
        unsigned char *entry_name;
        int line = i - start;
        int line_indent = 0;
        int style = STYLE_DEFAULT;
        bool is_selected = false;
        s = list->callback_get_item_name(i, list->data, simplelist_buffer,
                                         sizeof(simplelist_buffer));
        if (P2ID((unsigned char *)s) > VOICEONLY_DELIMITER)
            entry_name = "";
        else
            entry_name = P2STR(s);

        while (*entry_name == '\t')
        {
            line_indent++;
            entry_name++;
        }
        if (line_indent)
        {
            if (list->show_icons)
                line_indent *= icon_w;
            else
                line_indent *= character_width;
        }
        line_indent += indent;

        /* position the string at the correct offset place */
        int item_width,h;
        display->getstringsize(entry_name, &item_width, &h);
        item_offset = gui_list_get_item_offset(list, item_width, indent + (list->show_icons ? icon_w : 0),
                display, list_text_vp);

        /* draw the selected line */
        if(
                i >= list->selected_item
                && i <  list->selected_item + list->selected_size)
        {/* The selected item must be displayed scrolling */
            if (list->selection_color)
            {
                /* Display gradient line selector */
                style = STYLE_GRADIENT;
                linedes.text_color = list->selection_color->text_color;
                linedes.line_color = list->selection_color->line_color;
                linedes.line_end_color = list->selection_color->line_end_color;
            }
            else
            if (list->cursor_style == SYNCLIST_CURSOR_INVERT
            )
            {
                /* Display inverted-line-style */
                style = STYLE_INVERT;
            }
            else if (list->cursor_style == SYNCLIST_CURSOR_COLOR)
            {
                /* Display colour line selector */
                style = STYLE_COLORBAR;
                linedes.text_color = global_settings.lst_color;
                linedes.line_color = global_settings.lss_color;
            }
            else if (list->cursor_style == SYNCLIST_CURSOR_GRADIENT)
            {
                /* Display gradient line selector */
                style = STYLE_GRADIENT;
                linedes.text_color = global_settings.lst_color;
                linedes.line_color = global_settings.lss_color;
                linedes.line_end_color = global_settings.lse_color;
            }
            is_selected = true;
        }
        
        /* if the list has a color callback */
        if (list->callback_get_item_color)
        {
            int c = list->callback_get_item_color(i, list->data);
            if (c >= 0)
            {   /* if color selected */
                linedes.text_color = c;
                style |= STYLE_COLORED;
            }
        }
        linedes.style = style;
        linedes.scroll = is_selected ? true : list->scroll_all;
        linedes.line = i % list->selected_size;
        icon = list->callback_get_item_icon ?
                    list->callback_get_item_icon(i, list->data) : Icon_NOICON;


        list_info.y = line * linedes.height + draw_offset;
        list_info.is_selected = is_selected;
        list_info.item_indent = line_indent;
        list_info.line = i;
        list_info.icon = icon;
        list_info.dsp_text = entry_name;
        list_info.item_offset = item_offset;

        callback_draw_item(&list_info);
    }
    display->set_viewport(last_vp);
}

