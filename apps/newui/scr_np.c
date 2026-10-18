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
 *   3 of 12             • o o           Shuffle
 *   [ art 140x140 ]  Title
 *                    Artist
 *                    Album
 *   ======================------------------  progress, 4 px
 *   0:15                              -6:05
 *
 * Four pages (dots at the top; Centre twice for the next): the cover with
 * the title, the track's details, a big cover, and level meters. While scrubbing the bar
 * is white and thicker. The volume overlay appears over it for a moment
 * when the volume changes. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "lcd.h"
#include "font.h"
#include "metadata.h"
#include "screen_access.h"
#include "peakmeter.h"
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

#define NP_ART         140
#define NP_ART_X       UI_MARGIN
#define NP_ART_Y       22
#define NP_BAND_Y      NPL_BAND_Y /* redrawn on time updates */
/* the iPod's layout: a turned cover with a reflection, a thick bar with
 * the times beside it */
#define NPL_ART_Y      4
#define NPL_ART_W      132     /* on screen, turned */
#define NPL_ART_HL     138     /* left edge, nearer */
#define NPL_ART_HR     124     /* right edge, further */
#define NPL_REFL_H     40
#define NPL_BAND_Y     188
#define NPL_BAR_H      12
#define NP_ARTIST_COL  UI_COL_ARTIST
#define NP_ALBUM_COL   UI_COL_ALBUM
#define OVERLAY_TICKS  (HZ * 3 / 2)
#define NP_BIG_ART     148    /* Home's size: shares its decoded cover */
#define NP_PAGES       4
#define NP_PAGE_LEVELS 3
#define METER_SEGS     20
#define METER_W        56
#define METER_GAP      24
#define PEAK_HOLD      (HZ * 3 / 2)

static long volume_until;   /* tick until which the volume overlay shows */
static bool volume_shown;
static int page;            /* 0: cover and title, 1: details, 2: big cover */
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
static void draw_band(struct viewport *vp, const struct mp3entry *id3,
                      long elapsed)
{
    const int font = ui_font(UI_FONT_BODY);
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
    int by = NPL_BAND_Y + (band_h - NPL_BAR_H) / 2;

    ui_text_vcenter(UI_MARGIN, NPL_BAND_Y, band_h, font, UI_COL_TEXT, left, lw + 4, -1);
    ui_text_vcenter(vp->width - UI_MARGIN - rw - 4, NPL_BAND_Y, band_h, font,
                    UI_COL_TEXT, right, rw + 4, 1);

    /* track: a light gradient in a grey outline */
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_gradient_fillrect(bx, by, bw, NPL_BAR_H, ui_col(UI_C_TRACK_TOP),
                          ui_col(UI_C_TRACK_BOTTOM));
    int fill = len > 0 ? (int)((long long)bw * pos / len) : 0;
    if (fill > 0)
    {
        if (scrubbing)
            ui_fill_rect(bx, by, fill, NPL_BAR_H, UI_COL_TEXT);
        else
            lcd_gradient_fillrect(bx, by, fill, NPL_BAR_H, ui_sel_top(), ui_accent());
    }
    ui_stroke_round_rect(bx - 1, by - 1, bw + 2, NPL_BAR_H + 2, 0, 1,
                         ui_col(UI_C_OUTLINE));
}

static void draw_volume_overlay(struct viewport *vp)
{
    const int w = 216, h = 56;
    int x = (vp->width - w) / 2, y = NP_ART_Y + (NP_ART - h) / 2;
    int vol = global_status.volume;
    int lo = sound_min(SOUND_VOLUME), hi = sound_max(SOUND_VOLUME);
    char buf[24];

    ui_fill_round_rect(x, y, w, h, 12, UI_COL_PANEL);
    ui_text(x + 14, y + 8, ui_font(UI_FONT_SMALL), UI_COL_TEXT2,
            str(LANG_VOLUME), 0, -1);
    snprintf(buf, sizeof(buf), "%d %s", vol, sound_unit(SOUND_VOLUME));
    ui_text(x + 14, y + 6, ui_font(UI_FONT_BODY), UI_COL_TEXT, buf, w - 28, 1);
    int bw = w - 28, by = y + h - 18;
    ui_fill_round_rect(x + 14, by, bw, 6, 3, UI_COL_TRACK);
    int fill = hi > lo ? bw * (vol - lo) / (hi - lo) : 0;
    if (fill > 0)
        ui_fill_round_rect(x + 14, by, fill < 6 ? 6 : fill, 6, 3, UI_COL_FILL);
}

