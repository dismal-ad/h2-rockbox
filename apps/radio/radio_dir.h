/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
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

/* Station lists: the Radio Browser directory (radio-browser.info),
 * favourites, and the user's .m3u/.pls files */

#ifndef RADIO_DIR_H
#define RADIO_DIR_H

#include "radio.h"

#define RADIO_FAV_FILE      "/.rockbox/radio/favourites.m3u"
#define RADIO_LAST_FILE     "/.rockbox/radio/last.m3u"
#define RADIO_FILES_DIR     "/Radio"
#define RADIO_MAX_STATIONS  100

/* A category: a country or a genre */
struct radio_category {
    char name[48];
    char code[48];          /* country code or tag */
    int count;
};

/* Directory requests; < 0 on errors (err says why) */
int radio_dir_search(const char *name, struct radio_station *out, int max,
                     char *err, size_t errlen);
int radio_dir_popular(struct radio_station *out, int max,
                      char *err, size_t errlen);
int radio_dir_by_country(const char *code, struct radio_station *out, int max,
                         char *err, size_t errlen);
int radio_dir_by_tag(const char *tag, struct radio_station *out, int max,
                     char *err, size_t errlen);
int radio_dir_countries(struct radio_category *out, int max,
                        char *err, size_t errlen);
int radio_dir_tags(struct radio_category *out, int max,
                   char *err, size_t errlen);

/* Favourites */
int radio_fav_load(struct radio_station *out, int max);
bool radio_fav_has(const struct radio_station *st);
void radio_fav_add(const struct radio_station *st);
void radio_fav_remove(const struct radio_station *st);

/* The last station that played, for Resume Playback */
void radio_last_save(const struct radio_station *st);
bool radio_last_load(struct radio_station *out);

/* .m3u/.pls station files: the names in RADIO_FILES_DIR, then their
 * stations */
int radio_files_list(char names[][64], int max);
int radio_file_load(const char *name, struct radio_station *out, int max);

#endif /* RADIO_DIR_H */
