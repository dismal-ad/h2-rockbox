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
    ui_fill_round_rect(x, y, size, size, radius, UI_COL_TRACK);
    /* a quaver: head, stem and flag, scaled to the square */
    int u = size / 16;
    int hx = x + size / 2 - 3 * u, hy = y + size / 2 + 2 * u;
    ui_fill_round_rect(hx, hy, 4 * u, 3 * u, (3 * u) / 2, UI_COL_MUTED);
    ui_fill_rect(hx + 3 * u, y + size / 2 - 4 * u, u, 7 * u, UI_COL_MUTED);
    ui_fill_rect(hx + 3 * u, y + size / 2 - 4 * u, 3 * u, u, UI_COL_MUTED);
}
