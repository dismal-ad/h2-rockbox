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

/* Now Playing's drawing (PLAN.md §4.5), the iPod's layout. The screen's
 * logic stays in apps/gui/wps.c, which calls ui_np_draw(). Drawn below the
 * status bar (which names the screen):
 *
 *                                            Shuffle
 *   [ cover, turned a little ]  Title
 *   [                        ]  Artist
 *   [                        ]  Album
 *                               3 of 12
 *   [ its reflection         ]
 *   0:15 [==========---------------] -6:05      progress, 12 px
 *
 * When the volume changes, the band shows it for a moment instead of the
 * progress: a bar between a quiet and a loud speaker, as the iPod. */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "lcd.h"
#include "font.h"
#include "metadata.h"
#include "screen_access.h"
#include "audio.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "viewport.h"
#include "wps.h"
#include "lang.h"
#include "language.h"
#include "string-extra.h"
#include "metadata.h"
#include "ui.h"

#define NP_ART         140     /* the cover is decoded at this size */
#define NP_BAND_Y      NPL_BAND_Y /* redrawn on time updates */
/* the iPod's layout (measured from its screen, scaled to 320x240): a
 * turned cover with a reflection, a thick bar with the times beside it */
#define NPL_ART_X      18
#define NPL_ART_Y      22      /* below the status bar */
#define NPL_ART_W      123     /* on screen, turned */
#define NPL_ART_HL     114     /* left edge, nearer */
#define NPL_ART_HR     104     /* right edge, further */
#define NPL_REFL_H     36
#define NPL_BAND_Y     178
#define NPL_BOTTOM     18      /* below the bar's outline */
#define NPL_TEXT_GAP   10      /* between the cover and the text */
#define NPL_TEXT_Y     30      /* the title, below the cover's top */
/* the bar's centre line, NPL_BOTTOM above the screen's bottom */
#define NPL_BAR_YC(vp) ((vp)->height - NPL_BOTTOM - 1 - NPL_BAR_H / 2)
#define NPL_BAR_H      14
#define NP_ARTIST_COL  UI_COL_ARTIST
#define NP_ALBUM_COL   UI_COL_ALBUM
#define OVERLAY_TICKS  (HZ * 3 / 2)

static long volume_until;   /* tick until which the volume overlay shows */
static bool volume_shown;
static bool scrubbing;

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

/* "0:04 [=====-------] -5:32" on one line, the bar thick, outlined,
 * the elapsed part in the accent's gradient */
/* the bar: a light gradient track in a grey outline, part/total of it in
 * the accent's gradient */
static void draw_bar(int bx, int by, int bw, long part, long total)
{
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_gradient_fillrect(bx, by, bw, NPL_BAR_H, ui_col(UI_C_TRACK_TOP),
                          ui_col(UI_C_TRACK_BOTTOM));
    int fill = total > 0 ? (int)((long long)bw * part / total) : 0;
    if (fill > 0)
        lcd_gradient_fillrect(bx, by, MIN(fill, bw), NPL_BAR_H,
                              ui_sel_top(), ui_accent());
    ui_stroke_round_rect(bx - 1, by - 1, bw + 2, NPL_BAR_H + 2, 0, 1,
                         ui_col(UI_C_OUTLINE));
}

/* While scrubbing, as the iPod: a thin empty track and a diamond at the
 * position, in the accent colour (lighter on its upper half) */
#define SCRUB_TRACK_H  6
#define VOL_BAR_W      204    /* the volume bar, as the iPod's */
#define DIAMOND_R      8
static void draw_scrub_bar(int bx, int yc, int bw, long part, long total)
{
    int ty = yc - SCRUB_TRACK_H / 2;
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_gradient_fillrect(bx, ty, bw, SCRUB_TRACK_H, ui_col(UI_C_TRACK_TOP),
                          ui_col(UI_C_TRACK_BOTTOM));
    ui_stroke_round_rect(bx - 1, ty - 1, bw + 2, SCRUB_TRACK_H + 2, 0, 1,
                         ui_col(UI_C_OUTLINE));
    int x = bx + (total > 0 ? (int)((long long)bw * part / total) : 0);
    for (int dy = -DIAMOND_R; dy <= DIAMOND_R; dy++)
    {
        int half = DIAMOND_R - abs(dy);
        ui_fill_rect(x - half, yc + dy, 2 * half + 1, 1,
                     dy < 0 ? ui_sel_top() : ui_accent());
    }
}

/* a speaker, 12 px tall, centred on y: its box and cone, and with waves
 * the louder one */
static void draw_speaker(int x, int yc, bool waves)
{
    const unsigned c = UI_COL_TEXT;
    ui_fill_rect(x, yc - 3, 4, 6, c);
    for (int i = 0; i < 6; i++)                 /* the cone, widening */
        ui_fill_rect(x + 4 + i, yc - 3 - i, 1, 6 + 2 * i, c);
    if (!waves)
        return;
    for (int r = 4; r <= 10; r += 6)            /* two arcs */
        for (int dy = -r; dy <= r; dy++)
        {
            int dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= r * r)
                dx++;
            ui_fill_rect(x + 11 + dx, yc + dy, 2, 1, c);
        }
}

/* the volume in place of the progress bar, as the iPod */
static void draw_volume_band(struct viewport *vp)
{
    const int band_h = vp->height - NPL_BAND_Y;
    const int yc = NPL_BAR_YC(vp);
    int lo = sound_min(SOUND_VOLUME), hi = sound_max(SOUND_VOLUME);
    /* a shorter bar, centred, the speakers just outside its ends */
    int bw = VOL_BAR_W, bx = (vp->width - bw) / 2;

    ui_fill_rect(0, NPL_BAND_Y, vp->width, band_h, UI_COL_BG);
    draw_speaker(bx - 8 - 10, yc, false);
    draw_speaker(bx + bw + 8, yc, true);
    draw_bar(bx, yc - NPL_BAR_H / 2, bw, global_status.volume - lo, hi - lo);
}

