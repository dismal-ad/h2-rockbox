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

/* Now Playing's drawing (PLAN.md §4.5). The screen's logic stays in
 * apps/gui/wps.c, which calls ui_np_draw() where it used to update the
 * theme. Drawn below the status bar:
 *
 *   3 of 12                         Shuffle
 *   [ art 140x140 ]  Title
 *                    Artist
 *                    Album
 *   ======================------------------  progress, 4 px
 *   0:15                              -6:05
 *
 * The volume overlay appears over it for a moment when the volume changes. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "lcd.h"
#include "font.h"
#include "metadata.h"
#include "audio.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "viewport.h"
#include "wps.h"
#include "lang.h"
#include "language.h"
#include "string-extra.h"
#include "ui.h"

#define NP_ART         140
#define NP_ART_X       UI_MARGIN
#define NP_ART_Y       22
#define NP_TEXT_X      (NP_ART_X + NP_ART + UI_MARGIN)
#define NP_BAR_Y       176
#define NP_TIME_Y      184
#define NP_BAND_Y      170     /* bottom band redrawn on time updates */
#define NP_ARTIST_COL  LCD_RGBPACK(0xcc, 0xcc, 0xcc)
#define NP_ALBUM_COL   LCD_RGBPACK(0xbb, 0xbb, 0xbb)
#define OVERLAY_TICKS  (HZ * 3 / 2)

static long volume_until;   /* tick until which the volume overlay shows */
static bool volume_shown;

static void format_mmss(char *buf, size_t len, long ms, bool minus)
{
    long s = ms / 1000;
    if (s < 0)
        s = 0;
    if (s >= 3600)
        snprintf(buf, len, "%s%ld:%02ld:%02ld", minus ? "-" : "",
                 s / 3600, (s / 60) % 60, s % 60);
    else
        snprintf(buf, len, "%s%ld:%02ld", minus ? "-" : "", s / 60, s % 60);
}

/* title, falling back to the file name without its extension */
static const char *track_title(const struct mp3entry *id3, char *buf, size_t len)
{
    if (id3->title && *id3->title)
        return id3->title;
    const char *name = strrchr(id3->path, '/');
    strmemccpy(buf, name ? name + 1 : id3->path, len);
    char *dot = strrchr(buf, '.');
    if (dot && dot != buf)
        *dot = '\0';
    return buf;
}

static void draw_band(struct viewport *vp, const struct mp3entry *id3, long elapsed)
{
    int small = ui_font(UI_FONT_SMALL);
    int w = vp->width - 2 * UI_MARGIN;
    char buf[32];

    ui_fill_rect(0, NP_BAND_Y, vp->width, vp->height - NP_BAND_Y, UI_COL_BG);
    ui_fill_round_rect(UI_MARGIN, NP_BAR_Y, w, 4, 2, UI_COL_TRACK);
    if (id3 && id3->length > 0)
    {
        long pos = elapsed > (long)id3->length ? (long)id3->length : elapsed;
        int fill = (int)((long long)w * pos / id3->length);
        if (fill > 0)
            ui_fill_round_rect(UI_MARGIN, NP_BAR_Y, fill < 4 ? 4 : fill, 4, 2, UI_COL_SEL);
        format_mmss(buf, sizeof(buf), pos, false);
        ui_text(UI_MARGIN, NP_TIME_Y, small, UI_COL_TEXT2, buf, 0, -1);
        format_mmss(buf, sizeof(buf), id3->length - pos, true);
        ui_text(UI_MARGIN, NP_TIME_Y, small, UI_COL_TEXT2, buf, w, 1);
    }
}

