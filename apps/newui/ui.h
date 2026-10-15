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

/* The new code-drawn UI (see docs/newui/PLAN.md). */

#ifndef NEWUI_UI_H
#define NEWUI_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "lcd.h"

/* --- design tokens (PLAN.md §2) --- */

#define UI_STATUS_H     22  /* status bar height */
#define UI_MARGIN       12  /* side margin */
#define UI_GAP           8
#define UI_ROW_H        27  /* text row */
#define UI_ART_ROW_H    54  /* row with a cover */

#define UI_COL_BG       LCD_RGBPACK(0x00, 0x00, 0x00)
#define UI_COL_TEXT     LCD_RGBPACK(0xff, 0xff, 0xff)
#define UI_COL_TEXT2    LCD_RGBPACK(0xaa, 0xaa, 0xaa) /* secondary */
#define UI_COL_MUTED    LCD_RGBPACK(0x8c, 0x8c, 0x8c)
#define UI_COL_SEL      LCD_RGBPACK(0xee, 0xee, 0xec) /* Minimal selection */
#define UI_COL_SEL_TEXT LCD_RGBPACK(0x17, 0x17, 0x17)
#define UI_COL_TRACK    LCD_RGBPACK(0x1c, 0x1c, 0x1c) /* progress track */
#define UI_COL_ACCENT   LCD_RGBPACK(0x6e, 0x6e, 0x6e) /* Classic: Graphite */

/* --- fonts (ui_font.c) --- */

enum ui_font {
    UI_FONT_SMALL,  /* 12: times, counts, status bar */
    UI_FONT_BODY,   /* 15: list rows, artist/album */
    UI_FONT_TITLE,  /* 18 semibold: titles */
    UI_FONT_BIG,    /* 40 semibold, ASCII: fast-scroll letter, volume */
    UI_FONT_COUNT
};

/* Loads the fonts once; safe to call again. Missing fonts fall back to the
 * system font. */
void ui_font_init(void);
/* Rockbox font id for one of the UI fonts */
int ui_font(enum ui_font f);

/* --- drawing (ui_draw.c); coordinates relative to the current viewport --- */

void ui_blend_pixel(int x, int y, unsigned color, int alpha); /* alpha 0..256 */
void ui_fill_rect(int x, int y, int w, int h, unsigned color);
void ui_fill_round_rect(int x, int y, int w, int h, int r, unsigned color);
void ui_stroke_round_rect(int x, int y, int w, int h, int r, int stroke,
                          unsigned color);
void ui_fill_triangle_right(int x, int y, int w, int h, unsigned color);
int ui_text_width(int font, const char *text);
/* Transparent text, shortened with an ellipsis to max_w (0: no limit);
 * align -1 left, 0 centre, 1 right within max_w. Returns the drawn width. */
int ui_text(int x, int y, int font, unsigned color, const char *text,
            int max_w, int align);
/* Same, vertically centred in the band [y, y + h) */
int ui_text_vcenter(int x, int y, int h, int font, unsigned color,
                    const char *text, int max_w, int align);

/* --- status bar (ui_status.c) --- */

struct tm;
struct ui_status {
    int playmode;          /* enum playmode (status.h) */
    bool keylock;
    const struct tm *time; /* NULL: no clock */
    bool clock_24h;
    int battery;           /* percent, -1 unknown */
    bool battery_visible;  /* false while blinking off (low battery) */
    bool charging;
    int bluetooth;         /* 0 off, 1 on but not connected, 2 connected */
};
/* Draws the bar into the current viewport (UI_STATUS_H high) */
void ui_status_draw(const struct ui_status *st);

/* --- time (ui_time.c) --- */

/* Free-running microsecond counter for measuring frame times */
uint32_t ui_time_us(void);

/* --- gallery (ui_gallery.c) --- */

/* Debug menu: shows the UI's building blocks and measures drawing speed.
 * Returns true if USB was connected. */
bool ui_gallery(void);

#endif /* NEWUI_UI_H */
