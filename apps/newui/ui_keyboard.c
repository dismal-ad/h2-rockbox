/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026
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

/* Text entry (PLAN.md §4.8), iPod style: the text in a field at the top
 * and a strip of characters the wheel slides through, the one in the
 * middle picked with Centre.
 *
 *   ┌──────────────────────────────┐
 *   │ My playlist|                 │   field, cursor
 *   └──────────────────────────────┘
 *     abc   ABC   123   #+=           character sets (Menu)
 *    f  g  h  ┌─┐  j  k  l           the strip
 *             │i│
 *             └─┘
 *   Centre add · Back delete · hold Centre done
 *
 * Wheel: move along the strip. Centre: add the character (or do what the
 * Done / Space / Delete items say). Hold Centre: done. Back: delete, or
 * cancel when the text is empty. Menu: next character set. Prev/Next and
 * the volume keys move the cursor. Power: cancel.
 *
 * Replaces the old grid keyboard; loadable keyboard layouts are not
 * supported. */

#include "config.h"
#include <string.h>
#include "action.h"
#include "font.h"
#include "kernel.h"
#include "keyboard.h"
#include "lang.h"
#include "language.h"
#include "lcd.h"
#include "misc.h"
#include "rbunicode.h"
#include "settings.h"
#include "splash.h"
#include "string-extra.h"
#include "viewport.h"
#include "ui.h"

enum special {
    ITEM_DONE = 1,
    ITEM_SPACE,
    ITEM_DELETE,
};

static const char * const sets[] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    ".,-_'&()!?@#$%+=/:;\"[]{}<>*~^|\\`",
};
static const char * const set_names[] = { "abc", "ABC", "123", "#+=" };
#define SET_COUNT   ((int)(sizeof(sets) / sizeof(sets[0])))
#define SPECIALS    3   /* Done, Space, Delete before each set's characters */

#define FIELD_Y     8
#define FIELD_H     40
#define TABS_Y      64
#define STRIP_Y     104
#define STRIP_H     52
#define SLOT_W      34

static int set_pos[SET_COUNT] = { SPECIALS, SPECIALS, SPECIALS, SPECIALS };

static int item_count(int set)
{
    return SPECIALS + strlen(sets[set]);
}

/* the item's character, or 0 for a special item (*special set) */
static char item_char(int set, int i, enum special *special)
{
    *special = i < SPECIALS ? (enum special)(i + 1) : 0;
    return i < SPECIALS ? 0 : sets[set][i - SPECIALS];
}

static const char *item_label(int set, int i, char *buf)
{
    enum special sp;
    char c = item_char(set, i, &sp);
    switch (sp)
    {
        case ITEM_DONE:   return str(LANG_NEWUI_KBD_DONE);
        case ITEM_SPACE:  return str(LANG_NEWUI_KBD_SPACE);
        case ITEM_DELETE: return str(LANG_DELETE);
    }
    buf[0] = c;
    buf[1] = '\0';
    return buf;
}

static void draw(struct viewport *vp, const char *text, int editpos, int set)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int row = ui_font(UI_FONT_ROW), title = ui_font(UI_FONT_TITLE);
    const int small = ui_font(UI_FONT_SMALL);
    const int fw = vp->width - 2 * UI_MARGIN;
    char buf[8];

    ui_fill_rect(0, 0, vp->width, vp->height, UI_COL_BG);

    /* the field; the text slides left to keep the cursor in view */
    ui_fill_round_rect(UI_MARGIN, FIELD_Y, fw, FIELD_H, 8, UI_COL_TRACK);
    int pos = utf8seek((const unsigned char *)text, editpos);
    char before[MAX_PATH];
    strmemccpy(before, text, MIN(pos + 1, (int)sizeof(before)));
    int cur_x = ui_text_width(row, before);
    int inner = fw - 20;
    int shift = MAX(0, cur_x - inner);
    struct viewport field = *vp;
    field.x = vp->x + UI_MARGIN + 10;
    field.y = vp->y + FIELD_Y;
    field.width = inner + 2;
    field.height = FIELD_H;
    lcd_set_viewport(&field);
    ui_text_vcenter(-shift, 0, FIELD_H, row, UI_COL_TEXT, text, 0, -1);
    ui_fill_rect(cur_x - shift, 8, 2, FIELD_H - 16, UI_COL_TEXT);
    lcd_set_viewport(vp);

    /* character sets, the current one highlighted */
    int tab_w = 56, tx = (vp->width - SET_COUNT * tab_w) / 2;
    for (int s = 0; s < SET_COUNT; s++)
    {
        if (s == set)
            ui_fill_round_rect(tx + s * tab_w + 4, TABS_Y, tab_w - 8, 24, 6,
                               UI_COL_TRACK);
        ui_text_vcenter(tx + s * tab_w, TABS_Y, 24, small,
                        s == set ? UI_COL_TEXT : UI_COL_MUTED, set_names[s],
                        tab_w, 0);
    }

    /* the strip: the current item in a box in the middle */
    int n = item_count(set), cur = set_pos[set];
    int cx = vp->width / 2;
    const char *label = item_label(set, cur, buf);
    int box_w = MAX(SLOT_W, ui_text_width(title, label) + 16);
    ui_fill_round_rect(cx - box_w / 2, STRIP_Y, box_w, STRIP_H, 8, UI_COL_ACCENT);
    ui_text_vcenter(cx - box_w / 2, STRIP_Y, STRIP_H, title, UI_COL_TEXT,
                    label, box_w, 0);
    for (int side = -1; side <= 1; side += 2)
    {
        int edge = cx + side * (box_w / 2 + 4);
        for (int i = cur + side; i >= 0 && i < n; i += side)
        {
            label = item_label(set, i, buf);
            int w = MAX(SLOT_W, ui_text_width(row, label) + 10);
            int x = side > 0 ? edge : edge - w;
            if (x + w < 0 || x > vp->width)
                break;
            int d = (i - cur) * side;
            ui_text_vcenter(x, STRIP_Y, STRIP_H, row,
                            d <= 2 ? UI_COL_TEXT2 : UI_COL_MUTED, label, w, 0);
            edge += side * w;
        }
    }

    ui_text(UI_MARGIN, vp->height - font_get(small)->height - 6, small,
            UI_COL_MUTED, str(LANG_NEWUI_KBD_HINT), fw, 0);
    lcd_update_viewport();
    lcd_set_viewport(last);
}

