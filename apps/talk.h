/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2004 Jörg Hohensohn
 *
 * This module collects the Talkbox and voice UI functions.
 * (Talkbox reads directory names from mp3 clips called thumbnails,
 *  the voice UI lets menus and screens "talk" from a voicefont in memory.
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

#ifndef __TALK_H__
#define __TALK_H__

/* Voice (spoken menus) support has been removed from this tree: the
 * talk API is kept as no-op stubs so callers compile unchanged. */

#include <stdbool.h>
#include <inttypes.h>
#include "time.h"

#define VOICE_VERSION 400 /* 4.00 - if you change this, change it in voicefont too */

enum {
    /* See array "unit_voiced" in talk.c function "talk_value" */
    UNIT_INT = 1, /* plain number */
    UNIT_SIGNED,  /* number with mandatory sign (even if positive) */
    UNIT_MS,      /* milliseconds */
    UNIT_SEC,     /* seconds */
    UNIT_MIN,     /* minutes */
    UNIT_HOUR,    /* hours */
    UNIT_KHZ,     /* kHz */
    UNIT_DB,      /* dB, mandatory sign */
    UNIT_PERCENT, /* % */
    UNIT_MAH,     /* milliAmp hours */
    UNIT_PIXEL,   /* pixels */
    UNIT_PER_SEC, /* per second */
    UNIT_HERTZ,   /* hertz */
    UNIT_MB,      /* Megabytes */
    UNIT_KBIT,    /* kilobits per sec */
    UNIT_PM_TICK, /* peak meter units per tick */
    UNIT_TIME,    /* time duration/interval in seconds, says hours,mins,secs */
    UNIT_DATEYEAR,/* for 1999 say nineteen ninety nine */
    UNIT_LAST     /* END MARKER */
};

/* Status of loading talk file, shown in debug_menu */
enum talk_status {
    TALK_STATUS_OK = 0,
    TALK_STATUS_ERR_OOM,
    TALK_STATUS_ERR_ALLOC,
    TALK_STATUS_ERR_NOFILE,
    TALK_STATUS_ERR_INCOMPATIBLE
};

enum talk_speakmode {
    /* voice mode: 0=off, 1=number, 2=spell */
    TALK_SPEAK_OFF = 0,
    TALK_SPEAK_NUMBER,
    TALK_SPEAK_SPELL
};

#define UNIT_SHIFT (32-5) /* this many bits left from UNIT_xx enum */

#define DECIMAL_SHIFT (32 - 8)

/* make a "talkable" ID from number + unit
   unit is upper 4 bits, number the remaining (in regular 2's complement) */
#define TALK_ID(n,u) (((long)(u))<<UNIT_SHIFT | ((n) & ~(((unsigned int)-1L)<<DECIMAL_SHIFT)))

/* make a "talkable" ID from a decimal number + unit, the decimal number
   is represented like x*10^d where d is the number of decimal digits */
#define TALK_ID_DECIMAL(n,d,u) (((long)(u))<<UNIT_SHIFT |\
                             ((long)(d))<<DECIMAL_SHIFT |\
                             ((n) & ~(((unsigned int)-1L)<<DECIMAL_SHIFT)))

/* convenience macro to have both virtual pointer and ID as arguments */
#define STR(id) ID2P(id), id

/* Policy values for how hard to try to keep the talk/voice buffers.
 * Affects how genereous talk.c is when it's asked for memory in
 * shrink_callbacks().
 *
 * I.e. setting the policy to TALK_BUFFER_LOOSE, it will happily give its
 * entire bufer away if asked for, e.g. due to a another module
 * calling core_alloc_maximum(), TALK_BUFFER_HOLD on the other hand will
 * make it keep the buffers so that a call to core_alloc_maximum() does not
 * stop the speech-interface.
 */
enum talk_buffer_policies {
    TALK_BUFFER_DEFAULT,
    TALK_BUFFER_LOOSE,
    TALK_BUFFER_HOLD,
};

/* This sets the actual policy. Call this before core_alloc_maximum() to
 * get the desired outcome */
static inline void talk_buffer_set_policy(int policy) { (void)policy; }

/* publish these strings, so they're stored only once (better than #define) */
#define dir_thumbnail_name "_dirname.talk"
#define file_thumbnail_ext ".talk"

