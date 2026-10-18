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

/* The status bar (PLAN.md §4.1): play state on the left, clock in the
 * centre, Bluetooth and battery on the right. Drawn into the current
 * viewport, UI_STATUS_H high. */

#include "config.h"
#include <stdio.h>
#include "lcd.h"
#include "status.h"
#include "timefuncs.h"
#include "string-extra.h"
#include "statusbar.h"
#include "ui.h"

#define GLYPH_Y     6   /* top of the 10 px glyphs */
#define GLYPH_H     10
#define BATT_W      23
#define BATT_H      11

#define COL_LOW      LCD_RGBPACK(0xff, 0x45, 0x3a)
#define COL_CHARGING LCD_RGBPACK(0x30, 0xd1, 0x58)

static char status_title[48];
static int status_width;

void ui_status_title(const char *title)
{
    if (!title)
        title = "";
    if (!strcmp(title, status_title))
        return;
    strmemccpy(status_title, title, sizeof(status_title));
    statusbar_update(true);
}

const char *ui_status_get_title(void)
{
    return status_title;
}

void ui_status_width(int width)
{
    if (width == status_width)
        return;
    status_width = width;
    statusbar_update(true);
}

int ui_status_get_width(void)
{
    return status_width;
}

static void draw_play_state(int x, int mode)
{
    unsigned c = UI_COL_TEXT;
    switch (mode)
    {
        case STATUS_PLAY:
            ui_fill_triangle_right(x, GLYPH_Y, 9, GLYPH_H, c);
            break;
        case STATUS_PAUSE:
            ui_fill_rect(x, GLYPH_Y, 3, GLYPH_H, c);
            ui_fill_rect(x + 6, GLYPH_Y, 3, GLYPH_H, c);
            break;
        case STATUS_FASTFORWARD:
            ui_fill_triangle_right(x, GLYPH_Y + 1, 6, GLYPH_H - 2, c);
            ui_fill_triangle_right(x + 6, GLYPH_Y + 1, 6, GLYPH_H - 2, c);
            break;
        case STATUS_FASTBACKWARD:
            /* mirror of fast forward: draw two left-pointing triangles */
            for (int row = 0; row < GLYPH_H - 2; row++)
            {
                int twice = 2 * row + 1, h = GLYPH_H - 2;
                int len = (twice <= h ? twice : 2 * h - twice) * 6 / h;
                ui_fill_rect(x + 6 - len, GLYPH_Y + 1 + row, len, 1, c);
                ui_fill_rect(x + 12 - len, GLYPH_Y + 1 + row, len, 1, c);
            }
            break;
        default: /* stopped: nothing, like the iPod */
            break;
    }
}

static void draw_lock(int x)
{
    /* shackle, then body */
    ui_stroke_round_rect(x + 1, GLYPH_Y - 1, 7, 8, 3, 1, UI_COL_TEXT);
    ui_fill_round_rect(x, GLYPH_Y + 4, 9, 7, 1, UI_COL_TEXT);
}

static void draw_bluetooth(int x, unsigned c)
{
    /* the Bluetooth rune, 7 x 12 */
    int y = GLYPH_Y - 1;
    lcd_set_foreground(c);
    lcd_vline(x + 3, y, y + 11);
    lcd_drawline(x + 3, y, x + 6, y + 3);
    lcd_drawline(x + 6, y + 3, x, y + 8);
    lcd_drawline(x + 3, y + 11, x + 6, y + 8);
    lcd_drawline(x + 6, y + 8, x, y + 3);
}

static void draw_battery(int x, int percent, bool charging)
{
    unsigned c = UI_COL_TEXT, fill = UI_COL_TEXT;
    if (charging)
        fill = COL_CHARGING;
    else if (percent >= 0 && percent <= 15)
        fill = COL_LOW;

    int y = (UI_STATUS_H - BATT_H) / 2;
    ui_stroke_round_rect(x, y, BATT_W, BATT_H, 3, 1, c);
    ui_fill_round_rect(x + BATT_W, y + 3, 2, BATT_H - 6, 1, c);
    if (percent < 0)
        return;
    int inner = BATT_W - 4;
    int w = (inner * (percent > 100 ? 100 : percent) + 50) / 100;
    if (w < 1)
        w = 1;
    ui_fill_round_rect(x + 2, y + 2, w, BATT_H - 4, 1, fill);
}

void ui_status_draw(const struct ui_status *st)
{
    struct viewport *vp = lcd_current_viewport;
    int font = ui_font(UI_FONT_SMALL);

    {
        /* the iPod's bar: a vertical gradient (silver, or dark grey) and a
           hairline */
        unsigned top = ui_col(UI_C_BAR_TOP), bot = ui_col(UI_C_BAR_BOTTOM);
        int h = vp->height - 1;
        for (int y = 0; y < h; y++)
        {
            int t = y * 256 / (h - 1);
            unsigned c = LCD_RGBPACK(
                RGB_UNPACK_RED(top) + ((RGB_UNPACK_RED(bot) - RGB_UNPACK_RED(top)) * t >> 8),
                RGB_UNPACK_GREEN(top) + ((RGB_UNPACK_GREEN(bot) - RGB_UNPACK_GREEN(top)) * t >> 8),
                RGB_UNPACK_BLUE(top) + ((RGB_UNPACK_BLUE(bot) - RGB_UNPACK_BLUE(top)) * t >> 8));
            ui_fill_rect(0, y, vp->width, 1, c);
        }
        ui_fill_rect(0, h, vp->width, 1, ui_col(UI_C_BAR_LINE));
    }

    const bool titled = st->title && st->title[0];
    int x = UI_MARGIN;
    if (titled)
    {
        /* the iPod's layout: the screen's name left, the play state with
           the battery on the right */
        ui_text_vcenter(UI_MARGIN - 2, 0, UI_STATUS_H, ui_font(UI_FONT_BODY),
                        UI_COL_TEXT, st->title, vp->width - 2 * UI_MARGIN - 48, -1);
    }
    else
    {
        /* left: play state, then the key lock */
        draw_play_state(x, st->playmode);
        if (st->keylock)
            draw_lock(x + 18);

        /* centre: clock */
        if (st->time)
        {
            char buf[16];
            int h = st->time->tm_hour, m = st->time->tm_min;
            if (st->clock_24h)
                snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
            else
                snprintf(buf, sizeof(buf), "%d:%02d %s",
                         (h % 12) ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
            ui_text_vcenter(0, 0, UI_STATUS_H, font, UI_COL_TEXT, buf, vp->width, 0);
        }
    }

    /* right: battery, Bluetooth to its left */
    x = vp->width - UI_MARGIN - BATT_W - 2;
    if (st->battery_visible)
        draw_battery(x, st->battery, st->charging);
    if (st->battery_pct && st->battery >= 0)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", st->battery);
        int w = ui_text_width(font, buf);
        x -= w + 5;
        ui_text_vcenter(x, 0, UI_STATUS_H, font, UI_COL_TEXT, buf, 0, -1);
    }
    if (st->bluetooth)
    {
        x -= 14;
        draw_bluetooth(x, st->bluetooth > 1 ? UI_COL_TEXT : UI_COL_MUTED);
    }
    if (titled)
    {
        /* the play state in the accent colour, the key lock before it */
        x -= 16;
        if (st->playmode == STATUS_PLAY)
            ui_fill_triangle_right(x, GLYPH_Y, 9, GLYPH_H, ui_accent());
        else
            draw_play_state(x, st->playmode);
        if (st->keylock)
            draw_lock(x - 14);
    }
}
