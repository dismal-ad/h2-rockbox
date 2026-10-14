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
#ifndef PCM_PLAYBACK_H
#define PCM_PLAYBACK_H

#include <string.h> /* size_t */
#include <inttypes.h> /* uint32_t */
#include <stdbool.h>
#include "config.h"

enum pcm_dma_status
{
    PCM_DMAST_ERR_DMA   = -1,
    PCM_DMAST_OK        =  0,
    PCM_DMAST_STARTED   =  1,
};

/** RAW PCM routines used with playback and recording **/

/* Typedef for registered data callback */
typedef void (*pcm_play_callback_type)(const void **start, size_t *size);

/* Typedef for registered status callback */
typedef enum pcm_dma_status (*pcm_status_callback_type)(enum pcm_dma_status status);

/* set the pcm frequency - use values in hw_sampr_list
 * when CONFIG_SAMPR_TYPES is #defined, or-in SAMPR_TYPE_* fields with
 * frequency value. SAMPR_TYPE_PLAY is 0 and the default if none is
 * specified. */

/* Reenterable locks for locking and unlocking the playback interrupt */
void pcm_play_lock(void);
void pcm_play_unlock(void);

void pcm_init(void) INIT_ATTR;
void pcm_postinit(void);
bool pcm_is_initialized(void);

enum pcm_sink_ids pcm_current_sink(void);
const struct pcm_sink_caps* pcm_sink_caps(enum pcm_sink_ids sink);
bool pcm_switch_sink(enum pcm_sink_ids sink);

/* shortcut for plugins */
const struct pcm_sink_caps* pcm_current_sink_caps(void);

/* Kept internally for global PCM and used by mixer's verion of peak
   calculation */
struct pcm_peaks
{
    uint32_t left;  /* Left peak value */
    uint32_t right; /* Right peak value */
    long period;    /* For tracking calling period */
    long tick;      /* Last tick called */
};

#endif /* PCM_PLAYBACK_H */
