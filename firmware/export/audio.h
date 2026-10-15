/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
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
#ifndef __AUDIO_H
#define __AUDIO_H

#include <stdbool.h>
#include <string.h> /* size_t */
#include "config.h"
/* These must always be included with audio.h for this to compile under
   cetain conditions. Do it here or else spread the complication around to
   many files. */
#include "pcm_sampr.h"
#include "pcm.h"

#define AUDIO_STATUS_PLAY       0x0001
#define AUDIO_STATUS_PAUSE      0x0002
#define AUDIO_STATUS_RECORD     0x0004
#define AUDIO_STATUS_PRERECORD  0x0008
#define AUDIO_STATUS_ERROR      0x0010
#define AUDIO_STATUS_WARNING    0x0020

#define AUDIOERR_DISK_FULL      1

#define AUDIO_GAIN_LINEIN       0
#define AUDIO_GAIN_MIC          1


void audio_init(void) INIT_ATTR;
void audio_play(unsigned long elapsed, unsigned long offset);
void audio_stop(void);
/* Stops audio from serving playback and frees resources*/
void audio_hard_stop(void);
void audio_pause(void);
void audio_resume(void);
void audio_next(void);
void audio_prev(void);
int audio_status(void);
/* size of the audio buffer */
size_t audio_buffer_size(void);
/* size of the buffer available for allocating memory from the audio buffer using core_*()
 * returns core_available() if audio buffer is not allocated yet */
size_t audio_buffer_available(void);
void audio_pre_ff_rewind(void);
void audio_ff_rewind(long newpos);
void audio_flush_and_reload_tracks(void);
struct mp3entry* audio_current_track(void);
struct mp3entry* audio_next_track(void);
bool audio_peek_track(struct mp3entry* id3, int offset);
void audio_error_clear(void);
int audio_get_file_pos(void);
void audio_beep(int duration);

void audio_next_dir(void);
void audio_prev_dir(void);

/* channel modes */
enum rec_channel_modes
{
    __CHN_MODE_START_INDEX = -1,

    CHN_MODE_STEREO,
    CHN_MODE_MONO,

    CHN_NUM_MODES
};

/* channel mode capability bits */
#define CHN_CAP_STEREO  (1 << CHN_MODE_STEREO)
#define CHN_CAP_MONO    (1 << CHN_MODE_MONO)
#define CHN_CAP_ALL     (CHN_CAP_STEREO | CHN_CAP_MONO)

/* the enums below must match prestr[] in recording.c */
enum audio_sources
{
    AUDIO_SRC_PLAYBACK = -1, /* Virtual source */
    HAVE_MIC_IN_(AUDIO_SRC_MIC,)
    HAVE_LINE_IN_(AUDIO_SRC_LINEIN,)
    HAVE_SPDIF_IN_(AUDIO_SRC_SPDIF,)
    HAVE_FMRADIO_IN_(AUDIO_SRC_FMRADIO,)
    AUDIO_NUM_SOURCES,
    AUDIO_SRC_MAX = AUDIO_NUM_SOURCES-1,
    AUDIO_SRC_DEFAULT = AUDIO_SRC_PLAYBACK
};

extern int audio_channels;
extern int audio_output_source;

/* selects a source to monitor for recording or playback */
#define SRCF_PLAYBACK         0x0000    /* default */
#define SRCF_RECORDING        0x1000

/***********************************************************************/
/* audio event handling */
enum track_event_flags
{
    TEF_NONE      = 0x0,  /* no flags are set */
    TEF_CURRENT   = 0x1,  /* event is for the current track */
    TEF_AUTO_SKIP = 0x2,  /* event is sent in context of auto skip */
    TEF_REWIND    = 0x4,  /* interpret as rewind, id3->elapsed is the
                             position before the seek back to 0 */
};

struct track_event
{
    unsigned int flags;   /* combo of enum track_event_flags values */
    struct mp3entry *id3; /* pointer to mp3entry describing track */
};

#endif /* __AUDIO_H */
