/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Alan Korr
 * Copyright (C) 2008 by Frank Gevaerts
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
#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <stdbool.h>
#include "config.h" /* for HAVE_MULTIDRIVE or not */
#include "mv.h"
#include <kernel.h>

#if (CONFIG_STORAGE & STORAGE_HOSTFS) || defined(SIMULATOR)
#define HAVE_HOSTFS
#endif

#if (CONFIG_STORAGE & STORAGE_SD)
#include "sd.h"
#endif

enum
{
    Q_STORAGE_TICK = 1,
    Q_STORAGE_SLEEP,
    Q_STORAGE_SLEEPNOW,
#ifdef STORAGE_CLOSE
    Q_STORAGE_CLOSE,
#endif
    Q_STORAGE_MEDIUM_INSERTED,
    Q_STORAGE_MEDIUM_REMOVED,
};

#define STG_EVENT_ASSERT_ACTIVE(type) \
    ({ intptr_t __data = (data);          \
       *((unsigned int *)(__data)) |= (type); })

static FORCE_INLINE int storage_event_default_handler(long id,
                                                      intptr_t data,
                                                      long last_activity,
                                                      unsigned int type)
{
    /* fake sleep in order to trigger storage idle sequence */
    static long slept_at = -1;

    if (id ==  Q_STORAGE_TICK) {
        if (last_activity == slept_at ||
            TIME_BEFORE(current_tick, last_activity + 3*HZ)) {
            STG_EVENT_ASSERT_ACTIVE(type);
        }
    }
    else if (id == Q_STORAGE_SLEEPNOW) {
        slept_at = last_activity;
    }

    return 0;
}

#if (CONFIG_STORAGE & STORAGE_SD)
int sd_event(long id, intptr_t data);
#endif

struct storage_info
{
    unsigned int sector_size;
    sector_t num_sectors;
    char *vendor;
    char *product;
    char *revision;
};

int storage_init(void) STORAGE_INIT_ATTR;
void storage_close(void);
void storage_post_event(long event, intptr_t data);

#ifdef HAVE_HOSTFS
#include "hostfs.h"
/* stubs for the plugin api */
static inline void stub_storage_sleep(void) {}
static inline void stub_storage_spin(void) {}
static inline void stub_storage_spindown(int timeout) { (void)timeout; }
static inline int stub_storage_event(long id, intptr_t data)
    { return 0; (void)id; (void)data; }
static inline void storage_sleep(void) {};
#else /* ndef HAVE_HOSTFS */
static inline void storage_sleep(void) {};
#endif /* HAVE_HOSTFS */

/* storage_spindown, storage_sleep and storage_spin are passed as
 * pointers, which doesn't work with argument-macros.
 */
    #define storage_num_drives() NUM_DRIVES
    #if defined(HAVE_HOSTFS)
        #define STORAGE_FUNCTION(NAME) (stub_storage_## NAME)
        #define storage_event stub_storage_event
        #define storage_spindown(sec) stub_storage_spindown(sec)
        #define storage_sleep() stub_storage_sleep()
        #define storage_spin() stub_storage_spin()

        #define storage_enable(on)
        #define storage_sleepnow()
        #define storage_disk_is_active() 0
        #define storage_soft_reset()
        #define storage_init() hostfs_init()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() hostfs_flush()
        #endif
        #define storage_last_disk_activity() (-1)
        #define storage_spinup_time() 0
        #define storage_get_identify() (NULL) /* not actually called anywher */

        #ifdef STORAGE_GET_INFO
            #error storage_get_info not implemented
        #endif
            #define storage_removable(drive) hostfs_removable(IF_MD(drive))
            #define storage_present(drive) hostfs_present(IF_MD(drive))
        #define storage_driver_type(drive) hostfs_driver_type(IF_MV(drive))
    #elif (CONFIG_STORAGE & STORAGE_SD)
        #define STORAGE_FUNCTION(NAME) (sd_## NAME)
        #define storage_spindown(sec) sd_spindown(sec)
        #define storage_spin() sd_spin()

        #define storage_enable(on) sd_enable(on)
        #define storage_sleepnow() do {} while (0)
        #define storage_disk_is_active() 0
        #define storage_soft_reset() (void)0
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() (void)0
        #endif
        #define storage_last_disk_activity() sd_last_disk_activity()
        #define storage_spinup_time() 0
        #define storage_get_identify() sd_get_identify()

        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) sd_get_info(IF_MD(drive,) info)
        #endif
            #define storage_removable(drive) sd_removable(IF_MD(drive))
            #define storage_present(drive) sd_present(IF_MD(drive))
        #define storage_driver_type(drive) (STORAGE_SD_NUM)
    #else
        //#error No storage driver!
    #endif

int storage_read_sectors(IF_MD(int drive,) sector_t start, int count, void* buf);
int storage_write_sectors(IF_MD(int drive,) sector_t start, int count, const void* buf);

#endif /* __STORAGE_H__ */
