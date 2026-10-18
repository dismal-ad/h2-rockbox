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

/* Cover Flow (PLAN.md §4.4): the database's albums as covers in a row,
 * the centre one facing the viewer, the others turned towards it, all with
 * a faint reflection. Album name and artist below.
 *
 * Wheel/Prev/Next move one album per step (faster when spinning), animated.
 * Centre opens the album's songs; a song starts the album from it. Back
 * returns to Home.
 *
 * Covers are drawn straight into the frame buffer column by column, with
 * perspective-correct texture mapping for the turned ones.
 *
 * Each album's first track (for its cover) and artist are found in one pass
 * over the database when Cover Flow opens, and kept while the database is
 * the same. Covers saved on the card (ui_thumb.c) are read even while the
 * covers move, a few per frame; while nothing moves and no key is pending,
 * the missing ones are built, the shown ones first and then the rest of the
 * library outwards from the centre, so later visits find them saved. A
 * placeholder stands in until a cover is loaded. */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "action.h"
#include "font.h"
#include "kernel.h"
#include "lang.h"
#include "language.h"
#include "lcd.h"
#include "list.h"
#include "misc.h"
#include "playlist.h"
#include "root_menu.h"
#include "settings.h"
#include "splash.h"
#include "string-extra.h"
#include "tagcache.h"
#include "tagtree.h"
#include "viewport.h"
#include "ui.h"

#if LCD_PIXELFORMAT != RGB565
#error "Cover Flow draws RGB565 pixels directly"
#endif

/* layout at 320 wide; x of the left half, mirrored for the right.
 * As on the iPod, three covers each side show equal strips (CF_SPACING
 * wide): steeply turned to face the centre (their edges converge behind
 * it), the nearest tucked under the centre cover. */
#define CF_CENTER_L   (LCD_WIDTH / 2 - UI_COVER / 2)  /* centre cover */
#define CF_SIDE_W     46                  /* side cover's width on screen */
#define CF_SPACING    27                  /* the strip each side cover shows */
#define CF_SIDE_R     (CF_CENTER_L - CF_SPACING + CF_SIDE_W) /* nearest's inner edge */
#define CF_SIDE_HIN   112                 /* its inner edge's height: */
#define CF_SIDE_HOUT  142                 /* shorter than the outer, so the
                                             covers face the centre */
#define CF_SIDE_SHADE 190                 /* side covers' brightness /256 */
#define CF_TOP        4                   /* centre cover's top */
#define CF_REFLECT    80                  /* reflection's starting brightness /256 */
#define CF_VISIBLE    4                   /* covers drawn on each side */

#define MAX_ALBUMS    2000
#define NAMES_SIZE    (64 * 1024)
#define POOL_SIZE     (192 * 1024)  /* albums' track paths and artists */
#define MAX_TRACKS    200
#define TRACKS_SIZE   (32 * 1024)
#define LOADS_MOVING  2             /* saved covers read per frame in motion */
#define PROBES        16            /* covers checked per background step */

enum cover_state {
    COVER_UNKNOWN,      /* not checked */
    COVER_SAVED,        /* on the card (or "no art" is) */
    COVER_TO_BUILD,     /* has to be decoded */
};

struct album {
    int seek;           /* tag seek, the database's album id */
    int name;           /* offset in names[] */
    int path;           /* first track, offset in pool[]; -1: none */
    int artist;         /* offset in pool[] */
    unsigned char cover;
};
static struct album albums[MAX_ALBUMS];
static char names[NAMES_SIZE];
static char pool[POOL_SIZE];
static int album_count;
static unsigned long albums_stamp;  /* the album list they were found for */
static int current;         /* centred album, kept between visits */
static int bg_next;         /* background building: distance reached */

static fb_data placeholder[UI_COVER * UI_COVER];

static int compare_albums(const void *a, const void *b)
{
    return strcasecmp(names + ((const struct album *)a)->name,
                      names + ((const struct album *)b)->name);
}

static short by_seek[MAX_ALBUMS];   /* album indexes in seek order */