static inline void talk_init(void) { }
static inline int talk_get_bufsize(void) { return 0; }
static inline size_t talkbuf_init(char* bufstart) { (void)bufstart; return 0; }
static inline bool is_voice_queued(void) { return false; }
static inline int talk_id(int32_t id, bool enqueue) { (void)id; (void)enqueue; return -1; }
/* play a thumbnail from file */
static inline int talk_file(const char *root, const char *dir, const char *file, const char *ext, const long *prefix_ids, bool enqueue) { (void)root; (void)dir; (void)file; (void)ext; (void)prefix_ids; (void)enqueue; return -1; }
/* play file's thumbnail or spell name */
static inline int talk_file_or_spell(const char *dirname, const char* filename, const long *prefix_ids, bool enqueue) { (void)dirname; (void)filename; (void)prefix_ids; (void)enqueue; return -1; }

/* play dir's thumbnail or spell name */
static inline int talk_dir_or_spell(const char* filename, const long *prefix_ids, bool enqueue) { (void)filename; (void)prefix_ids; (void)enqueue; return -1; }

/* play thumbnails for each components of full path, or spell */
static inline int talk_fullpath(const char* path, bool enqueue) { (void)path; (void)enqueue; return -1; }
static inline int talk_number(long n, bool enqueue) { (void)n; (void)enqueue; return -1; }
static inline int talk_value(long n, int unit, bool enqueue) { (void)n; (void)unit; (void)enqueue; return -1; }
static inline int talk_value_decimal(long n, int unit, int decimals, bool enqueue) { (void)n; (void)unit; (void)decimals; (void)enqueue; return -1; }
static inline int talk_spell(const char* spell, bool enqueue) { (void)spell; (void)enqueue; return -1; }
static inline void talk_setting(const void *global_settings_variable) { (void)global_settings_variable; }
static inline void talk_disable(bool disable) { (void)disable; }
static inline void talk_force_shutup(void) { }
static inline void talk_shutup(void) { }

/* helper function for speaking fractional numbers */
static inline void talk_fractional(char *tbuf, int value, int unit) { (void)tbuf; (void)value; (void)unit; }

static inline void talk_time(const struct tm *tm, bool enqueue) { (void)tm; (void)enqueue; }
static inline void talk_date(const struct tm *tm, bool enqueue) { (void)tm; (void)enqueue; }

#ifdef HAVE_MULTIVOLUME
static inline int talk_volume_id(int volume) { (void)volume; return 0; }
#endif

/* speaks hr, min, sec, ms; unit_idx is lowest or base unit of the time value */
static inline int talk_time_intervals(long time, int unit_idx, bool enqueue) { (void)time; (void)unit_idx; (void)enqueue; return -1; }

/* This (otherwise invalid) ID signals the end of the array. */
#define TALK_FINAL_ID LANG_LAST_INDEX_IN_ARRAY
#define TALK_FINAL_ID_VOICEONLY LANG_LAST_VOICEONLY_INDEX_IN_ARRAY
/* Enqueue next utterance even if enqueue parameter is false: don't
   interrupt the current utterance. */
static inline void talk_force_enqueue_next(void) { }

/* speaks one or more IDs (from an array)). */
static inline int talk_idarray(const long *idarray, bool enqueue) { (void)idarray; (void)enqueue; return -1; }
/* This makes an initializer for the array of IDs and takes care to
   put the final sentinel element at the end. */
#define TALK_IDARRAY(ids...) ((long[]){ids,TALK_FINAL_ID})
/* And this handy macro makes it look like a variadic function. */
#define talk_ids(enqueue, ids...) talk_idarray(TALK_IDARRAY(ids), enqueue)
/* This version talks only if talking menus are enabled, and does not
   enqueue the initial id. */
#define cond_talk_ids(ids...) do { \
        if (global_settings.talk_menu) \
            talk_ids(false, ids); \
    } while(0)
/* And a version that takes the array parameter... */
#define cond_talk_idarray(idarray) do { \
        if (global_settings.talk_menu \
            talk_idarray(idarray, false); \
    } while(0)
/* Convenience macro to conditionally speak something and not have
   it interrupted. */
#define cond_talk_ids_fq(ids...) do { \
        if (global_settings.talk_menu) { \
            talk_ids(false, ids);                 \
            talk_force_enqueue_next(); \
        } \
    }while(0)

struct talk_debug_data {
    char voicefile[32];
    long memory_allocated, memory_used;
    int  num_clips, num_empty_clips;
    int  min_clipsize, avg_clipsize, max_clipsize;
    int  cached_clips;
    int  cache_hits;
    int  cache_misses;
    enum talk_status status;
};

static inline void talk_announce_voice_invalid(void) { }

static inline bool talk_get_debug_data(struct talk_debug_data *data) { (void)data; return false; }

#endif /* __TALK_H__ */
