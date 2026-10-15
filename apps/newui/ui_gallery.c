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

/* UI gallery (Debug menu > New UI gallery): one page per building block,
 * shown with sample data, plus a drawing speed test. Prev/Next or the wheel
 * change page, Back leaves. */

#include "config.h"
#include <stdio.h>
#include "action.h"
#include "kernel.h"
#include "lcd.h"
#include "font.h"
#include "misc.h"
#include "viewport.h"
#include "screens.h"
#include "status.h"
#include "timefuncs.h"
#include "ui.h"

#define UTF8_EMDASH   "\xe2\x80\x94"
#define UTF8_ELLIPSIS "\xe2\x80\xa6"
#define UTF8_LQUOTE   "\xe2\x80\x9c"
#define UTF8_RQUOTE   "\xe2\x80\x9d"

static void title(const char *text, int page, int pages)
{
    char buf[16];
    lcd_setfont(ui_font(UI_FONT_SMALL));
    lcd_set_foreground(UI_COL_TEXT2);
    lcd_putsxy(UI_MARGIN, 4, text);
    snprintf(buf, sizeof(buf), "%d/%d", page + 1, pages);
    int w, h;
    font_getstringsize(buf, &w, &h, ui_font(UI_FONT_SMALL));
    lcd_putsxy(LCD_WIDTH - UI_MARGIN - w, 4, buf);
}

static void page_fonts(void)
{
    static const struct { enum ui_font f; const char *name; } fonts[] = {
        { UI_FONT_SMALL, "Small 13 " UTF8_EMDASH " 0:15  -6:05  3 of 12" },
        { UI_FONT_BODY,  "Body 17 " UTF8_EMDASH " Pink Floyd, Bj\xc3\xb6rk" },
        { UI_FONT_ROW,   "Row 17 " UTF8_EMDASH " Sound Settings" },
        { UI_FONT_TITLE, "Title 20 " UTF8_EMDASH " Money" },
    };
    int y = UI_STATUS_H + 4;
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
    {
        int w, h;
        lcd_setfont(ui_font(fonts[i].f));
        lcd_set_foreground(UI_COL_TEXT);
        font_getstringsize(fonts[i].name, &w, &h, ui_font(fonts[i].f));
        lcd_putsxy(UI_MARGIN, y, fonts[i].name);
        y += h + 6;
    }

    lcd_setfont(ui_font(UI_FONT_BODY));
    lcd_set_foreground(UI_COL_TEXT2);
    lcd_putsxy(UI_MARGIN, y, UTF8_LQUOTE "Wish You Were Here" UTF8_RQUOTE UTF8_ELLIPSIS);
    y += 24;
    lcd_set_foreground(UI_COL_MUTED);
    lcd_putsxy(UI_MARGIN, y, "\xce\x91\xce\xb8\xce\xae\xce\xbd\xce\xb1  \xd0\x9c\xd0\xbe\xd1\x81\xd0\xba\xd0\xb2\xd0\xb0");
    y += 26;

    lcd_setfont(ui_font(UI_FONT_BIG));
    lcd_set_foreground(UI_COL_TEXT);
    lcd_putsxy(UI_MARGIN, y, "A 42");

    /* colour swatches */
    static const unsigned cols[] = {
        UI_COL_TEXT, UI_COL_TEXT2, UI_COL_MUTED, UI_COL_SEL,
        UI_COL_SEL_TEXT, UI_COL_TRACK, UI_COL_ACCENT,
    };
    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++)
    {
        lcd_set_foreground(cols[i]);
        lcd_fillrect(150 + i * 22, y + 8, 18, 30);
    }
    lcd_set_foreground(UI_COL_TEXT2);
    lcd_drawrect(150 + 5 * 22, y + 8, 18, 30); /* outline the near-black swatch */
}