/* page 3: left and right level meters, 2 dB a segment from -40 dB, with
 * the peak held for a moment */
static const unsigned short meter_thr[METER_SEGS] = {
    413, 519, 654, 823, 1036, 1304, 1642, 2067, 2603, 3277,
    4125, 5193, 6538, 8231, 10362, 13045, 16422, 20675, 26028, 32767,
};

static void draw_levels(struct viewport *vp)
{
    static int hold[2];
    static long hold_until[2];
    int lv[2];
    peak_meter_peek();
    peak_meter_current_vals(&lv[0], &lv[1]);

    const int seg_h = (NP_ART - 4) / METER_SEGS;
    const int x0 = (vp->width - 2 * METER_W - METER_GAP) / 2;
    const int bottom = NP_ART_Y + NP_ART;
    ui_fill_rect(0, NP_ART_Y, vp->width, NP_ART + 2, UI_COL_BG);
    for (int ch = 0; ch < 2; ch++)
    {
        int n = 0;
        while (n < METER_SEGS && lv[ch] >= meter_thr[n])
            n++;
        if (n >= hold[ch] || TIME_AFTER(current_tick, hold_until[ch]))
        {
            hold[ch] = n;
            hold_until[ch] = current_tick + PEAK_HOLD;
        }
        int x = x0 + ch * (METER_W + METER_GAP);
        for (int i = 0; i < METER_SEGS; i++)
        {
            int y = bottom - (i + 1) * seg_h;
            unsigned c = i < n ? (i >= METER_SEGS - 2 ? LCD_RGBPACK(0xe0, 0x40, 0x40) :
                                  i >= METER_SEGS - 5 ? UI_COL_FILL : UI_COL_ACCENT)
                               : UI_COL_TRACK;
            if (i == hold[ch] - 1 && i >= n)
                c = UI_COL_TEXT2;
            ui_fill_round_rect(x, y, METER_W, seg_h - 2, 2, c);
        }
    }
}

bool ui_np_wants_fast_updates(void)
{
    return page == NP_PAGE_LEVELS;
}

/* page 0, as the iPod: the cover turned a little with its reflection,
 * title, artist, album and "1 of 8" beside it */
