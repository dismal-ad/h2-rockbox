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

/* Small album covers for list rows (UI_THUMB x UI_THUMB), keyed by a track
 * of the album. Decoded once: kept in RAM (THUMB_RAM most recent) and on
 * the card in THUMB_DIR, as raw native pixels, or a 1-byte file when the
 * track has no art. */

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
#define THUMB_BYTES (UI_THUMB * UI_THUMB * sizeof(fb_data))

static struct {
    uint32_t key;       /* 0: unused */
    unsigned age;
    bool has_art;
    fb_data px[UI_THUMB * UI_THUMB];
} ram[THUMB_RAM];
static unsigned thumb_clock;

static uint32_t key_of(const char *path)
{
    uint32_t h = 5381;
    while (*path)
        h = h * 33 + (unsigned char)*path++;
    return h ? h : 1;
}

static void file_of(uint32_t key, char *buf, size_t len)
{
    snprintf(buf, len, THUMB_DIR "/%08lx.t%d", (unsigned long)key, UI_THUMB);
}

/* from the card; true if found (art or "no art") */
static bool load(uint32_t key, bool *has_art, fb_data *px)
{
    char name[MAX_PATH];
    file_of(key, name, sizeof(name));
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
    else if (size == (off_t)THUMB_BYTES)
    {
        ok = read(fd, px, THUMB_BYTES) == (ssize_t)THUMB_BYTES;
        *has_art = ok;
    }
    close(fd);
    return ok;
}

static void store(uint32_t key, bool has_art, const fb_data *px)
{
    char name[MAX_PATH];
    if (!dir_exists(THUMB_DIR))
    {
        mkdir(ROCKBOX_DIR "/newui");
        mkdir(THUMB_DIR);
    }
    file_of(key, name, sizeof(name));
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (has_art)
        write(fd, px, THUMB_BYTES);
    else
        write(fd, "", 1);
    close(fd);
}

/* decode the track's cover at thumbnail size */
static bool generate(const char *path, fb_data *px)
{
    static struct mp3entry id3;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    bool ok = get_metadata(&id3, fd, path);
    close(fd);
    return ok && ui_art_decode_to(&id3, UI_THUMB, px);
}

const fb_data *ui_thumb_get(const char *track_path)
{
    uint32_t key = key_of(track_path);
    int slot = 0;
    for (int i = 0; i < THUMB_RAM; i++)
    {
        if (ram[i].key == key)
        {
            ram[i].age = ++thumb_clock;
            return ram[i].has_art ? ram[i].px : NULL;
        }
        if (ram[i].age < ram[slot].age)
            slot = i;
    }

    bool has_art;
    if (!load(key, &has_art, ram[slot].px))
    {
        has_art = generate(track_path, ram[slot].px);
        store(key, has_art, ram[slot].px);
    }
    ram[slot].key = key;
    ram[slot].age = ++thumb_clock;
    ram[slot].has_art = has_art;
    return has_art ? ram[slot].px : NULL;
}