static void page_shapes(void)
{
    int f = ui_font(UI_FONT_BODY);

    /* Minimal selection card and Classic accent bar, as list rows */
    int y = UI_STATUS_H + 2;
    ui_fill_round_rect(6, y + 1, LCD_WIDTH - 12, UI_ROW_H - 2, 6, UI_COL_SEL);
    ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, f, UI_COL_SEL_TEXT, "Minimal selection", 0, -1);
    y += UI_ROW_H;
    ui_fill_rect(0, y, LCD_WIDTH, UI_ROW_H, UI_COL_ACCENT);
    ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, f, UI_COL_TEXT, "Classic selection", 0, -1);
    y += UI_ROW_H;
    ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, f, UI_COL_TEXT,
                    "A very long album title that will not fit on one row of the list",
                    LCD_WIDTH - 2 * UI_MARGIN, -1);
    y += UI_ROW_H;
    ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, f, UI_COL_TEXT, "Value on the right", 0, -1);
    ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, f, UI_COL_TEXT2, "On",
                    LCD_WIDTH - 2 * UI_MARGIN, 1);
    y += UI_ROW_H + 6;

    /* shapes: art placeholder, outline, play glyph, battery */
    ui_fill_round_rect(UI_MARGIN, y, 64, 64, 10, UI_COL_TRACK);
    ui_stroke_round_rect(UI_MARGIN + 76, y, 64, 64, 10, 2, UI_COL_TEXT2);
    ui_fill_triangle_right(UI_MARGIN + 160, y + 16, 28, 32, UI_COL_TEXT);
    ui_fill_rect(UI_MARGIN + 196, y + 16, 9, 32, UI_COL_TEXT);
    ui_fill_rect(UI_MARGIN + 212, y + 16, 9, 32, UI_COL_TEXT);
    ui_stroke_round_rect(UI_MARGIN + 234, y + 20, 44, 22, 5, 2, UI_COL_TEXT);
    ui_fill_round_rect(UI_MARGIN + 278, y + 26, 4, 10, 2, UI_COL_TEXT);
    ui_fill_round_rect(UI_MARGIN + 238, y + 24, 26, 14, 2, UI_COL_TEXT);

    /* progress line with times */
    y += 74;
    ui_fill_rect(UI_MARGIN, y, LCD_WIDTH - 2 * UI_MARGIN, 4, UI_COL_TRACK);
    ui_fill_rect(UI_MARGIN, y, 110, 4, UI_COL_SEL);
}

static void page_status(void)
{
    static struct tm t = { .tm_hour = 14, .tm_min = 41, .tm_year = 126,
                           .tm_mon = 9, .tm_mday = 15 };
    struct ui_status st[] = {
        { .playmode = STATUS_PLAY,  .time = &t, .clock_24h = true,
          .battery = 80, .battery_visible = true, .bluetooth = 2 },
        { .playmode = STATUS_PAUSE, .time = &t, .clock_24h = false,
          .battery = 45, .battery_visible = true, .charging = true },
        { .playmode = STATUS_FASTFORWARD, .keylock = true, .time = &t,
          .clock_24h = false, .battery = 9, .battery_visible = true,
          .bluetooth = 1 },
        { .playmode = STATUS_STOP, .time = NULL, .battery = -1,
          .battery_visible = true },
        { .playmode = STATUS_FASTBACKWARD, .time = &t, .clock_24h = true,
          .battery = 100, .battery_visible = true },
    };
    static const char * const notes[] = {
        "Playing, 24 h, Bluetooth connected",
        "Paused, 12 h, charging",
        "Seeking, locked, low battery, Bluetooth on",
        "Stopped, no clock, battery unknown",
        "Rewinding, full battery",
    };
    for (int i = 0; i < 5; i++)
    {
        struct viewport vp;
        viewport_set_defaults(&vp, SCREEN_MAIN);
        vp.x = 0; vp.y = UI_STATUS_H + 4 + i * 42;
        vp.width = LCD_WIDTH; vp.height = UI_STATUS_H;
        struct viewport *last = lcd_set_viewport(&vp);
        ui_status_draw(&st[i]);
        lcd_set_viewport(last);
        ui_text(UI_MARGIN, vp.y + UI_STATUS_H, ui_font(UI_FONT_SMALL),
                UI_COL_MUTED, notes[i], 0, -1);
    }
}

/* Average time of one frame of 'draw' followed by a full lcd_update */
static uint32_t time_frames(void (*draw)(int), bool update, int frames)
{
    uint32_t start = ui_time_us();
    for (int i = 0; i < frames; i++)
    {
        draw(i);
        if (update)
            lcd_update();
    }
    return (ui_time_us() - start) / frames;
}

static void draw_fill(int i)
{
    lcd_set_background((i & 1) ? UI_COL_TRACK : UI_COL_BG);
    lcd_clear_display();
}

