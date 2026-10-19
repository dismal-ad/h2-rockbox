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

/* Home's cover slideshow (when nothing plays): random album covers from
 * the database, zoomed in and panning slowly in a random direction, a new
 * one every SLIDE_MS with a crossfade. Covers are decoded at SLIDE_SRC into
 * one of two buffers (the one showing and the next); the next is decoded
 * just after a crossfade, the clock held meanwhile so the pan does not
 * jump. Drawn with bilinear sampling, so the slow pan is smooth. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "metadata.h"
#include "tagcache.h"
#include "tagtree.h"
#include "ui.h"

#define SLIDE_SRC     UI_ART_MAX       /* decoded size */
#define SLIDE_MS      5000             /* each cover */
#define FADE_MS       700              /* the crossfade at its end */
#define ZOOM_256      333              /* 1.3x */
#define PAN_PCT       61               /* of the range the zoom leaves: the speed */
#define MAX_SLIDE_ALBUMS 1024
#define TRIES         6                /* albums tried for one with a cover */

struct slide {
    bool ok;
    int x0, y0, x1, y1;                /* window's top left, start and end, 16.16 */
    fb_data px[SLIDE_SRC * SLIDE_SRC];
};
static struct slide slides[2];
static int cur;                        /* the slide showing */
static bool next_ready;
static uint32_t start_us;              /* when the showing slide started */
static int32_t seeks[MAX_SLIDE_ALBUMS];
static int nseeks = -1;                /* -1: not looked up yet */

static void find_albums(void)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    if (!tagcache_is_usable())
        return;     /* not yet (boot): looked up again later */
    nseeks = 0;
    if (!tagcache_search(&tcs, tag_album))
        return;
    while (nseeks < MAX_SLIDE_ALBUMS && tagcache_get_next(&tcs, buf, sizeof(buf)))
        if (strcmp(buf, UNTAGGED))
            seeks[nseeks++] = tcs.result_seek;
    tagcache_search_finish(&tcs);
}

/* the window (w x h in source pixels) pans along a random one of eight
   directions, across the range the zoom leaves */
static void pick_path(struct slide *s, int w, int h)
{
    static const signed char dirs[8][2] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 1, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 },
    };
    const signed char *d = dirs[rand() % 8];
    int rx = (SLIDE_SRC - w) << 15, ry = (SLIDE_SRC - h) << 15; /* half ranges */
    int cx = rx, cy = ry;
    s->x0 = cx - d[0] * (rx / 100) * PAN_PCT;
    s->y0 = cy - d[1] * (ry / 100) * PAN_PCT;
    s->x1 = cx + d[0] * (rx / 100) * PAN_PCT;
    s->y1 = cy + d[1] * (ry / 100) * PAN_PCT;
}

static bool load(struct slide *s, int w, int h)
{
    static struct mp3entry id3;
    char path[MAX_PATH], artist[8];
    s->ok = false;
    for (int t = 0; t < TRIES && nseeks > 0; t++)
    {
        if (!tagtree_album_info(seeks[rand() % nseeks], path, sizeof(path),
                                artist, sizeof(artist), NULL))
            continue;
        int fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        bool ok = get_metadata(&id3, fd, path);
        close(fd);
        if (ok && ui_art_decode_to(&id3, SLIDE_SRC, s->px))
        {
            pick_path(s, w, h);
            s->ok = true;
            return true;
        }
    }
    return false;
}

bool ui_slides_available(void)
{
    if (nseeks < 0)
    {
        static bool seeded;
        if (!seeded)
            srand(current_tick);
        seeded = true;
        find_albums();
    }
    return nseeks > 0;
}

