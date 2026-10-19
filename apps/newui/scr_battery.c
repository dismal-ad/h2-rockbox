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

/* Battery usage: a log of the battery level against the running time since
 * the last charge, a point each time the level enters a new 5% step, and
 * System > Battery Usage to see it as a chart.
 *
 * The log is checked every LOG_EVERY (on the UI's action updates, so it
 * costs nothing in between), kept in RAM and in LOG_FILE so it survives
 * restarts. Nothing is logged while charging; the running time resets when
 * the charger is unplugged (global_status.runtime), which starts a new log. */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "action.h"
#include "appevents.h"
#include "dir.h"
#include "file.h"
#include "font.h"
#include "kernel.h"
#include "lang.h"
#include "language.h"
#include "lcd.h"
#include "misc.h"
#include "power.h"
#include "powermgmt.h"
#include "rbpaths.h"
#include "settings.h"
#include "viewport.h"
#include "ui.h"

#define LOG_FILE    ROCKBOX_DIR "/newui/battery.txt"
#define LOG_POINTS  64
#define LOG_STEP    5               /* percent */
#define LOG_EVERY   (30 * HZ)

struct point {
    int min;                        /* running minutes since the last charge */
    int pct;
};
static struct point pts[LOG_POINTS];
static int npts;
static long next_check;

static void save(void)
{
    if (!dir_exists(ROCKBOX_DIR "/newui"))
        mkdir(ROCKBOX_DIR "/newui");
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    for (int i = 0; i < npts; i++)
        fdprintf(fd, "%d %d\n", pts[i].min, pts[i].pct);
    close(fd);
}

static void load(void)
{
    char line[32];
    npts = 0;
    int fd = open(LOG_FILE, O_RDONLY);
    if (fd < 0)
        return;
    while (npts < LOG_POINTS && read_line(fd, line, sizeof(line)) > 0)
    {
        char *end;
        int m = strtol(line, &end, 10), p = strtol(end, NULL, 10);
        if (end != line && p >= 0 && p <= 100)
        {
            pts[npts].min = m;
            pts[npts].pct = p;
            npts++;
        }
    }
    close(fd);
}

static void add(int min, int pct)
{
    if (npts == LOG_POINTS)
    {
        memmove(pts, pts + 1, (LOG_POINTS - 1) * sizeof(pts[0]));
        npts--;
    }
    pts[npts].min = min;
    pts[npts].pct = pct;
    npts++;
    save();
}

static void check(void)
{
    int pct = battery_level();
    if (pct < 0 || charger_inserted())
        return;
    update_runtime();
    int min = global_status.runtime / 60;

    /* the running time went back: charged since, a new log */
    if (npts && min < pts[npts - 1].min)
        npts = 0;
    if (!npts || pct / LOG_STEP != pts[npts - 1].pct / LOG_STEP)
        add(min, pct);
}

static void on_action(unsigned short id, void *data)
{
    (void)id;
    (void)data;
    if (TIME_AFTER(current_tick, next_check))
    {
        next_check = current_tick + LOG_EVERY;
        check();
    }
}

void ui_battery_log_init(void)
{
    load();
    next_check = current_tick + LOG_EVERY;
    add_event(GUI_EVENT_ACTIONUPDATE, on_action);
}

/* --- the chart --- */

#define CH_X0   38                  /* the plot area */
#define CH_X1   (LCD_WIDTH - 14)
#define CH_Y0   48
#define CH_Y1   (LCD_HEIGHT - UI_STATUS_H - 30)

static inline unsigned mix(unsigned a, unsigned b, int t) /* t/256 of b */
{
    return LCD_RGBPACK(
        RGB_UNPACK_RED(a) + ((RGB_UNPACK_RED(b) - RGB_UNPACK_RED(a)) * t >> 8),
        RGB_UNPACK_GREEN(a) + ((RGB_UNPACK_GREEN(b) - RGB_UNPACK_GREEN(a)) * t >> 8),
        RGB_UNPACK_BLUE(a) + ((RGB_UNPACK_BLUE(b) - RGB_UNPACK_BLUE(a)) * t >> 8));
}

/* The level at minute m (x 256), a smooth curve through the points that
 * does not overshoot them (monotone cubic, Fritsch-Carlson) */
static int level_at(int m256)
{
    if (npts == 1 || m256 <= pts[0].min * 256)
        return pts[0].pct * 256;
    if (m256 >= pts[npts - 1].min * 256)
        return pts[npts - 1].pct * 256;
    int i = 0;
    while (i < npts - 2 && m256 > pts[i + 1].min * 256)
        i++;
    /* slopes (pct per minute, x256) at the two ends of segment i */
    long h = pts[i + 1].min - pts[i].min;
    long d = h ? (pts[i + 1].pct - pts[i].pct) * 256 / h : 0;
    long s[2];
    for (int e = 0; e < 2; e++)
    {
        int k = i + e;
        long dl = k > 0 && pts[k].min > pts[k - 1].min ?
            (pts[k].pct - pts[k - 1].pct) * 256 / (pts[k].min - pts[k - 1].min) : d;
        long dr = k < npts - 1 && pts[k + 1].min > pts[k].min ?
            (pts[k + 1].pct - pts[k].pct) * 256 / (pts[k + 1].min - pts[k].min) : d;
        s[e] = (dl > 0) != (dr > 0) || !dl || !dr ? 0 : 2 * dl * dr / (dl + dr);
    }
    if (!h)
        return pts[i].pct * 256;
    long t = (m256 - pts[i].min * 256) / h;          /* 0..256 */
    long t2 = t * t >> 8, t3 = t2 * t >> 8;
    long h00 = 2 * t3 - 3 * t2 + 256, h10 = t3 - 2 * t2 + t;
    long h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
    return (int)((h00 * pts[i].pct * 256 + h10 * h * s[0] +
                  h01 * pts[i + 1].pct * 256 + h11 * h * s[1]) >> 8);
}

