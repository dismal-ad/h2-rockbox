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

#define HDR_Y     4                 /* the two lines, over the chart's top */
#define HDR_ROW_H 22                /* right: the curve has fallen by then */
#define CH_X0   38                  /* the plot area: all of the screen */
#define CH_X1   (LCD_WIDTH - 14)
#define CH_Y0   12
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

/* "label  value", right justified: the value in the title font ending at
   the right margin, the label in body text ending value_w + a gap before
   it (both rows' labels end in one column). Drawn over the chart on a
   background box, so grid lines don't run through the text. */
static void header_row(int y, int value_w, const char *label, const char *value)
{
    const int right = LCD_WIDTH - UI_MARGIN, label_r = right - value_w - 8;
    int left = label_r - ui_text_width(ui_font(UI_FONT_BODY), label) - 4;
    ui_fill_rect(left, y + 2, right + 2 - left, HDR_ROW_H - 4, UI_COL_BG);
    ui_text_vcenter(UI_MARGIN, y, HDR_ROW_H, ui_font(UI_FONT_BODY),
                    UI_COL_TEXT2, label, label_r - UI_MARGIN, 1);
    ui_text_vcenter(UI_MARGIN, y, HDR_ROW_H, ui_font(UI_FONT_TITLE),
                    UI_COL_TEXT, value, right - UI_MARGIN, 1);
}

/* now: how long since the last charge, and the level */
struct header {
    char since[16], level[16];
    int value_w;                    /* the wider value */
};

static void header_text(struct header *h)
{
    int pct = battery_level();
    update_runtime();
    int mins = global_status.runtime / 60;
    if (mins >= 60)
        snprintf(h->since, sizeof(h->since), "%dh %dm", mins / 60, mins % 60);
    else
        snprintf(h->since, sizeof(h->since), "%dm", mins);
    snprintf(h->level, sizeof(h->level), "%d%%", MAX(pct, 0));
    const int title = ui_font(UI_FONT_TITLE);
    h->value_w = MAX(ui_text_width(title, h->since),
                     ui_text_width(title, h->level));
}

/* where the header's text starts (its left edge) */
static int header_left(const struct header *h)
{
    const int body = ui_font(UI_FONT_BODY);
    int lw = MAX(ui_text_width(body, str(LANG_NEWUI_SINCE_CHARGE)),
                 ui_text_width(body, str(LANG_NEWUI_BATTERY_LEVEL)));
    return LCD_WIDTH - UI_MARGIN - h->value_w - 8 - lw - 4;
}

static void draw_header(const struct header *h)
{
    header_row(HDR_Y, h->value_w, str(LANG_NEWUI_SINCE_CHARGE), h->since);
    header_row(HDR_Y + HDR_ROW_H, h->value_w, str(LANG_NEWUI_BATTERY_LEVEL),
               h->level);
}

static void draw(struct viewport *vp)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int small = ui_font(UI_FONT_SMALL), row = ui_font(UI_FONT_ROW);
    const unsigned acc = ui_accent(), bg = UI_COL_BG;
    const int pw = CH_X1 - CH_X0;
    char buf[48];
    struct header h;

    ui_status_title(str(LANG_NEWUI_BATTERY_USAGE));
    ui_fill_rect(0, 0, vp->width, vp->height, bg);
    header_text(&h);

    /* x axis: whole hours, at a step that leaves at most five labels; the
       axis ends on a label, so the curve may stop short of it */
    static const unsigned char steps[] = { 1, 2, 3, 4, 6, 12, 24, 48 };
    int h0 = 0, step = 1, ticks = 1, m0 = 0, m1 = 60;
    if (npts)
    {
        h0 = pts[0].min / 60;
        int span = MAX((pts[npts - 1].min + 59) / 60 - h0, 1);
        step = steps[sizeof(steps) - 1];
        for (unsigned i = 0; i < sizeof(steps); i++)
            if (span <= 4 * steps[i])
            {
                step = steps[i];
                break;
            }
        ticks = (span + step - 1) / step;
        m0 = h0 * 60;
        m1 = (h0 + ticks * step) * 60;
    }
#define X_OF(m) (CH_X0 + (int)((long)((m) - m0) * pw / (m1 - m0)))
#define M256_OF(x) ((int)(((long)((x) - CH_X0) * (m1 - m0) * 256) / pw) + m0 * 256)

    /* The chart takes the whole screen, the header over its top right,
       where a falling curve has gone below it. Early in a discharge the
       curve is still up there: then the chart starts under the header. */
    int top = CH_Y0;
    if (npts)
    {
        const int hb = HDR_Y + 2 * HDR_ROW_H, ph = CH_Y1 - top;
        for (int x = MAX(header_left(&h), X_OF(pts[0].min));
             x <= X_OF(pts[npts - 1].min); x += 2)
            if (CH_Y1 - (int)((long)level_at(M256_OF(x)) * ph / 25600) - 4 < hb)
            {
                top = hb + 10;
                break;
            }
    }
    const int ph = CH_Y1 - top;

    /* grid: dashed lines at 0, 25, 50, 75, 100% */
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
        ui_text_vcenter(CH_X0, top, ph, row, UI_COL_TEXT2,
                        str(LANG_NEWUI_BATTERY_NO_DATA), pw, 0);
    else
    {
        for (int g = 0; g <= ticks; g++)
        {
            int x = CH_X0 + pw * g / ticks;
            snprintf(buf, sizeof(buf), "%dh", h0 + g * step);
            int w = ui_text_width(small, buf);
            ui_text(g == 0 ? x : g == ticks ? x - w : x - w / 2, CH_Y1 + 6,
                    small, UI_COL_MUTED, buf, 0, -1);
        }

        /* the curve, from the first reading to the latest: a gradient fill
           under it, then a 3 px line with smooth edges */
        int prev = -1;
        for (int x = X_OF(pts[0].min); x <= X_OF(pts[npts - 1].min); x++)
        {
            int y256 = CH_Y1 * 256 - (int)((long)level_at(M256_OF(x)) * ph / 100);
            int yi = y256 >> 8;
            for (int y = yi + 1; y < CH_Y1; y++)
            {
                int a = 120 - 100 * (y - yi) / MAX(CH_Y1 - yi, 1);
                ui_fill_rect(x, y, 1, 1, mix(bg, acc, a));
            }
            /* the line: from the last column's height to this one's, 3 px */
            int t = y256 - 384, b = y256 + 384;
            if (prev >= 0)
            {
                t = MIN(t, prev - 384);
                b = MAX(b, prev + 384);
            }
            for (int y = t >> 8; y <= b >> 8; y++)
            {
                int p0 = MAX(y * 256, t), p1 = MIN(y * 256 + 256, b);
                if (p1 > p0)
                    ui_blend_pixel(x, y, acc, p1 - p0);
            }
            prev = y256;
        }
        /* a dot on the latest point */
        int x = X_OF(pts[npts - 1].min);
        int y = CH_Y1 - ph * pts[npts - 1].pct / 100;
        ui_fill_round_rect(x - 4, y - 4, 9, 9, 4, acc);
        ui_fill_round_rect(x - 2, y - 2, 5, 5, 2, bg);
    }
#undef X_OF
#undef M256_OF
    draw_header(&h);
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