static void draw_cover_and_title(struct viewport *vp, const struct mp3entry *id3,
                                 char *buf, size_t len)
{
    const int body = ui_font(UI_FONT_BODY), title_f = ui_font(UI_FONT_TITLE);
    const int tx = NP_ART_X + NPL_ART_W + UI_MARGIN + 4;
    const int text_w = vp->width - tx - UI_MARGIN;

    const struct bitmap *art = ui_art_current(NP_ART);
    if (art)
        ui_art_draw_tilted(art, NP_ART_X, NPL_ART_Y, NPL_ART_W,
                           NPL_ART_HL, NPL_ART_HR, NPL_REFL_H, 110);
    else
        ui_art_placeholder(NP_ART_X, NPL_ART_Y, NPL_ART_HR, 6);

    const char *title = track_title(id3, buf, len);
    const char *artist = id3->artist && *id3->artist ? id3->artist :
                         id3->albumartist && *id3->albumartist ? id3->albumartist : NULL;
    const char *album = id3->album && *id3->album ? id3->album : NULL;
    int y = NPL_ART_Y + 36;
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


/* page 1: the track's details, a label and a value per row */
static void draw_details(struct viewport *vp, const struct mp3entry *id3)
{
    const int small = ui_font(UI_FONT_SMALL), body = ui_font(UI_FONT_BODY);
    const int lx = UI_MARGIN, vx = UI_MARGIN + 92, row = 24;
    const int vw = vp->width - vx - UI_MARGIN;
    char buf[MAX_PATH];
    int y = NP_ART_Y + 2;

    struct { int lang; char value[64]; } rows[6];
    int n = 0;

    rows[n].lang = LANG_NEWUI_FORMAT;
    strmemccpy(rows[n++].value, get_codec_string(id3->codectype), 64);
    rows[n].lang = LANG_NEWUI_BITRATE;
    snprintf(rows[n++].value, 64, "%u kbps%s", id3->bitrate, id3->vbr ? " VBR" : "");
    rows[n].lang = LANG_NEWUI_SAMPLE_RATE;
    snprintf(rows[n++].value, 64, "%lu.%lu kHz", id3->frequency / 1000,
             (id3->frequency % 1000) / 100);
    if (id3->year > 0)
    {
        rows[n].lang = LANG_ID3_YEAR;
        snprintf(rows[n++].value, 64, "%d", id3->year);
    }
    if (id3->genre_string && *id3->genre_string)
    {
        rows[n].lang = LANG_ID3_GENRE;
        strmemccpy(rows[n++].value, id3->genre_string, 64);
    }
    if (id3->tracknum > 0)
    {
        rows[n].lang = LANG_NEWUI_TRACK;
        snprintf(rows[n++].value, 64, "%d", id3->tracknum);
    }

    for (int i = 0; i < n; i++, y += row)
    {
        ui_text_vcenter(lx, y, row, small, UI_COL_TEXT2, str(rows[i].lang), vx - lx - 6, -1);
        ui_text_vcenter(vx, y, row, body, UI_COL_TEXT, rows[i].value, vw, -1);
    }

    /* the folder, shortened */
    strmemccpy(buf, id3->path, sizeof(buf));
    char *slash = strrchr(buf, '/');
    if (slash && slash != buf)
        *slash = '\0';
    if (y + row <= NP_BAND_Y)
    {
        ui_text_vcenter(lx, y, row, small, UI_COL_TEXT2, str(LANG_NEWUI_FOLDER), vx - lx - 6, -1);
        ui_text_vcenter(vx, y, row, small, UI_COL_TEXT, buf, vw, -1);
    }
}

void ui_np_next_page(void)
{
    page = (page + 1) % NP_PAGES;
    ui_np_draw(true);
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
        volume_shown = false;
        full = true; /* take the overlay down */
    }

    if (!full && !overlay)
    {
        draw_band(&vp, id3, elapsed);
        if (page == NP_PAGE_LEVELS && id3)
        {
            draw_levels(&vp);
            lcd_update_viewport_rect(0, NP_ART_Y, vp.width, vp.height - NP_ART_Y);
        }
        else
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

        /* top row: position in the playlist, play mode */
        snprintf(buf, sizeof(buf), "%d of %d",
                 playlist_get_display_index(), playlist_amount());
        if (page != 0) /* page 0 shows it under the album */
            ui_text(UI_MARGIN, 3, small, UI_COL_TEXT2, buf, 0, -1);
        const char *mode = global_settings.playlist_shuffle ? str(LANG_SHUFFLE) :
                           global_settings.repeat_mode == REPEAT_ONE ? str(LANG_REPEAT_ONE) :
                           global_settings.repeat_mode == REPEAT_ALL ? str(LANG_ALL) : NULL;
        if (mode)
            ui_text(UI_MARGIN, 3, small, UI_COL_TEXT2, mode,
                    vp.width - 2 * UI_MARGIN, 1);

        /* page dots, in the middle of the top row */
        /* over the text column, clear of the turned cover */
        const int dots_x = NP_ART_X + NPL_ART_W + UI_MARGIN + 4 +
            (vp.width - NP_ART_X - NPL_ART_W - 2 * UI_MARGIN - 4 - NP_PAGES * 12) / 2;
        for (int i = 0; i < NP_PAGES; i++)
            ui_fill_round_rect(dots_x + i * 12, 8, 6, 6, 3,
                               i == page ? UI_COL_TEXT :
                               ui_light() ? UI_COL_MUTED : UI_COL_ACCENT);

        if (page == NP_PAGE_LEVELS)
            draw_levels(&vp);
        else if (page == 1)
            draw_details(&vp, id3);
        else if (page == 2)
        {
            int x = (vp.width - NP_BIG_ART) / 2;
            const struct bitmap *art = ui_art_current(NP_BIG_ART);
            if (art)
                ui_art_draw(art, x, NP_ART_Y, NP_BIG_ART, 10);
            else
                ui_art_placeholder(x, NP_ART_Y, NP_BIG_ART, 10);
        }
        else
            draw_cover_and_title(&vp, id3, buf, sizeof(buf));
    }
    draw_band(&vp, id3, elapsed);
    if (overlay)
        draw_volume_overlay(&vp);
    /* slide in if a screen change asked for it */
    if (!ui_transition_run(vp.x, vp.y, vp.width, vp.height))
        lcd_update_viewport();
    lcd_set_viewport(last);
}

void ui_np_volume_changed(void)
{
    volume_until = current_tick + OVERLAY_TICKS;
    volume_shown = true;
    ui_np_draw(true);
}