static void draw(struct viewport *vp)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int small = ui_font(UI_FONT_SMALL), row = ui_font(UI_FONT_ROW);
    const int title = ui_font(UI_FONT_TITLE);
    const unsigned acc = ui_accent(), bg = UI_COL_BG;
    char buf[48];

    ui_status_title(str(LANG_NEWUI_BATTERY_USAGE));
    ui_fill_rect(0, 0, vp->width, vp->height, bg);

    /* now: the level, and how long since the last charge */
    int pct = battery_level();
    update_runtime();
    int mins = global_status.runtime / 60;
    snprintf(buf, sizeof(buf), "%d%%", MAX(pct, 0));
    ui_text(UI_MARGIN, 8, title, UI_COL_TEXT, buf, 0, -1);
    if (mins >= 60)
        snprintf(buf, sizeof(buf), "%dh%02dm since charge", mins / 60,
                 mins % 60);
    else
        snprintf(buf, sizeof(buf), "%dm since charge", mins);
    ui_text_vcenter(UI_MARGIN, 8, font_get(title)->height, row, UI_COL_TEXT2,
                    buf, vp->width - 2 * UI_MARGIN, 1);

    /* grid: dashed lines at 0, 25, 50, 75, 100% */
    const int ph = CH_Y1 - CH_Y0, pw = CH_X1 - CH_X0;
    for (int g = 0; g <= 4; g++)
    {
        int y = CH_Y1 - ph * g / 4;
        for (int x = CH_X0; x < CH_X1; x += 6)
            ui_fill_rect(x, y, 3, 1, UI_COL_TRACK);
        snprintf(buf, sizeof(buf), "%d", g * 25);
        ui_text(0, y - font_get(small)->height / 2, small, UI_COL_MUTED, buf,
                CH_X0 - 6, 1);
    }

    if (npts == 0)
    {
        ui_text_vcenter(CH_X0, CH_Y0, ph, row, UI_COL_TEXT2,
                        str(LANG_NEWUI_BATTERY_NO_DATA), pw, 0);
        lcd_update_viewport();
        lcd_set_viewport(last);
        return;
    }

    /* x axis: minutes, from the first point to the last (or now) */
    int m0 = pts[0].min, m1 = MAX(pts[npts - 1].min, m0 + 10);
    for (int g = 0; g <= 3; g++)
    {
        int m = m0 + (m1 - m0) * g / 3, x = CH_X0 + pw * g / 3;
        snprintf(buf, sizeof(buf), g == 3 ? "%d min" : "%d", m);
        int w = ui_text_width(small, buf);
        ui_text(g == 0 ? x : g == 3 ? x - w : x - w / 2, CH_Y1 + 6, small,
                UI_COL_MUTED, buf, 0, -1);
    }

    /* the curve: a gradient fill under it, then a 3 px line with smooth
       edges */
    int prev = -1;
    int last_x = npts > 1 ? CH_X1 : CH_X0;
    for (int x = CH_X0; x <= last_x; x++)
    {
        int m256 = npts > 1 ? (int)(((long)(x - CH_X0) * (m1 - m0) * 256) / pw) +
                              m0 * 256 : pts[0].min * 256;
        int y256 = CH_Y1 * 256 - (int)((long)level_at(m256) * ph / 100);
        int yi = y256 >> 8;
        for (int y = yi + 1; y < CH_Y1; y++)
        {
            int a = 120 - 100 * (y - yi) / MAX(CH_Y1 - yi, 1);
            ui_fill_rect(x, y, 1, 1, mix(bg, acc, a));
        }
        /* the line: from the last column's height to this one's, 3 px */
        int top = y256 - 384, bot = y256 + 384;
        if (prev >= 0)
        {
            top = MIN(top, prev - 384);
            bot = MAX(bot, prev + 384);
        }
        for (int y = top >> 8; y <= bot >> 8; y++)
        {
            int p0 = MAX(y * 256, top), p1 = MIN(y * 256 + 256, bot);
            if (p1 > p0)
                ui_blend_pixel(x, y, acc, p1 - p0);
        }
        prev = y256;
    }
    /* a dot on the latest point */
    {
        int x = npts > 1 ? CH_X1 : CH_X0;
        int y = CH_Y1 - ph * pts[npts - 1].pct / 100;
        ui_fill_round_rect(x - 4, y - 4, 9, 9, 4, acc);
        ui_fill_round_rect(x - 2, y - 2, 5, 5, 2, bg);
    }
    lcd_update_viewport();
    lcd_set_viewport(last);
}

int ui_battery_usage(void)
{
    struct viewport vp;
    viewport_set_defaults(&vp, SCREEN_MAIN);
    check();                            /* the latest level, if a new step */
    draw(&vp);
    while (true)
    {
        int action = get_action(CONTEXT_STD, HZ * 10);
        if (action == ACTION_STD_CANCEL || action == ACTION_STD_MENU)
            return 0;
        if (action == ACTION_NONE)
            draw(&vp);                  /* now's level and time */
        else if (default_event_handler(action) == SYS_USB_CONNECTED)
            return 1;
    }
}
