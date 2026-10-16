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
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
 * ANY KIND, either express or implied.
 *
 ****************************************************************************/

/* Slide transitions between list levels (PLAN.md §2 Motion): entering a
 * menu or folder slides the new list in from the right, going back from
 * the left. ui_transition() keeps a copy of the screen below the status
 * bar before the level changes; the next full-area list draw slides from
 * that copy to what it drew. */

#include "config.h"
#include <string.h>
#include "kernel.h"
#include "lcd.h"
#include "ui.h"

#define AREA_Y      UI_STATUS_H
#define AREA_H      (LCD_HEIGHT - UI_STATUS_H)
#define FRAMES      6
#define MAX_WAIT    (HZ / 2)    /* the next draw must come soon after */

static fb_data old_px[LCD_WIDTH * AREA_H];
static fb_data new_px[LCD_WIDTH * AREA_H];
static int pending;             /* direction, 0: none */
static long pending_tick;

static void copy_area(fb_data *dst, bool from_screen)
{
    struct viewport *last = lcd_set_viewport(NULL);
    for (int y = 0; y < AREA_H; y++)
    {
        fb_data *row = FBADDR(0, AREA_Y + y);
        if (from_screen)
            memcpy(dst + y * LCD_WIDTH, row, LCD_WIDTH * sizeof(fb_data));
        else
            memcpy(row, dst + y * LCD_WIDTH, LCD_WIDTH * sizeof(fb_data));
    }
    lcd_set_viewport(last);
}

void ui_transition(int dir)
{
    copy_area(old_px, true);
    pending = dir;
    pending_tick = current_tick;
}

bool ui_transition_run(int x, int y, int w, int h)
{
    int dir = pending;
    pending = 0;
    if (!dir || TIME_AFTER(current_tick, pending_tick + MAX_WAIT) ||
        x != 0 || y != AREA_Y || w != LCD_WIDTH || h != AREA_H)
        return false;

    copy_area(new_px, true);
    struct viewport *last = lcd_set_viewport(NULL);
    for (int f = 1; f < FRAMES; f++)
    {
        /* ease out: fast, then settling */
        int t = 256 - ((FRAMES - f) * (FRAMES - f) * 256) / (FRAMES * FRAMES);
        int off = LCD_WIDTH * t / 256;
        for (int r = 0; r < AREA_H; r++)
        {
            fb_data *dst = FBADDR(0, AREA_Y + r);
            const fb_data *o = old_px + r * LCD_WIDTH;
            const fb_data *n = new_px + r * LCD_WIDTH;
            if (dir > 0)
            {   /* old moves left, new comes from the right */
                memcpy(dst, o + off, (LCD_WIDTH - off) * sizeof(fb_data));
                memcpy(dst + LCD_WIDTH - off, n, off * sizeof(fb_data));
            }
            else
            {   /* old moves right, new comes from the left */
                memcpy(dst, n + LCD_WIDTH - off, off * sizeof(fb_data));
                memcpy(dst + off, o, (LCD_WIDTH - off) * sizeof(fb_data));
            }
        }
        lcd_update_rect(0, AREA_Y, LCD_WIDTH, AREA_H);
    }
    copy_area(new_px, false);
    lcd_update_rect(0, AREA_Y, LCD_WIDTH, AREA_H);
    lcd_set_viewport(last);
    return true;
}
