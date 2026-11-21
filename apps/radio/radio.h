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

/* Internet radio: plays an HTTP(S) stream through the normal playback by
 * serving it as a file under /.radio/ */

#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stddef.h>
#include "net/http.h"

#define RADIO_NAME_LEN 96

struct radio_station {
    char name[RADIO_NAME_LEN];
    char url[HTTP_URL_MAX];
    char codec[12];         /* "MP3", "AAC", ... from the directory */
    int bitrate;            /* kbit/s, 0 unknown */
    char country[4];        /* ISO 3166-1 */
    char tags[48];
};

enum radio_state {
    RADIO_IDLE,
    RADIO_NETWORK,          /* waiting for Wi-Fi */
    RADIO_CONNECTING,
    RADIO_BUFFERING,
    RADIO_PLAYING,          /* playback has the stream */
    RADIO_FAILED,
};

void radio_init(void);

/* Starts a station: stops what plays, connects, and starts playback once
 * enough is buffered. Returns at once; follow radio_get_state(). */
void radio_start(const struct radio_station *st);
/* Stops the stream (and playback, if it plays the stream) */
void radio_stop(void);
/* After radio_start: hands the stream to playback when it's ready.
 * Returns true once playback started, false while waiting or on failure. */
bool radio_poll_start(void);

enum radio_state radio_get_state(void);
/* What the radio is doing, or why it failed */
const char *radio_status_text(char *buf, size_t len);

/* The station playing (or last started), NULL if none */
const struct radio_station *radio_current(void);
/* The track playing is the stream */
bool radio_is_path(const char *path);
/* The stream's current title ("Artist - Title"), "" if none */
const char *radio_now_title(void);
/* Changes whenever the title changes */
unsigned radio_title_gen(void);
/* "MP3 128 kbps" */
const char *radio_format_text(char *buf, size_t len);

#endif /* RADIO_H */
