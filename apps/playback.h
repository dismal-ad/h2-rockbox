/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 Miika Pekkarinen
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

#ifndef _PLAYBACK_H
#define _PLAYBACK_H

#include <stdbool.h>
#include <stdlib.h>
#include "config.h"

/* Including the code for fast previews is entirely optional since it
   does add two more mp3entry's - for certain targets it may be less
   beneficial such as flash-only storage */
#if MEMORYSIZE > 2
#define AUDIO_FAST_SKIP_PREVIEW
#endif


/* Functions */
unsigned int audio_track_count(void);
long audio_filebufused(void);
void audio_pre_ff_rewind(void);
void audio_skip(int direction);

void audio_set_cuesheet(bool enable);
void audio_set_crossfade(int enable);
void audio_set_playback_frequency(unsigned int sample_rate_hz);

size_t audio_get_filebuflen(void);

unsigned int playback_status(void);

struct mp3entry* get_temp_mp3entry(struct mp3entry *free);

void allocate_playback_log(void);
void add_playbacklog(struct mp3entry *id3);

#endif /* _PLAYBACK_H */
