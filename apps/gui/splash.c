/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) Daniel Stenberg (2002)
 * Copyright (C) William Wilgus (2026)
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
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "kernel.h"
#include "lang.h"
#include "splash.h"
#include "settings.h" /* P2STR */
#include "viewport.h"
#include "font.h"
#include "misc.h" /* get_current_activity */
#include "newui/ui.h"

/* Splashes are the new UI's pop-up panel (PLAN.md §4.8): bold text,
 * centred and word wrapped, on a rounded panel over the current screen;
 * splash_progress adds a bar under the text.
 *
 * '\n' and '\t' break lines; '\f' first resets the panel's size (it does
 * not shrink while one activity shows splashes, so a shorter text does
 * not leave the edges of a longer one); '\r', '\v' and '\b' are spaces. */

#define MAXBUFFER     1024
#define PANEL_VPAD    14
#define BAR_H         6
#define BAR_SPACE     (BAR_H + 12)

static long progress_next_tick;

/* progress: 0..1000, or -1 for none */
static void splash_draw(const char *fmt, va_list ap, int progress)
{
    static int keep_w, keep_h;
    static enum current_activity last_act = ACTIVITY_UNKNOWN;
    static char buf[MAXBUFFER];
    static struct ui_lines lines;

    enum current_activity act = get_current_activity();
    if (act != last_act)
    {
        keep_w = keep_h = 0;
        last_act = act;
    }

    if (vsnprintf(buf, sizeof(buf), fmt, ap) <= 0)
        return;
    char *text = buf;
    if (*text == '\f')
    {
        keep_w = keep_h = 0;
        text++;
    }
    for (char *c = text; *c; c++)
        if (*c == '\f' || *c == '\r' || *c == '\v' || *c == '\b')
            *c = ' ';

    struct viewport vp;
    viewport_set_defaults(&vp, SCREEN_MAIN);
    struct viewport *last = lcd_set_viewport(&vp);

    const int font = ui_font(UI_FONT_ROW);
    const int lh = font_get(font)->height;
    const int max_w = vp.width - 2 * UI_MARGIN;
    lines.count = lines.widest = 0;
    ui_wrap(&lines, text, font, max_w - 2 * UI_POPUP_PAD);
    while (lines.count > 1 && !lines.line[lines.count - 1][0])
        lines.count--;

    const int bar = progress >= 0 ? BAR_SPACE : 0;
    int w = MIN(MAX(lines.widest, 120) + 2 * UI_POPUP_PAD, max_w);
    int h = MIN(lines.count * lh + 2 * PANEL_VPAD + bar, vp.height);
    w = keep_w = MAX(w, keep_w);
    h = keep_h = MAX(h, keep_h);
    int x = (vp.width - w) / 2, y = (vp.height - h) / 2;

    lcd_scroll_stop();
    ui_panel(x, y, w, h);
    int ty = y + (h - bar - lines.count * lh) / 2;
    for (int i = 0; i < lines.count && ty + lh <= y + h; i++, ty += lh)
        ui_text(x + UI_POPUP_PAD, ty, font, UI_COL_TEXT, lines.line[i],
                w - 2 * UI_POPUP_PAD, 0);
    if (progress >= 0)
    {
        int bw = w - 2 * UI_POPUP_PAD, by = y + h - PANEL_VPAD - BAR_H;
        int fill = bw * MIN(progress, 1000) / 1000;
        ui_fill_round_rect(x + UI_POPUP_PAD, by, bw, BAR_H, BAR_H / 2,
                           UI_COL_TRACK);
        if (fill > 0)
            ui_fill_round_rect(x + UI_POPUP_PAD, by, MAX(fill, BAR_H), BAR_H,
                               BAR_H / 2, UI_COL_FILL);
    }
    lcd_update_viewport();
    lcd_set_viewport(last);
}

void splashf(int ticks, const char *fmt, ...)
{
    va_list ap;

    /* If fmt is a lang ID then get the corresponding string (which
       still might contain % place holders). */
    fmt = P2STR((unsigned char *)fmt);
    va_start(ap, fmt);
    splash_draw(fmt, ap, -1);
    va_end(ap);
    if (ticks)
        sleep(ticks);
}

/* set delay before progress meter is shown */
void splash_progress_set_delay(long delay_ticks)
{
    progress_next_tick = current_tick + delay_ticks;
}

/* splash a progress meter */
void splash_progress(int current, int total, const char *fmt, ...)
{
    va_list ap;
    long now = current_tick;

    if (current < total)
    {
        if (TIME_BEFORE(now, progress_next_tick))
            return;
        /* limit to 20fps */
        progress_next_tick = now + HZ/20;
    }

    fmt = P2STR((unsigned char *)fmt);
    va_start(ap, fmt);
    splash_draw(fmt, ap, total > 0 ? (int)((long long)current * 1000 / total) : 0);
    va_end(ap);
}
