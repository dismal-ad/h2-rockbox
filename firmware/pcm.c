/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 by Michael Sevakis
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
#include <stdlib.h>
#include "system.h"
#include "kernel.h"
#include "panic.h"

/* Define LOGF_ENABLE to enable logf output in this file */
//#define LOGF_ENABLE
#include "logf.h"
#include "audio.h"
#include "sound.h"
#include "general.h"
#include "pcm-internal.h"
#include "pcm_mixer.h"
#include "pcm_sw_volume.h"

/**
 * Aspects implemented in the target-specific portion:
 *
 * ==Playback==
 *   Public -
 *      pcm_postinit
 *      pcm_play_lock
 *      pcm_play_unlock
 *   Semi-private -
 *      pcm_play_dma_complete_callback
 *      pcm_play_dma_status_callback
 *      pcm_get_current_sink
 *      pcm_sink.init
 *      pcm_sink.postinit
 *      pcm_sink.play
 *      pcm_sink.stop
 *   Data Read/Written within TSP -
 *      pcm_playing (R)
 *
 * ==Playback/Recording==
 *   Public -
 *      pcm_dma_addr
 *   Semi-private -
 *      pcm_sink.set_freq
 *
 * ==Recording==
 *   Public -
 *      pcm_rec_lock
 *      pcm_rec_unlock
 *   Semi-private -
 *      pcm_rec_dma_complete_callback
 *      pcm_rec_dma_status_callback
 *      pcm_rec_dma_init
 *      pcm_rec_dma_close
 *      pcm_rec_dma_start
 *      pcm_rec_dma_stop
 *      pcm_rec_dma_get_peak_buffer
 *   Data Read/Written within TSP -
 *      pcm_recording (R)
 *
 * States are set _after_ the target's pcm driver is called so that it may
 * know from whence the state is changed. One exception is init.
 *
 */

/* 'true' when all stages of pcm initialization have completed */
static volatile bool pcm_is_ready[PCM_SINK_NUM] SHAREDBSS_ATTR = { false };

#ifdef HAVE_BLUETOOTH
extern struct pcm_sink bt_pcm_sink;
#endif

static struct pcm_sink* sinks[PCM_SINK_NUM] = {
    [PCM_SINK_BUILTIN] = &builtin_pcm_sink,
#ifdef HAVE_BLUETOOTH
    [PCM_SINK_BLUETOOTH] = &bt_pcm_sink,
#endif
};
static enum pcm_sink_ids cur_sink SHAREDBSS_ATTR = PCM_SINK_BUILTIN;

/* The registered callback function to ask for more mp3 data */
volatile pcm_play_callback_type
    pcm_callback_for_more SHAREDBSS_ATTR = NULL;
/* The registered callback function to inform of DMA status */
volatile pcm_status_callback_type
    pcm_play_status_callback SHAREDBSS_ATTR = NULL;
/* PCM playback state */
volatile bool pcm_playing SHAREDBSS_ATTR = false;

struct pcm_sink* pcm_get_current_sink(void)
{
    return sinks[cur_sink];
}

/** Standard hw volume/unbuffered control functions - otherwise, see
 ** pcm_sw_volume.c **/
static inline void pcm_play_dma_start_int(const void *addr, size_t size)
{
    if (sinks[cur_sink]->caps.volume_type == PCM_SINK_SWVOL) {
        pcm_play_dma_start_int_swvol(addr, size);
        return;
    }
    /* Smoothed transition might not have happened so sync now */
    pcm_sync_pcm_factors();
    sinks[cur_sink]->ops.play(addr, size);
}

static inline void pcm_play_dma_stop_int(void)
{
    if (sinks[cur_sink]->caps.volume_type == PCM_SINK_SWVOL) {
        pcm_play_dma_stop_int_swvol();
        return;
    }
    sinks[cur_sink]->ops.stop();
}