static void draw_volume_overlay(struct viewport *vp)
{
    const int w = 216, h = 56;
    int x = (vp->width - w) / 2, y = NP_ART_Y + (NP_ART - h) / 2;
    int vol = global_status.volume;
    int lo = sound_min(SOUND_VOLUME), hi = sound_max(SOUND_VOLUME);
    char buf[24];

    ui_fill_round_rect(x, y, w, h, 12, LCD_RGBPACK(0x26, 0x26, 0x26));
    ui_text(x + 14, y + 8, ui_font(UI_FONT_SMALL), UI_COL_TEXT2,
            str(LANG_VOLUME), 0, -1);
    snprintf(buf, sizeof(buf), "%d %s", vol, sound_unit(SOUND_VOLUME));
    ui_text(x + 14, y + 6, ui_font(UI_FONT_BODY), UI_COL_TEXT, buf, w - 28, 1);
    int bw = w - 28, by = y + h - 18;
    ui_fill_round_rect(x + 14, by, bw, 6, 3, UI_COL_TRACK);
    int fill = hi > lo ? bw * (vol - lo) / (hi - lo) : 0;
    if (fill > 0)
        ui_fill_round_rect(x + 14, by, fill < 6 ? 6 : fill, 6, 3, UI_COL_SEL);
}

void ui_np_draw(bool full)
{
    struct wps_state *state = get_wps_state();
    struct mp3entry *id3 = state->id3;
    struct viewport vp;
    viewport_set_defaults(&vp, SCREEN_MAIN);
    struct viewport *last = lcd_set_viewport(&vp);

    long elapsed = id3 ? (long)id3->elapsed + state->ff_rewind_count : 0;
    bool overlay = volume_shown && TIME_BEFORE(current_tick, volume_until);
    if (volume_shown && !overlay)
    {
        volume_shown = false;
        full = true; /* take the overlay down */
    }

    if (!full && !overlay)
    {
        draw_band(&vp, id3, elapsed);
        lcd_update_viewport_rect(0, NP_BAND_Y, vp.width, vp.height - NP_BAND_Y);
        lcd_set_viewport(last);
        return;
    }

    ui_fill_rect(0, 0, vp.width, vp.height, UI_COL_BG);
    if (id3)
    {
        int small = ui_font(UI_FONT_SMALL), body = ui_font(UI_FONT_BODY);
        char buf[MAX_PATH];
        int text_w = vp.width - NP_TEXT_X - UI_MARGIN;

        /* top row: position in the playlist, play mode */
        snprintf(buf, sizeof(buf), "%d of %d",
                 playlist_get_display_index(), playlist_amount());
        ui_text(UI_MARGIN, 3, small, UI_COL_TEXT2, buf, 0, -1);
        const char *mode = global_settings.playlist_shuffle ? str(LANG_SHUFFLE) :
                           global_settings.repeat_mode == REPEAT_ONE ? str(LANG_REPEAT_ONE) :
                           global_settings.repeat_mode == REPEAT_ALL ? str(LANG_ALL) : NULL;
        if (mode)
            ui_text(UI_MARGIN, 3, small, UI_COL_TEXT2, mode,
                    vp.width - 2 * UI_MARGIN, 1);

        /* cover */
        const struct bitmap *art = ui_art_current(NP_ART);
        if (art)
            ui_art_draw(art, NP_ART_X, NP_ART_Y, NP_ART, 10);
        else
            ui_art_placeholder(NP_ART_X, NP_ART_Y, NP_ART, 10);

        /* title, artist, album, centred against the cover */
        const char *title = track_title(id3, buf, sizeof(buf));
        const char *artist = id3->artist && *id3->artist ? id3->artist :
                             id3->albumartist && *id3->albumartist ? id3->albumartist : NULL;
        const char *album = id3->album && *id3->album ? id3->album : NULL;
        int block = 29 + (artist ? 24 : 0) + (album ? 20 : 0);
        int y = NP_ART_Y + (NP_ART - block) / 2;
        ui_text(NP_TEXT_X, y, ui_font(UI_FONT_TITLE), UI_COL_TEXT, title, text_w, -1);
        y += 29;
        if (artist)
        {
            ui_text(NP_TEXT_X, y, body, NP_ARTIST_COL, artist, text_w, -1);
            y += 24;
        }
        if (album)
            ui_text(NP_TEXT_X, y, body, NP_ALBUM_COL, album, text_w, -1);
    }
    draw_band(&vp, id3, elapsed);
    if (overlay)
        draw_volume_overlay(&vp);
    lcd_update_viewport();
    lcd_set_viewport(last);
}

void ui_np_volume_changed(void)
{
    volume_until = current_tick + OVERLAY_TICKS;
    volume_shown = true;
    ui_np_draw(true);
}