static int compare_seeks(const void *a, const void *b)
{
    return albums[*(const short *)a].seek - albums[*(const short *)b].seek;
}

static int album_of_seek(int32_t seek)
{
    int lo = 0, hi = album_count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2, s = albums[by_seek[mid]].seek;
        if (s == seek)
            return by_seek[mid];
        if (s < seek)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static int pool_add(int *used, const char *str)
{
    int len = strlen(str) + 1;
    if (*used + len > POOL_SIZE)
        return -1;
    memcpy(pool + *used, str, len);
    *used += len;
    return *used - len;
}

/* one pass over the tracks: each album's first track and its artist */
static void find_tracks(void)
{
    struct tagcache_search tcs;
    static char path[MAX_PATH];
    char artist[64];
    int used = 0, found = 0;

    for (int i = 0; i < album_count; i++)
    {
        albums[i].path = albums[i].artist = -1;
        albums[i].cover = COVER_UNKNOWN;
        by_seek[i] = i;
    }
    qsort(by_seek, album_count, sizeof(by_seek[0]), compare_seeks);

    if (!tagcache_search(&tcs, tag_filename))
        return;
    splash_progress_set_delay(HZ / 2);
    while (found < album_count && tagcache_get_next(&tcs, path, sizeof(path)))
    {
        int a = album_of_seek(tagcache_get_tag_seek(&tcs, tag_album));
        if (a < 0 || albums[a].path >= 0)
            continue;
        if (!tagcache_retrieve(&tcs, tcs.idx_id, tag_albumartist, artist, sizeof(artist))
            || !*artist || !strcmp(artist, UNTAGGED))
        {
            if (!tagcache_retrieve(&tcs, tcs.idx_id, tag_artist, artist, sizeof(artist))
                || !strcmp(artist, UNTAGGED))
                *artist = '\0';
        }
        albums[a].path = pool_add(&used, path);
        albums[a].artist = pool_add(&used, artist);
        found++;
        splash_progress(found, album_count, "%s", str(LANG_NEWUI_CF_PREPARING));
    }
    tagcache_search_finish(&tcs);
}

/* The album list, sorted by name; the tracks are looked for again only
 * when it changed. */
static bool load_albums(void)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    unsigned long stamp = 5381;
    int used = 0, count = 0;

    if (!tagcache_search(&tcs, tag_album))
        return false;
    while (count < MAX_ALBUMS && tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        int len = strlen(buf) + 1;
        if (!strcmp(buf, UNTAGGED) || used + len > NAMES_SIZE)
            continue;
        stamp = stamp * 33 + tcs.result_seek + len;
        memcpy(names + used, buf, len);
        albums[count].seek = tcs.result_seek;
        albums[count].name = used;
        count++;
        used += len;
    }
    tagcache_search_finish(&tcs);
    stamp = stamp * 33 + count;

    if (stamp != albums_stamp || count != album_count)
    {
        album_count = count;
        qsort(albums, album_count, sizeof(albums[0]), compare_albums);
        find_tracks();
        albums_stamp = stamp;
        bg_next = 0;
    }
    else
    {
        /* same list: names[] was rewritten identically, the rest stays */
    }
    return true;
}

static const char *album_path(int a)
{
    return albums[a].path >= 0 ? pool + albums[a].path : NULL;
}

/* Reads saved covers of the albums shown around 'centre' into RAM,
 * nearest first, at most 'limit'; true if any was read */
static bool load_saved(int centre, int limit)
{
    bool loaded = false;
    for (int d = 0; d <= CF_VISIBLE && limit > 0; d++)
        for (int s = (d ? -1 : 1); s <= 1 && limit > 0; s += 2)
        {
            int a = centre + s * d;
            if (a < 0 || a >= album_count || !album_path(a) ||
                albums[a].cover == COVER_TO_BUILD)
                continue;
            bool known;
            ui_cover_peek(album_path(a), &known);
            if (known)
                continue;
            limit--;
            if (ui_cover_load_saved(album_path(a)))
            {
                albums[a].cover = COVER_SAVED;
                loaded = true;
            }
            else
                albums[a].cover = COVER_TO_BUILD;
        }
    return loaded;
}