bool pcm_play_dma_complete_callback(enum pcm_dma_status status,
                                    const void **addr, size_t *size)
{
    if (sinks[cur_sink]->caps.volume_type == PCM_SINK_SWVOL) {
        return pcm_play_dma_complete_callback_swvol(status, addr, size);
    }
    /* Check status callback first if error */
    if (status < PCM_DMAST_OK)
        status = pcm_play_dma_status_callback(status);

    if (status >= PCM_DMAST_OK && pcm_get_more_int(addr, size)) {
        return true;
    }

    /* Error, callback missing or no more DMA to do */
    pcm_play_stop_int();
    return false;
}

void pcm_play_stop_int(void)
{
    pcm_play_dma_stop_int();
    pcm_callback_for_more = NULL;
    pcm_play_status_callback = NULL;
    pcm_playing = false;
}

static void pcm_wait_for_init(void)
{
    while (!pcm_is_ready[cur_sink])
        sleep(0);
}

/**
 * Perform peak calculation on a buffer of packed 16-bit samples.
 *
 * Used for recording and playback.
 */
static void pcm_peak_peeker(const int16_t *p, int count,
                            struct pcm_peaks *peaks)
{
    uint32_t peak_l = 0, peak_r = 0;
    const int16_t *pend = p + 2 * count;

    do
    {
        int32_t s;

        s = p[0];

        if (s < 0)
            s = -s;

        if ((uint32_t)s > peak_l)
            peak_l = s;

        s = p[1];

        if (s < 0)
            s = -s;

        if ((uint32_t)s > peak_r)
            peak_r = s;

        p += 4 * 2; /* Every 4th sample, interleaved */
    }
    while (p < pend);

    peaks->left = peak_l;
    peaks->right = peak_r;
}

void pcm_do_peak_calculation(struct pcm_peaks *peaks, bool active,
                             const void *addr, int count)
{
    long tick = current_tick;

    /* Peak no farther ahead than expected period to avoid overcalculation */
    long period = tick - peaks->tick;

    /* Keep reasonable limits on period */
    if (period < 1)
        period = 1;
    else if (period > HZ/5)
        period = HZ/5;

    peaks->period = (3*peaks->period + period) / 4;
    peaks->tick = tick;

    if (active)
    {
        struct pcm_sink* sink = sinks[cur_sink];
        if (sink->configured_freq == -1U)
        {
            logf("not configured yet");
            return;
        }

        unsigned long sampr = sink->caps.samprs[sink->configured_freq];
        int framecount = peaks->period * sampr / HZ;
        count = MIN(framecount, count);

        if (count > 0)
            pcm_peak_peeker(addr, count, peaks);
        /* else keep previous peak values */
    }
    else
    {
        /* peaks are zero */
        peaks->left = peaks->right = 0;
    }
}

bool pcm_is_playing(void)
{
    return pcm_playing;
}

/****************************************************************************
 * Functions that do not require targeted implementation but only a targeted
 * interface
 */

void pcm_play_lock(void) {
    sinks[cur_sink]->ops.lock();
}

void pcm_play_unlock(void) {
    sinks[cur_sink]->ops.unlock();
}

/* This should only be called at startup before any audio playback or
   recording is attempted */
void pcm_init(void)
{
    logf("pcm_init");

    for(size_t i = 0; i < PCM_SINK_NUM; i += 1) {
        struct pcm_sink* sink = sinks[i];
        pcm_is_ready[i] = false;
        sink->pending_freq = sink->caps.default_freq;
        sink->configured_freq = -1U;
        sink->ops.init();
    }
}

/* Finish delayed init */
void pcm_postinit(void)
{
    logf("pcm_postinit");

    for(size_t i = 0; i < PCM_SINK_NUM; i += 1) {
        struct pcm_sink* sink = sinks[i];
        sink->ops.postinit();
        pcm_is_ready[i] = true;
    }

    /* Ensure mixer is in a sane state */
    mixer_set_frequency(pcm_get_frequency());
}

bool pcm_is_initialized(void)
{
    return pcm_is_ready[cur_sink];
}

enum pcm_sink_ids pcm_current_sink(void)
{
    return cur_sink;
}

