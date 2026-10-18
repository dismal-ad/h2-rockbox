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

/* Album art for the new UI: the current track's cover, decoded once at the
 * size asked for and kept until the track or size changes. Embedded JPEG
 * art is tried first, then cover files next to the track (find_albumart),
 * the same sources playback uses. */

#include "config.h"
#include <string.h>
#include "file.h"
#include "lcd.h"
#include "metadata.h"
#include "audio.h"
#include "albumart.h"
#include "jpeg_load.h"
#include "bmp.h"
#include "string-extra.h"
#include "ui.h"

/* decoded bitmap plus the JPEG decoder's working memory */
#define ART_BUF_SIZE (UI_ART_MAX * UI_ART_MAX * sizeof(fb_data) + 64 * 1024)

static unsigned char art_buf[ART_BUF_SIZE] __attribute__((aligned(4)));
static struct bitmap art_bm;
static bool art_valid;
static char art_key[MAX_PATH];
static int art_size;

static bool decode(const struct mp3entry *id3, int size)
{
    const int format = FORMAT_NATIVE | FORMAT_DITHER |
                       FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int rc = -1;

    art_bm.width = size;
    art_bm.height = size;
    art_bm.data = art_buf;
    art_bm.maskdata = NULL;

    if (id3->has_embedded_albumart &&
        (id3->albumart.type & AA_CLEAR_FLAGS_MASK) == AA_TYPE_JPG)
    {
        int fd = open(id3->path, O_RDONLY);
        if (fd >= 0)
        {
            lseek(fd, id3->albumart.pos, SEEK_SET);
            rc = clip_jpeg_fd(fd, id3->albumart.type, id3->albumart.size,
                              &art_bm, sizeof(art_buf), format, NULL);
            close(fd);
        }
    }

    if (rc <= 0)
    {
        char path[MAX_PATH];
        struct dim dim = { .width = size, .height = size };
        if (find_albumart(id3, path, sizeof(path), &dim))
        {
            art_bm.width = size;
            art_bm.height = size;
            size_t len = strlen(path);
            if (len > 4 && !strcasecmp(path + len - 4, ".bmp"))
                rc = read_bmp_file(path, &art_bm, sizeof(art_buf), format, NULL);
            else
                rc = read_jpeg_file(path, &art_bm, sizeof(art_buf), format, NULL);
        }
    }
    return rc > 0;
}

const struct bitmap *ui_art_current(int size)
{
    struct mp3entry *id3 = audio_status() ? audio_current_track() : NULL;
    if (!id3 || !id3->path[0])
        return NULL;
    if (size > UI_ART_MAX)
        size = UI_ART_MAX;

    if (size != art_size || strcmp(art_key, id3->path))
    {
        strmemccpy(art_key, id3->path, sizeof(art_key));
        art_size = size;
        art_valid = decode(id3, size);
    }
    return art_valid ? &art_bm : NULL;
}

bool ui_art_decode_to(const struct mp3entry *id3, int size, fb_data *out)
{
    /* shares the decode buffer with the current track's art */
    art_key[0] = '\0';
    art_valid = false;
    if (size > UI_ART_MAX || !decode(id3, size))
        return false;
    for (int i = 0; i < size * size; i++)
        out[i] = FB_SCALARPACK(UI_COL_BG);
    int ox = (size - art_bm.width) / 2, oy = (size - art_bm.height) / 2;
    const fb_data *src = (const fb_data *)art_bm.data;
    for (int y = 0; y < art_bm.height; y++)
        memcpy(&out[(oy + y) * size + ox], &src[y * art_bm.width],
               art_bm.width * sizeof(fb_data));
    return true;
}

void ui_art_draw(const struct bitmap *bm, int x, int y, int size, int radius)
{
    /* centre the (aspect-kept) image in the size x size square */
    int bx = x + (size - bm->width) / 2, by = y + (size - bm->height) / 2;
    lcd_bitmap((const fb_data *)bm->data, bx, by, bm->width, bm->height);
    if (radius)
        ui_round_corners(bx, by, bm->width, bm->height, radius, UI_COL_BG);
}

void ui_art_placeholder(int x, int y, int size, int radius)
{
    ui_fill_round_rect(x, y, size, size, radius, UI_COL_PH_BG);
    /* a quaver: head, stem and flag, scaled to the square */
    int u = size / 16;
    int hx = x + size / 2 - 3 * u, hy = y + size / 2 + 2 * u;
    ui_fill_round_rect(hx, hy, 4 * u, 3 * u, (3 * u) / 2, UI_COL_PH_FG);
    ui_fill_rect(hx + 3 * u, y + size / 2 - 4 * u, u, 7 * u, UI_COL_PH_FG);
    ui_fill_rect(hx + 3 * u, y + size / 2 - 4 * u, 3 * u, u, UI_COL_PH_FG);
}

/* blends c toward bg, keeping a/256 of c */
static inline unsigned mix565(unsigned c, unsigned bg, int a)
{
    int r = RGB_UNPACK_RED(bg), g = RGB_UNPACK_GREEN(bg), b = RGB_UNPACK_BLUE(bg);
    return LCD_RGBPACK(r + ((RGB_UNPACK_RED(c) - r) * a >> 8),
                       g + ((RGB_UNPACK_GREEN(c) - g) * a >> 8),
                       b + ((RGB_UNPACK_BLUE(c) - b) * a >> 8));
}

void ui_art_draw_tilted(const struct bitmap *bm, int x, int y, int w,
                        int hl, int hr, int refl_h, int refl_a)
{
    struct viewport *vp = lcd_current_viewport;
    const fb_data *src = (const fb_data *)bm->data;
    const int bw = bm->width, bh = bm->height;
    const unsigned bg = UI_COL_BG;
    const int hmax = MAX(hl, hr), yc = y + hmax / 2;

    for (int i = 0; i < w; i++)
    {
        int px = x + i;
        if (px < 0 || px >= vp->width)
            continue;
        /* the column's height, and its texture column interpolated in
           1/depth so the turn looks right */
        long t = ((long)i << 16) / w;
        int h = hl + (int)(((long)(hr - hl) * t) >> 16);
        long long den = (long long)(65536 - t) * hl + (long long)t * hr;
        int u = den ? (int)((long long)t * hr * bw / den) : 0;
        if (u >= bw)
            u = bw - 1;
        const int step = (bh << 16) / h;
        int top = yc - h / 2, v = 0;
        for (int r = 0; r < h; r++, v += step)
        {
            int py = top + r;
            if (py >= 0 && py < vp->height)
                *FBADDR(vp->x + px, vp->y + py) = src[(v >> 16) * bw + u];
        }
        /* the reflection: the cover upside down below it, fading out */
        int rh = MIN(refl_h, h);
        v = (bh << 16) - 1;
        for (int r = 0; r < rh; r++, v -= step)
        {
            int py = top + h + 1 + r;
            if (py < 0 || py >= vp->height || v < 0)
                continue;
            unsigned c = FB_UNPACK_SCALAR_LCD(src[(v >> 16) * bw + u]);
            *FBADDR(vp->x + px, vp->y + py) =
                FB_SCALARPACK(mix565(c, bg, refl_a * (rh - r) / rh));
        }
    }
}