static bool build(int a)
{
    if (albums[a].cover == COVER_UNKNOWN)
        albums[a].cover = ui_cover_saved(album_path(a)) ? COVER_SAVED
                                                        : COVER_TO_BUILD;
    if (albums[a].cover != COVER_TO_BUILD)
        return false;
    ui_cover_build(album_path(a));
    albums[a].cover = COVER_SAVED;
    return true;
}

/* One step of background work while nothing moves: build a missing cover,
 * the shown ones first, then outwards through the library. False when
 * there is nothing left to do. */
static bool idle_work(void)
{
    for (int d = 0; d <= CF_VISIBLE; d++)
        for (int s = (d ? -1 : 1); s <= 1; s += 2)
        {
            int a = current + s * d;
            if (a >= 0 && a < album_count && album_path(a) &&
                albums[a].cover == COVER_TO_BUILD)
                return build(a);
        }

    for (int probes = 0; bg_next < album_count && probes < PROBES; )
    {
        int d = bg_next;
        for (int s = (d ? -1 : 1); s <= 1; s += 2)
        {
            int a = current + s * d;
            if (a < 0 || a >= album_count || !album_path(a))
                continue;
            probes++;
            if (build(a))
                return true;
        }
        bg_next++;
    }
    return bg_next < album_count;
}

static void make_placeholder(void)
{
    const fb_data bg = FB_SCALARPACK(UI_COL_TRACK);
    const fb_data fg = FB_SCALARPACK(UI_COL_MUTED);
    for (int i = 0; i < UI_COVER * UI_COVER; i++)
        placeholder[i] = bg;
    /* a quaver, as ui_art_placeholder draws it */
    const int u = UI_COVER / 16, c = UI_COVER / 2;
    const int hx = c - 3 * u, hy = c + 2 * u;
    for (int y = 0; y < UI_COVER; y++)
        for (int x = 0; x < UI_COVER; x++)
        {
            int dx = 2 * (x - hx) - 4 * u, dy = 2 * (y - hy) - 3 * u;
            bool head = dx * dx * 9 + dy * dy * 16 <= 36 * u * u * 4;
            bool stem = x >= hx + 3 * u && x < hx + 4 * u &&
                        y >= c - 4 * u && y < c + 3 * u;
            bool flag = x >= hx + 3 * u && x < hx + 6 * u &&
                        y >= c - 4 * u && y < c - 3 * u;
            if (head || stem || flag)
                placeholder[y * UI_COVER + x] = fg;
        }
}

/* RGB565 pixel mixed toward the background: a/256 of it is kept (darker
 * on the Dark theme, paler on the Light one) */
static unsigned bg_r, bg_g, bg_b;   /* the background, RGB565 fields */

static inline fb_data shade(fb_data c, int a)
{
    unsigned r = bg_r + ((((int)(c >> 11) - (int)bg_r) * a) >> 8);
    unsigned g = bg_g + (((((int)(c >> 5) & 63) - (int)bg_g) * a) >> 8);
    unsigned b = bg_b + (((((int)c & 31) - (int)bg_b) * a) >> 8);
    return (r << 11) | (g << 5) | b;
}

/* Draws src between columns xl and xr, its left edge hl and its right edge
 * hr tall, centred on line yc, brightness a/256, and its reflection down to
 * line yref (all relative to fb, which is vh lines tall). */