const struct pcm_sink_caps* pcm_sink_caps(enum pcm_sink_ids sink)
{
    return &sinks[sink]->caps;
}

const struct pcm_sink_caps* pcm_current_sink_caps(void)
{
    return pcm_sink_caps(pcm_current_sink());
}

bool pcm_switch_sink(enum pcm_sink_ids sink)
{
    logf("pcm_switch_sink %d to %d", cur_sink, sink);
    if(sink >= PCM_SINK_NUM) {
        return false;
    }

    if(cur_sink == sink) {
        return true;
    }

    /*
     * If PCM_SINK_NUM == 1, GCC 9.5 can infer that cur_sink
     * must be nonzero here (because of the above checks) and
     * issue a -Warray-bounds warning. This only happens on
     * some architectures (ARM), and oddly enough, only when
     * cur_sink is an enum type.
     *
     * Since this situation isn't possible outside of memory
     * corruption we can just tell the compiler to assume it
     * can't happen. This avoids the warning, and saves a bit
     * of code size since none of the code below is reachable
     * when there's only one PCM sink.
     */
    ASSUME(cur_sink < PCM_SINK_NUM);

    /* save current sink before switching */
    struct pcm_sink* old_sink = sinks[cur_sink];

    /* update sink index */
    cur_sink = sink;
    /* synchronize frequency */
    unsigned long cur_sampr = old_sink->caps.samprs[old_sink->pending_freq];
    pcm_set_frequency(cur_sampr);
    pcm_apply_settings();
    /* when playing, continue playing on new sink */
    if(pcm_playing) {
        old_sink->ops.stop();
        /* need more */
        const void *start;
        size_t size;
        if(pcm_get_more_int(&start, &size)) {
            pcm_play_dma_start_int(start, size);
        } else {
            pcm_play_stop_int();
        }
    }

    return true;
}

void pcm_play_data(pcm_play_callback_type get_more,
                   pcm_status_callback_type status_cb,
                   const void *start, size_t size)
{
    logf("pcm_play_data");

    pcm_play_lock();

    pcm_callback_for_more = get_more;
    pcm_play_status_callback = status_cb;

    ALIGN_AUDIOBUF(start, size);
    if ((start && size) || pcm_get_more_int(&start, &size))
    {
        pcm_apply_settings();
        logf(" pcm_play_dma_start_int");
        pcm_play_dma_start_int(start, size);
        pcm_playing = true;
    }
    else
    {
        /* Force a stop */
        logf(" pcm_play_stop_int");
        pcm_play_stop_int();
    }

    pcm_play_unlock();
}

void pcm_play_stop(void)
{
    logf("pcm_play_stop");

    pcm_play_lock();

    if (pcm_playing)
    {
        logf(" pcm_play_stop_int");
        pcm_play_stop_int();
    }

    pcm_play_unlock();
}

/**/

/* set frequency next frequency used by the audio hardware -
 * what pcm_apply_settings will set */
void pcm_set_frequency(unsigned int samplerate)
{
    logf("pcm_set_frequency %u", samplerate);

    int index;

    struct pcm_sink* sink = sinks[cur_sink];
    index = round_value_to_list32(samplerate, sink->caps.samprs, sink->caps.num_samprs, false);

    if (samplerate != sink->caps.samprs[index])
        index = sink->caps.default_freq; /* Invalid = default */

    sink->pending_freq = index;
}

/* return last-set frequency */
unsigned int pcm_get_frequency(void)
{
    struct pcm_sink* sink = sinks[cur_sink];
    return sink->caps.samprs[sink->pending_freq];
}

/* apply pcm settings to the hardware */
void pcm_apply_settings(void)
{
    logf("pcm_apply_settings");

    pcm_wait_for_init();

    struct pcm_sink* sink = sinks[cur_sink];
    if(sink->pending_freq != sink->configured_freq) {
        logf(" sink->set_freq");
        sink->ops.set_freq(sink->pending_freq);
        sink->configured_freq = sink->pending_freq;
    }
}

