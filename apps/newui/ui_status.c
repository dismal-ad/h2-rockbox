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

/* Colours: the play icon and the Bluetooth icon (while Bluetooth is on)
 * in the accent colour, everything else in the theme's soft black or soft
 * white (UI_COL_STATUS); the battery's fill red when low, green when
 * charging. */

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

/* glyphs are centred on ICON_YC, the title's capitals' middle (the bar is
 * 21 px over its hairline) */
#define ICON_YC     11
#define GLYPH_H     13
#define GLYPH_Y     (ICON_YC - GLYPH_H / 2)
#define BATT_W      26
#define BATT_H      13

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

#define PLAY_W      11  /* play triangle's width */

static void draw_play_state(int x, int mode)
{
    unsigned c = UI_COL_STATUS;
    switch (mode)
    {
        case STATUS_PLAY:
            ui_fill_triangle_right(x, GLYPH_Y, PLAY_W, GLYPH_H, ui_accent());
            break;
        case STATUS_PAUSE:
            ui_fill_rect(x, GLYPH_Y, 4, GLYPH_H, c);
            ui_fill_rect(x + 7, GLYPH_Y, 4, GLYPH_H, c);
            break;
        case STATUS_FASTFORWARD:
            ui_fill_triangle_right(x, GLYPH_Y + 1, 7, GLYPH_H - 2, c);
            ui_fill_triangle_right(x + 7, GLYPH_Y + 1, 7, GLYPH_H - 2, c);
            break;
        case STATUS_FASTBACKWARD:
            /* mirror of fast forward: draw two left-pointing triangles */
            for (int row = 0; row < GLYPH_H - 2; row++)
            {
                int twice = 2 * row + 1, h = GLYPH_H - 2;
                int len = (twice <= h ? twice : 2 * h - twice) * 7 / h;
                ui_fill_rect(x + 7 - len, GLYPH_Y + 1 + row, len, 1, c);
                ui_fill_rect(x + 14 - len, GLYPH_Y + 1 + row, len, 1, c);
            }
            break;
        default: /* stopped: nothing, like the iPod */
            break;
    }
}

static void draw_lock(int x)
{
    /* shackle (2 px), then body */
    ui_stroke_round_rect(x + 2, GLYPH_Y, 7, 8, 3, 2, UI_COL_STATUS);
    ui_fill_round_rect(x, GLYPH_Y + 5, 11, GLYPH_H - 5, 2, UI_COL_STATUS);
}

static void draw_bluetooth(int x, unsigned c)
{
    /* the Bluetooth rune, 8 x 13, its strokes doubled to be bold */
    int y = GLYPH_Y;
    lcd_set_foreground(c);
    for (int d = 0; d < 2; d++)
    {
        lcd_vline(x + 3 + d, y, y + 12);
        lcd_drawline(x + 3 + d, y, x + 7, y + 3 + d);
        lcd_drawline(x + 7, y + 3 + d, x + d, y + 9);
        lcd_drawline(x + 3 + d, y + 12, x + 7, y + 9 - d);
        lcd_drawline(x + 7, y + 9 - d, x + d, y + 3);
    }
}

static void draw_battery(int x, int percent, bool charging)
{
    unsigned c = UI_COL_STATUS, fill = UI_COL_STATUS;
    if (charging)
        fill = COL_CHARGING;
    else if (percent >= 0 && percent <= 15)
        fill = COL_LOW;

    int y = ICON_YC - BATT_H / 2;
    ui_stroke_round_rect(x, y, BATT_W, BATT_H, 3, 2, c);
    ui_fill_round_rect(x + BATT_W, y + 4, 3, BATT_H - 8, 1, c);
    if (percent < 0)
        return;
    int inner = BATT_W - 6;
    int w = (inner * (percent > 100 ? 100 : percent) + 50) / 100;
    if (w < 1)
        w = 1;
    ui_fill_rect(x + 3, y + 3, w, BATT_H - 6, fill);
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
        /* drawn last, in the room the right side's icons leave */
    }
    else
    {
        /* left: play state, then the key lock */
        draw_play_state(x, st->playmode);
        if (st->keylock)
            draw_lock(x + PLAY_W + 9);

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
            ui_text_vcenter(0, 0, UI_STATUS_H, font, UI_COL_STATUS, buf, vp->width, 0);
        }
    }

    /* right: battery, Bluetooth to its left */
    x = vp->width - UI_MARGIN - BATT_W - 3;
    if (st->battery_visible)
        draw_battery(x, st->battery, st->charging);
    if (st->battery_pct && st->battery >= 0)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", st->battery);
        int w = ui_text_width(font, buf);
        x -= w + 5;
        ui_text_vcenter(x, 0, UI_STATUS_H, font, UI_COL_STATUS, buf, 0, -1);
    }
    if (st->bluetooth)
    {
        x -= 16;
        draw_bluetooth(x, ui_accent());   /* Bluetooth is on */
    }
    if (titled)
    {
        /* the play state in the accent colour, the key lock before it */
        x -= PLAY_W + 8;
        draw_play_state(x, st->playmode);
        if (st->keylock)
        {
            x -= 17;
            draw_lock(x);
        }
        /* the title, shortened with "..." before the icons */
        int tx = UI_MARGIN - 2;
        ui_text_vcenter(tx, 0, UI_STATUS_H, ui_font(UI_FONT_BODY), UI_COL_STATUS,
                        st->title, x - UI_GAP - tx, -1);
    }
}