static void draw_text_page(int i)
{
    lcd_set_background(UI_COL_BG);
    lcd_clear_display();
    lcd_setfont(ui_font(UI_FONT_BODY));
    for (int r = 0; r < 8; r++)
    {
        lcd_set_foreground(r == (i & 7) ? UI_COL_SEL_TEXT : UI_COL_TEXT);
        if (r == (i & 7))
        {
            lcd_set_foreground(UI_COL_SEL);
            lcd_fillrect(6, UI_STATUS_H + r * UI_ROW_H, LCD_WIDTH - 12, UI_ROW_H);
            lcd_set_foreground(UI_COL_SEL_TEXT);
            lcd_set_background(UI_COL_SEL);
        }
        lcd_putsxy(UI_MARGIN, UI_STATUS_H + r * UI_ROW_H + 4,
                   "The Dark Side of the Moon " UTF8_EMDASH " Pink Floyd");
        lcd_set_background(UI_COL_BG);
    }
}

static void page_speed(void)
{
    enum { FRAMES = 60 };
    char buf[64];
    struct { const char *label; uint32_t us; } r[3];

    lcd_setfont(ui_font(UI_FONT_BODY));
    lcd_set_foreground(UI_COL_TEXT);
    lcd_putsxy(UI_MARGIN, UI_STATUS_H + 10, "Measuring" UTF8_ELLIPSIS);
    lcd_update();

    r[0].label = "Clear only";
    r[0].us = time_frames(draw_fill, false, FRAMES);
    r[1].label = "Clear + update";
    r[1].us = time_frames(draw_fill, true, FRAMES);
    r[2].label = "8 text rows + update";
    r[2].us = time_frames(draw_text_page, true, FRAMES);

    lcd_set_background(UI_COL_BG);
    lcd_clear_display();
    title("Drawing speed (full screen)", 3, 4);
    int y = UI_STATUS_H + 8;
    for (int i = 0; i < 3; i++)
    {
        lcd_setfont(ui_font(UI_FONT_BODY));
        lcd_set_foreground(UI_COL_TEXT);
        lcd_putsxy(UI_MARGIN, y, r[i].label);
        snprintf(buf, sizeof(buf), "%lu.%lu ms  %lu fps",
                 (unsigned long)r[i].us / 1000, (unsigned long)(r[i].us % 1000) / 100,
                 r[i].us ? 1000000ul / r[i].us : 0ul);
        lcd_set_foreground(UI_COL_TEXT2);
        lcd_putsxy(UI_MARGIN, y + 20, buf);
        y += 48;
    }
    lcd_setfont(ui_font(UI_FONT_SMALL));
    lcd_set_foreground(UI_COL_MUTED);
    lcd_putsxy(UI_MARGIN, LCD_HEIGHT - 18, "Play: run again");
}

bool ui_gallery(void)
{
    enum { PAGES = 4 };
    int page = 0;
    bool usb = false, redraw = true;

    ui_font_init();
    FOR_NB_SCREENS(i)
        viewportmanager_theme_enable(i, false, NULL);
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);

    while (!usb)
    {
        if (redraw)
        {
            lcd_set_background(UI_COL_BG);
            lcd_clear_display();
            if (page == 0)
            {
                title("Fonts and colours", 0, PAGES);
                page_fonts();
            }
            else if (page == 1)
            {
                title("Shapes and rows", 1, PAGES);
                page_shapes();
            }
            else if (page == 2)
            {
                title("Status bar", 2, PAGES);
                page_status();
            }
            else
            {
                page_speed();
            }
            lcd_update();
            redraw = false;
        }

        int action = get_action(CONTEXT_STD, HZ / 2);
        switch (action)
        {
            case ACTION_STD_NEXT:
                page = (page + 1) % PAGES;
                redraw = true;
                break;
            case ACTION_STD_PREV:
                page = (page + PAGES - 1) % PAGES;
                redraw = true;
                break;
            case ACTION_STD_OK:
                redraw = true;
                break;
            case ACTION_STD_CANCEL:
                goto out;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                    usb = true;
                break;
        }
    }
out:
    lcd_setfont(FONT_UI);
    FOR_NB_SCREENS(i)
        viewportmanager_theme_undo(i, true);
    return usb;
}