/* RGB565 bilinear sample at 16.16 source coordinates */
static inline unsigned sample(const fb_data *px, int sx, int sy)
{
    int x = sx >> 16, y = sy >> 16;
    int fx = (sx >> 8) & 255, fy = (sy >> 8) & 255;
    if (x >= SLIDE_SRC - 1) { x = SLIDE_SRC - 2; fx = 255; }
    if (y >= SLIDE_SRC - 1) { y = SLIDE_SRC - 2; fy = 255; }
    const fb_data *p = px + y * SLIDE_SRC + x;
    unsigned a = FB_UNPACK_SCALAR_LCD(p[0]), b = FB_UNPACK_SCALAR_LCD(p[1]);
    unsigned c = FB_UNPACK_SCALAR_LCD(p[SLIDE_SRC]), d = FB_UNPACK_SCALAR_LCD(p[SLIDE_SRC + 1]);
    int w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy);
    int w01 = (256 - fx) * fy, w11 = fx * fy;
    int r = (RGB_UNPACK_RED(a) * w00 + RGB_UNPACK_RED(b) * w10 +
             RGB_UNPACK_RED(c) * w01 + RGB_UNPACK_RED(d) * w11) >> 16;
    int g = (RGB_UNPACK_GREEN(a) * w00 + RGB_UNPACK_GREEN(b) * w10 +
             RGB_UNPACK_GREEN(c) * w01 + RGB_UNPACK_GREEN(d) * w11) >> 16;
    int bl = (RGB_UNPACK_BLUE(a) * w00 + RGB_UNPACK_BLUE(b) * w10 +
              RGB_UNPACK_BLUE(c) * w01 + RGB_UNPACK_BLUE(d) * w11) >> 16;
    return LCD_RGBPACK(r, g, bl);
}

static inline unsigned mix(unsigned a, unsigned b, int t) /* t/256 of b */
{
    return LCD_RGBPACK(
        RGB_UNPACK_RED(a) + ((RGB_UNPACK_RED(b) - RGB_UNPACK_RED(a)) * t >> 8),
        RGB_UNPACK_GREEN(a) + ((RGB_UNPACK_GREEN(b) - RGB_UNPACK_GREEN(a)) * t >> 8),
        RGB_UNPACK_BLUE(a) + ((RGB_UNPACK_BLUE(b) - RGB_UNPACK_BLUE(a)) * t >> 8));
}

/* the window's top left at t (0..65536 of the slide) */
static void window_at(const struct slide *s, int t, int *x, int *y)
{
    *x = s->x0 + (int)(((long long)(s->x1 - s->x0) * t) >> 16);
    *y = s->y0 + (int)(((long long)(s->y1 - s->y0) * t) >> 16);
}

bool ui_slides_draw(int w, int h)
{
    struct viewport *vp = lcd_current_viewport;
    const int sw = w * 256 / ZOOM_256, sh = h * 256 / ZOOM_256;
    const int step = (256 << 16) / ZOOM_256;

    if (!slides[cur].ok)
    {
        if (!load(&slides[cur], sw, sh))
            return false;
        start_us = ui_time_us();
        next_ready = false;
    }
    uint32_t now = ui_time_us();
    uint32_t ms = (now - start_us) / 1000;
    if (ms >= SLIDE_MS)
    {
        /* the crossfade is over: the next one shows. Its pan began with
           the fade, so its clock starts there (else it jumps back) */
        cur ^= 1;
        start_us += (SLIDE_MS - FADE_MS) * 1000;
        ms -= SLIDE_MS - FADE_MS;
        next_ready = false;
    }
    if (!next_ready && ms < SLIDE_MS - FADE_MS)
    {
        /* decode the next cover now, holding the clock meanwhile */
        uint32_t t0 = ui_time_us();
        next_ready = load(&slides[cur ^ 1], sw, sh);
        start_us += ui_time_us() - t0;
        now = ui_time_us();
        ms = (now - start_us) / 1000;
    }

    const struct slide *a = &slides[cur], *b = &slides[cur ^ 1];
    int fade = 0;
    if (next_ready && ms > SLIDE_MS - FADE_MS)
        fade = (ms - (SLIDE_MS - FADE_MS)) * 256 / FADE_MS;
    /* each slide pans over SLIDE_MS; the next starts its pan as it fades in */
    int ax, ay, bx = 0, by = 0;
    window_at(a, (int)((long long)ms * 65536 / SLIDE_MS), &ax, &ay);
    if (fade)
        window_at(b, (int)((long long)(ms - (SLIDE_MS - FADE_MS)) * 65536 / SLIDE_MS),
                  &bx, &by);

    for (int y = 0; y < h; y++)
    {
        fb_data *d = FBADDR(vp->x, vp->y + y);
        int say = ay + y * step, sby = by + y * step;
        int sax = ax, sbx = bx;
        for (int x = 0; x < w; x++, sax += step, sbx += step)
        {
            unsigned c = sample(a->px, sax, say);
            if (fade)
                c = mix(c, sample(b->px, sbx, sby), fade);
            d[x] = FB_SCALARPACK(c);
        }
    }
    return true;
}