static void draw_cover(fb_data *fb, int vh, const fb_data *src,
                       int xl, int xr, int hl, int hr, int a, int yc, int yref)
{
    const int w = xr - xl;
    if (w <= 0)
        return;
    for (int x = MAX(xl, 0); x < MIN(xr, LCD_WIDTH); x++)
    {
        /* t: position across, 0..65536; u: texture column, interpolated
         * in 1/depth so the turned cover looks right */
        int t = ((x - xl) << 16) / w;
        int h = hl + (((hr - hl) * t) >> 16);
        if (h <= 0)
            continue;
        long long den = (long long)(65536 - t) * hl + (long long)t * hr;
        int u = den ? (int)((long long)t * hr * UI_COVER / den) : 0;
        if (u >= UI_COVER)
            u = UI_COVER - 1;

        const fb_data *col = src + u;
        const int step = (UI_COVER << 16) / h;
        int top = yc - h / 2, v = 0;
        fb_data *d = fb + top * LCD_WIDTH + x;
        for (int y = top; y < top + h; y++, v += step, d += LCD_WIDTH)
            if (y >= 0 && y < vh)
                *d = a >= 256 ? col[(v >> 16) * UI_COVER]
                              : shade(col[(v >> 16) * UI_COVER], a);

        /* reflection: the cover upside down, fading out */
        const int rh = h * 3 / 10;
        int y = top + h + 1;
        d = fb + y * LCD_WIDTH + x;
        v = (UI_COVER << 16) - 1;
        for (int i = 0; i < rh && y < yref; i++, y++, v -= step, d += LCD_WIDTH)
            if (y >= 0 && v >= 0)
                *d = shade(col[(v >> 16) * UI_COVER],
                           a * CF_REFLECT * (rh - i) / rh >> 8);
    }
}

static inline int lerp(int a, int b, int t) /* t 0..256 */
{
    return a + (((b - a) * t) >> 8);
}

/* where the cover k/256 albums from the centre goes */
static void place(int k, int *xl, int *xr, int *hl, int *hr, int *a)
{
    int n = abs(k);
    int l, r, hout, hin;
    if (n <= 256)
    {
        /* between the centre and the first place on the left */
        l = lerp(CF_CENTER_L, CF_SIDE_R - CF_SIDE_W, n);
        r = lerp(CF_CENTER_L + UI_COVER, CF_SIDE_R, n);
        hout = lerp(UI_COVER, CF_SIDE_HOUT, n);
        hin = lerp(UI_COVER, CF_SIDE_HIN, n);
        *a = lerp(256, CF_SIDE_SHADE, n);
    }
    else
    {
        int shift = (n - 256) * CF_SPACING >> 8;
        l = CF_SIDE_R - CF_SIDE_W - shift;
        r = CF_SIDE_R - shift;
        hout = CF_SIDE_HOUT;
        hin = CF_SIDE_HIN;
        *a = CF_SIDE_SHADE;
    }
    if (k < 0)
    {
        *xl = l; *xr = r; *hl = hout; *hr = hin;
    }
    else
    {
        *xl = LCD_WIDTH - r; *xr = LCD_WIDTH - l; *hl = hin; *hr = hout;
    }
}

static void draw(struct viewport *vp, int pos)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int row = ui_font(UI_FONT_ROW), body = ui_font(UI_FONT_BODY);
    const int row_h = font_get(row)->height, body_h = font_get(body)->height;
    const int text_y = vp->height - body_h - row_h - 2;
    const int yc = CF_TOP + UI_COVER / 2;
    fb_data *fb = FBADDR(vp->x, vp->y);

    ui_fill_rect(0, 0, vp->width, vp->height, UI_COL_BG);

    /* farthest first, so nearer covers overlap them */
    int centre = (pos + 128) >> 8;
    for (int d = CF_VISIBLE + 1; d >= 0; d--)
        for (int s = -1; s <= 1; s += 2)
        {
            int a = centre + s * d;
            if ((d == 0 && s > 0) || a < 0 || a >= album_count)
                continue;
            int k = a * 256 - pos;
            if (abs(k) > (CF_VISIBLE + 1) * 256)
                continue;
            int xl, xr, hl, hr, shade_a;
            place(k, &xl, &xr, &hl, &hr, &shade_a);
            bool known;
            const fb_data *px = album_path(a) ? ui_cover_peek(album_path(a), &known)
                                              : NULL;
            draw_cover(fb, vp->height, px ? px : placeholder,
                       xl, xr, hl, hr, shade_a, yc, text_y - 2);
        }

    if (album_count > 0)
    {
        int w = vp->width - 2 * UI_MARGIN;
        ui_text(UI_MARGIN, text_y, row, UI_COL_TEXT,
                names + albums[centre].name, w, 0);
        if (albums[centre].artist >= 0 && pool[albums[centre].artist])
            ui_text(UI_MARGIN, text_y + row_h, body, UI_COL_TEXT2,
                    pool + albums[centre].artist, w, 0);
    }
    /* slide in if a screen change asked for it */
    if (!ui_transition_run(vp->x, vp->y, vp->width, vp->height))
        lcd_update_viewport();
    lcd_set_viewport(last);
}