static void draw_band(struct viewport *vp, const struct mp3entry *id3,
                      long elapsed)
{
    const int font = ui_font(UI_FONT_ROW); /* Home's 20 px bold */
    const int band_h = vp->height - NPL_BAND_Y;
    char left[24], right[24];
    long pos = 0, len = id3 ? (long)id3->length : 0;
    if (len > 0)
        pos = MAX(0, MIN(elapsed, len));
    format_mmss(left, sizeof(left), pos, false);
    format_mmss(right, sizeof(right), len - pos, true);

    ui_fill_rect(0, NPL_BAND_Y, vp->width, band_h, UI_COL_BG);
    int lw = ui_text_width(font, "00:00"), rw = ui_text_width(font, "-00:00");
    int bx = UI_MARGIN + lw + 8, bw = vp->width - UI_MARGIN - rw - 8 - bx;
    int by = NPL_BAR_YC(vp) - NPL_BAR_H / 2;

    ui_text_vcenter(UI_MARGIN, by, NPL_BAR_H, font, UI_COL_TEXT, left, lw + 4, -1);
    ui_text_vcenter(vp->width - UI_MARGIN - rw - 4, by, NPL_BAR_H, font,
                    UI_COL_TEXT, right, rw + 4, 1);

    if (scrubbing)
        draw_scrub_bar(bx, by + NPL_BAR_H / 2, bw, pos, len);
    else
        draw_bar(bx, by, bw, pos, len);
}


/* as the iPod: the cover turned a little with its reflection,
 * title, artist, album and "1 of 8" beside it */
static void draw_cover_and_title(struct viewport *vp, const struct mp3entry *id3,
                                 char *buf, size_t len)
{
    const int body = ui_font(UI_FONT_BODY), title_f = ui_font(UI_FONT_TITLE);
    const int tx = NPL_ART_X + NPL_ART_W + NPL_TEXT_GAP;
    const int text_w = vp->width - tx - UI_MARGIN;

    const struct bitmap *art = ui_art_current(NP_ART);
    if (art)
        ui_art_draw_tilted(art, NPL_ART_X, NPL_ART_Y, NPL_ART_W,
                           NPL_ART_HL, NPL_ART_HR, NPL_REFL_H, 110);
    else
        ui_art_placeholder(NPL_ART_X, NPL_ART_Y, NPL_ART_HR, 6);

    const char *title = track_title(id3, buf, len);
    const char *artist = id3->artist && *id3->artist ? id3->artist :
                         id3->albumartist && *id3->albumartist ? id3->albumartist : NULL;
    const char *album = id3->album && *id3->album ? id3->album : NULL;
    int y = NPL_ART_Y + NPL_TEXT_Y;
    ui_text(tx, y, title_f, UI_COL_TEXT, title, text_w, -1);
    y += font_get(title_f)->height + 2;
    if (artist)
    {
        ui_text(tx, y, body, NP_ARTIST_COL, artist, text_w, -1);
        y += font_get(body)->height + 2;
    }
    if (album)
    {
        ui_text(tx, y, body, NP_ALBUM_COL, album, text_w, -1);
        y += font_get(body)->height + 2;
    }
    char pos[24];
    snprintf(pos, sizeof(pos), "%d of %d",
             playlist_get_display_index(), playlist_amount());
    ui_text(tx, y + 14, body, NP_ARTIST_COL, pos, text_w, -1);
}


void ui_np_set_scrub(bool on)
{
    scrubbing = on;
    ui_np_draw(false);
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
        volume_shown = false; /* back to the progress bar */
    }

    if (!full)
    {
        if (overlay)
            draw_volume_band(&vp);
        else
            draw_band(&vp, id3, elapsed);
        lcd_update_viewport_rect(0, NP_BAND_Y, vp.width, vp.height - NP_BAND_Y);
        lcd_set_viewport(last);
        return;
    }

    ui_status_title(str(LANG_NOW_PLAYING));
    ui_fill_rect(0, 0, vp.width, vp.height, UI_COL_BG);
    if (id3)
    {
        int small = ui_font(UI_FONT_SMALL);
        char buf[MAX_PATH];

        /* top right: the play mode ("3 of 12" is under the album) */
        const char *mode = global_settings.playlist_shuffle ? str(LANG_SHUFFLE) :
                           global_settings.repeat_mode == REPEAT_ONE ? str(LANG_REPEAT_ONE) :
                           global_settings.repeat_mode == REPEAT_ALL ? str(LANG_ALL) : NULL;
        if (mode)
            ui_text(UI_MARGIN, 3, small, UI_COL_TEXT2, mode,
                    vp.width - 2 * UI_MARGIN, 1);

        draw_cover_and_title(&vp, id3, buf, sizeof(buf));
    }
    if (overlay)
        draw_volume_band(&vp);
    else
        draw_band(&vp, id3, elapsed);
    /* slide in if a screen change asked for it */
    if (!ui_transition_run(vp.x, vp.y, vp.width, vp.height))
        lcd_update_viewport();
    lcd_set_viewport(last);
}

void ui_np_volume_changed(void)
{
    volume_until = current_tick + OVERLAY_TICKS;
    volume_shown = true;
    ui_np_draw(false); /* only the band changes */
}