/* inserts c at character position *editpos */
static void insert(char *text, int buflen, int *editpos, char c)
{
    int len = strlen(text);
    if (len + 2 > buflen)
        return;
    int pos = utf8seek((const unsigned char *)text, *editpos);
    memmove(text + pos + 1, text + pos, len - pos + 1);
    text[pos] = c;
    (*editpos)++;
}

/* deletes the character before *editpos */
static void delete(char *text, int *editpos)
{
    if (*editpos <= 0)
        return;
    int end = utf8seek((const unsigned char *)text, *editpos);
    int start = utf8seek((const unsigned char *)text, *editpos - 1);
    memmove(text + start, text + end, strlen(text + end) + 1);
    (*editpos)--;
}

int kbd_input(char *text, int buflen, ucschar_t *kbd)
{
    (void)kbd;
    struct viewport vp;
    int editpos = utf8length((const unsigned char *)text);
    int set = 0, ret = 0;
    long last_delete = current_tick - HZ;
    bool done = false;

    viewportmanager_theme_enable(SCREEN_MAIN, true, &vp);
    lcd_scroll_stop();
    action_wait_for_release();

    while (!done)
    {
        draw(&vp, text, editpos, set);
        int action = get_action(CONTEXT_KEYBOARD, HZ / 2);
        int n = item_count(set);
        switch (action)
        {
            case ACTION_KBD_RIGHT:
                set_pos[set] = MIN(set_pos[set] + ui_wheel_step(1, n), n - 1);
                break;
            case ACTION_KBD_LEFT:
                set_pos[set] = MAX(set_pos[set] - ui_wheel_step(-1, n), 0);
                break;
            case ACTION_KBD_PAGE_FLIP:
                set = (set + 1) % SET_COUNT;
                break;
            case ACTION_KBD_UP:
            case ACTION_KBD_CURSOR_LEFT:
                if (editpos > 0)
                    editpos--;
                break;
            case ACTION_KBD_DOWN:
            case ACTION_KBD_CURSOR_RIGHT:
                if (editpos < (int)utf8length((const unsigned char *)text))
                    editpos++;
                break;
            case ACTION_KBD_SELECT:
            {
                enum special sp;
                char c = item_char(set, set_pos[set], &sp);
                if (sp == ITEM_DONE)
                    done = true;
                else if (sp == ITEM_SPACE)
                    insert(text, buflen, &editpos, ' ');
                else if (sp == ITEM_DELETE)
                    delete(text, &editpos);
                else
                    insert(text, buflen, &editpos, c);
                break;
            }
            case ACTION_KBD_DONE:
                done = true;
                break;
            case ACTION_KBD_BACKSPACE:
                /* Back on empty text cancels, unless it is held down
                   from deleting */
                if (!text[0] && TIME_AFTER(current_tick, last_delete + HZ / 2))
                {
                    ret = -1;
                    done = true;
                }
                delete(text, &editpos);
                last_delete = current_tick;
                break;
            case ACTION_KBD_ABORT:
                ret = -1;
                done = true;
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = -1;
                    done = true;
                }
                break;
        }
    }

    viewportmanager_theme_undo(SCREEN_MAIN, false);
    if (ret < 0)
        splash(HZ / 2, ID2P(LANG_CANCEL));
    return ret;
}

int load_kbd(unsigned char *filename)
{
    /* only the built-in layout */
    return filename ? -1 : 0;
}