/* --- an album's songs --- */

struct track {
    int sort;       /* disc and track number; then by title */
    int title;      /* offsets in track_text[] */
    int path;
    int length;     /* ms */
};
static struct track tracks[MAX_TRACKS];
static char track_text[TRACKS_SIZE];
static int track_count;

static int compare_tracks(const void *a, const void *b)
{
    const struct track *x = a, *y = b;
    if (x->sort != y->sort)
        return x->sort - y->sort;
    return strcasecmp(track_text + x->title, track_text + y->title);
}

static bool load_tracks(int seek)
{
    struct tagcache_search tcs;
    char title[TAGCACHE_BUFSZ], path[MAX_PATH];
    int used = 0;

    track_count = 0;
    if (!tagcache_search(&tcs, tag_title))
        return false;
    tagcache_search_add_filter(&tcs, tag_album, seek);
    while (track_count < MAX_TRACKS &&
           tagcache_get_next(&tcs, title, sizeof(title)))
    {
        if (!tagcache_retrieve(&tcs, tcs.idx_id, tag_filename, path, sizeof(path)))
            continue;
        const char *name = title;
        if (!strcmp(title, UNTAGGED))
        {
            const char *slash = strrchr(path, '/');
            name = slash ? slash + 1 : path;
        }
        int tl = strlen(name) + 1, pl = strlen(path) + 1;
        if (used + tl + pl > TRACKS_SIZE)
            break;
        struct track *t = &tracks[track_count];
        int disc = tagcache_get_numeric(&tcs, tag_discnumber);
        int num = tagcache_get_numeric(&tcs, tag_tracknumber);
        t->sort = (MAX(disc, 0) << 16) + MAX(num, 0);
        t->length = tagcache_get_numeric(&tcs, tag_length);
        t->title = used;
        memcpy(track_text + used, name, tl);
        t->path = used + tl;
        memcpy(track_text + used + tl, path, pl);
        used += tl + pl;
        track_count++;
    }
    tagcache_search_finish(&tcs);
    qsort(tracks, track_count, sizeof(tracks[0]), compare_tracks);
    return track_count > 0;
}

static const char *track_name(int i, void *data, char *buf, size_t len)
{
    (void)data;
    int s = tracks[i].length / 1000;
    snprintf(buf, len, "%s\t%d:%02d", track_text + tracks[i].title,
             s / 60, s % 60);
    return buf;
}

static bool tracks_header(void *data, const fb_data **thumb,
                          char *line2, size_t len)
{
    const struct album *al = data;
    char path[MAX_PATH], artist[64], year[16] = "";
    int y;
    if (!tagtree_album_info(al->seek, path, sizeof(path),
                            artist, sizeof(artist), &y))
        return false;
    if (y > 0)
        snprintf(year, sizeof(year), "%d \xc2\xb7 ", y);
    snprintf(line2, len, "%s\t%s%d %s", artist, year, track_count,
             track_count == 1 ? "song" : "songs");
    *thumb = ui_thumb_get(path);
    return true;
}

/* replaces the playlist with the album and plays it from song 'first' */
static bool play_album(int first)
{
    if (!warn_on_pl_erase() || playlist_create(NULL, NULL) < 0)
        return false;
    for (int i = 0; i < track_count; i++)
        if (playlist_insert_track(NULL, track_text + tracks[i].path,
                                  PLAYLIST_INSERT_LAST, false, false) < 0)
            break;
    playlist_sync(NULL);
    if (global_settings.playlist_shuffle)
        first = playlist_shuffle(current_tick, first);
    playlist_start(first, 0, 0);
    return true;
}

