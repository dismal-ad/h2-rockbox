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

/* Album covers at fixed small sizes, keyed by a track of the album:
 * UI_THUMB for list rows and UI_COVER for Cover Flow. Decoded once: kept
 * in RAM (the most recently used few) and on the card in THUMB_DIR, as raw
 * native pixels, or a 1-byte file when the track has no art. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "dir.h"
#include "lcd.h"
#include "metadata.h"
#include "rbpaths.h"
#include "misc.h"
#include "ui.h"

#define THUMB_DIR   ROCKBOX_DIR "/newui/thumbs"
#define THUMB_RAM   24
#define COVER_RAM   12

struct cache {
    int size, slots;
    uint32_t *key;      /* 0: unused */
    unsigned *age;
    bool *has_art;
    fb_data *px;        /* slots * size * size */
};

#define CACHE(name, sz, n)                                              \
    static uint32_t name##_key[n];                                      \
    static unsigned name##_age[n];                                      \
    static bool name##_art[n];                                          \
    static fb_data name##_px[(n) * (sz) * (sz)];                        \
    static struct cache name = { sz, n, name##_key, name##_age,         \
                                 name##_art, name##_px }

CACHE(thumbs, UI_THUMB, THUMB_RAM);
CACHE(covers, UI_COVER, COVER_RAM);
static unsigned thumb_clock;

static uint32_t key_of(const char *path)
{
    uint32_t h = 5381;
    while (*path)
        h = h * 33 + (unsigned char)*path++;
    return h ? h : 1;
}

static void file_of(const struct cache *c, uint32_t key, char *buf, size_t len)
{
    snprintf(buf, len, THUMB_DIR "/%08lx.t%d", (unsigned long)key, c->size);
}

static size_t bytes(const struct cache *c)
{
    return c->size * c->size * sizeof(fb_data);
}

static fb_data *pixels(const struct cache *c, int slot)
{
    return c->px + slot * c->size * c->size;
}

/* from the card; true if found (art or "no art") */
static bool load(const struct cache *c, uint32_t key, bool *has_art, fb_data *px)
{
    char name[MAX_PATH];
    file_of(c, key, name, sizeof(name));
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return false;
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    bool ok = false;
    if (size == 1)
    {
        *has_art = false;
        ok = true;
    }
    else if (size == (off_t)bytes(c))
    {
        ok = read(fd, px, bytes(c)) == (ssize_t)bytes(c);
        *has_art = ok;
    }
    close(fd);
    return ok;
}

static void store(const struct cache *c, uint32_t key, bool has_art,
                  const fb_data *px)
{
    char name[MAX_PATH];
    if (!dir_exists(THUMB_DIR))
    {
        mkdir(ROCKBOX_DIR "/newui");
        mkdir(THUMB_DIR);
    }
    file_of(c, key, name, sizeof(name));
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (has_art)
        write(fd, px, bytes(c));
    else
        write(fd, "", 1);
    close(fd);
}

/* decode the track's cover at the cache's size */
static bool generate(const struct cache *c, const char *path, fb_data *px)
{
    static struct mp3entry id3;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    bool ok = get_metadata(&id3, fd, path);
    close(fd);
    return ok && ui_art_decode_to(&id3, c->size, px);
}

/* the RAM slot holding key, or -1 */
static int find(struct cache *c, uint32_t key)
{
    for (int i = 0; i < c->slots; i++)
        if (c->key[i] == key)
        {
            c->age[i] = ++thumb_clock;
            return i;
        }
    return -1;
}

/* loads key into the least recently used slot, from the card or decoded */
static int fill(struct cache *c, uint32_t key, const char *path)
{
    int slot = 0;
    for (int i = 1; i < c->slots; i++)
        if (c->age[i] < c->age[slot])
            slot = i;

    bool has_art;
    if (!load(c, key, &has_art, pixels(c, slot)))
    {
        has_art = generate(c, path, pixels(c, slot));
        store(c, key, has_art, pixels(c, slot));
    }
    c->key[slot] = key;
    c->age[slot] = ++thumb_clock;
    c->has_art[slot] = has_art;
    return slot;
}

static const fb_data *get(struct cache *c, const char *path, bool *known)
{
    uint32_t key = key_of(path);
    int slot = find(c, key);
    if (slot < 0)
    {
        if (!known)
            slot = fill(c, key, path);
        else
        {
            *known = false;
            return NULL;
        }
    }
    if (known)
        *known = true;
    return c->has_art[slot] ? pixels(c, slot) : NULL;
}

const fb_data *ui_thumb_get(const char *track_path)
{
    return get(&thumbs, track_path, NULL);
}

const fb_data *ui_cover_peek(const char *track_path, bool *known)
{
    return get(&covers, track_path, known);
}

void ui_cover_load(const char *track_path)
{
    get(&covers, track_path, NULL);
}
