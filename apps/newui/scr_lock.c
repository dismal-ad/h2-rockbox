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

/* The lock screen (PLAN.md §4.8), shown while the keys are locked:
 *
 *                 12:41
 *            Thu 15 Oct
 *   [ cover ]  Title
 *              Artist
 *   Press Power or double-press Centre to unlock
 *
 * Called from action.c's softlock handling, so it reads buttons directly
 * rather than through get_action(). */

#include "config.h"
#include <stdio.h>
#include "action.h"
#include "appevents.h"
#include "audio.h"
#include "button.h"
#include "font.h"
#include "kernel.h"
#include "lang.h"
#include "language.h"
#include "lcd.h"
#include "metadata.h"
#include "settings.h"
#include "timefuncs.h"
#include "viewport.h"
#include "ui.h"

#define LOCK_ART    96
#define DOUBLE_TAP  (HZ / 2)

static void draw(struct viewport *vp)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int big = ui_font(UI_FONT_BIG), body = ui_font(UI_FONT_BODY);
    const int small = ui_font(UI_FONT_SMALL);
    char buf[64];
    int y = 4;

    ui_fill_rect(0, 0, vp->width, vp->height, UI_COL_BG);

    struct tm *tm = get_time();
    int hour = tm->tm_hour;
    if (global_settings.timeformat)
    {
        hour %= 12;
        if (hour == 0)
            hour = 12;
    }
    snprintf(buf, sizeof(buf), global_settings.timeformat ? "%d:%02d" : "%02d:%02d",
             hour, tm->tm_min);
    ui_text(0, y, big, UI_COL_TEXT, buf, vp->width, 0);
    y += font_get(big)->height;
    snprintf(buf, sizeof(buf), "%s %d %s", str(LANG_WEEKDAY_SUNDAY + tm->tm_wday),
             tm->tm_mday, str(LANG_MONTH_JANUARY + tm->tm_mon));
    ui_text(0, y, body, UI_COL_TEXT2, buf, vp->width, 0);

    /* the playing track */
    struct mp3entry *id3 = audio_status() ? audio_current_track() : NULL;
    if (id3)
    {
        const int ay = 92, tx = UI_MARGIN + LOCK_ART + UI_MARGIN;
        const int tw = vp->width - tx - UI_MARGIN;
        const int row = ui_font(UI_FONT_ROW);
        const struct bitmap *art = ui_art_current(LOCK_ART);
        if (art)
            ui_art_draw(art, UI_MARGIN, ay, LOCK_ART, 8);
        else
            ui_art_placeholder(UI_MARGIN, ay, LOCK_ART, 8);
        const char *title = id3->title && *id3->title ? id3->title : NULL;
        const char *artist = id3->artist && *id3->artist ? id3->artist :
                             id3->albumartist && *id3->albumartist ? id3->albumartist : NULL;
        int ty = ay + (LOCK_ART - font_get(row)->height -
                       (artist ? font_get(body)->height : 0)) / 2;
        if (title)
            ui_text(tx, ty, row, UI_COL_TEXT, title, tw, -1);
        if (artist)
            ui_text(tx, ty + font_get(row)->height, body, UI_COL_TEXT2,
                    artist, tw, -1);
    }

    ui_text(UI_MARGIN, vp->height - font_get(small)->height - 4, small,
            UI_COL_MUTED, str(LANG_NEWUI_UNLOCK_HINT),
            vp->width - 2 * UI_MARGIN, 0);
    lcd_update_viewport();
    lcd_set_viewport(last);
}

/* a value that changes when the screen should be redrawn */
static unsigned long state(void)
{
    struct tm *tm = get_time();
    unsigned long h = tm->tm_min * 60 + tm->tm_hour;
    struct mp3entry *id3 = audio_status() ? audio_current_track() : NULL;
    if (id3)
        for (const char *p = id3->path; *p; p++)
            h = h * 33 + (unsigned char)*p;
    return h;
}

int ui_lock_screen(void)
{
    struct viewport vp;
    viewport_set_defaults(&vp, SCREEN_MAIN);
    lcd_scroll_stop();
    button_clear_queue();

    unsigned long shown = state();
    long last_tap = current_tick - 2 * DOUBLE_TAP;
    draw(&vp);

    while (true)
    {
        send_event(GUI_EVENT_ACTIONUPDATE, NULL); /* status bar */
        int button = button_get_w_tmo(HZ / 2);

        if (IS_SYSEVENT(button))
            return button;
        if (button == (BUTTON_POWER | BUTTON_REL))
            return 0;
        if (button == (BUTTON_PLAY | BUTTON_REL))
        {
            if (TIME_BEFORE(current_tick, last_tap + DOUBLE_TAP))
                return 0;
            last_tap = current_tick;
        }

        if (button == BUTTON_NONE)
        {
            unsigned long now = state();
            if (now != shown)
            {
                shown = now;
                draw(&vp);
            }
        }
    }
}