/* GO_TO_WPS once a song was started, MENU_ATTACHED_USB, or GO_TO_ROOT
 * for Back */
static int album_songs(int a)
{
    if (!load_tracks(albums[a].seek))
    {
        splash(HZ, ID2P(LANG_NO_FILES));
        return GO_TO_ROOT;
    }

    struct gui_synclist list;
    gui_synclist_init(&list, track_name, &albums[a], false, 1, NULL);
    gui_synclist_set_title(&list, names + albums[a].name, NOICON);
    gui_synclist_set_header_callback(&list, tracks_header);
    gui_synclist_set_nb_items(&list, track_count);
    gui_synclist_draw(&list);

    while (true)
    {
        int action;
        if (list_do_action(CONTEXT_LIST, HZ, &list, &action))
            continue;
        switch (action)
        {
            case ACTION_STD_OK:
                if (play_album(gui_synclist_get_sel_pos(&list)))
                    return GO_TO_WPS;
                gui_synclist_draw(&list);
                break;
            case ACTION_STD_CANCEL:
                return GO_TO_ROOT;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                    return MENU_ATTACHED_USB;
                break;
        }
    }
}

/* --- the covers --- */

int ui_coverflow(void)
{
    if (!root_menu_wait_for_database())
        return GO_TO_ROOT;
    if (!load_albums() || album_count == 0)
    {
        splash(HZ * 2, ID2P(LANG_NO_FILES));
        return GO_TO_ROOT;
    }
    make_placeholder();
    ui_status_title(str(LANG_NEWUI_COVERFLOW));
    fb_data bg = FB_SCALARPACK(UI_COL_BG);
    bg_r = bg >> 11;
    bg_g = (bg >> 5) & 63;
    bg_b = bg & 31;

    struct viewport vp;
    viewport_set_defaults(&vp, SCREEN_MAIN);
    if (current >= album_count)
        current = album_count - 1;
    int pos = current * 256;    /* shown position, 1/256 album */
    uint32_t last_us = ui_time_us();
    bool redraw = true, work = true;

    while (true)
    {
        int goal = current * 256;
        bool moving = pos != goal;
        if (moving)
        {
            /* ease towards the goal, by time so the speed does not
               depend on the frame rate */
            uint32_t now = ui_time_us();
            int ms = MIN((now - last_us) / 1000, 50);
            last_us = now;
            int diff = goal - pos;
            int step = diff * ms / 70;
            int min_step = 256 * ms / 400 + 1;
            if (abs(step) < min_step)
                step = diff > 0 ? MIN(min_step, diff) : MAX(-min_step, diff);
            pos += step;
            redraw = true;
            load_saved((pos + 128) >> 8, LOADS_MOVING);
        }
        else if (load_saved(current, 2 * CF_VISIBLE + 1))
            redraw = true;
        if (redraw)
        {
            draw(&vp, pos);
            redraw = false;
        }

        bool busy = pos != goal;
        int action = get_action(CONTEXT_STD,
                                busy || work ? TIMEOUT_NOBLOCK : HZ);
        switch (action)
        {
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
            {
                int dir = action == ACTION_STD_NEXT ||
                          action == ACTION_STD_NEXTREPEAT ? 1 : -1;
                if (!moving)
                    last_us = ui_time_us();
                current += dir * ui_wheel_step(dir, album_count);
                current = MAX(0, MIN(current, album_count - 1));
                bg_next = 0;
                work = true;
                break;
            }
            case ACTION_STD_OK:
            {
                ui_transition(1);
                int ret = album_songs(current);
                if (ret != GO_TO_ROOT)
                    return ret;
                ui_transition(-1);
                redraw = work = true;
                break;
            }
            case ACTION_STD_CANCEL:
                return GO_TO_ROOT;
            case ACTION_NONE:
                if (busy)
                    yield();
                else if (work)
                    work = idle_work();
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                    return MENU_ATTACHED_USB;
                break;
        }
    }
}
