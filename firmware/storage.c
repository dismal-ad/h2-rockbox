/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
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
#include "storage.h"
#include "kernel.h"
#include "ata_idle_notify.h"
#include "usb.h"
#include "disk.h"
#include "pathfuncs.h"

# include "sdmmc_host.h"

#ifdef CONFIG_STORAGE_MULTI

#define DRIVER_MASK     0xff000000
#define DRIVER_OFFSET   24
#define DRIVE_MASK      0x00ff0000
#define DRIVE_OFFSET    16
#define PARTITION_MASK  0x0000ff00

static unsigned int storage_drivers[NUM_DRIVES];
static unsigned int num_drives;
#endif /* CONFIG_STORAGE_MULTI */

/* defaults: override elsewhere target-wise if they must be different */
#if (CONFIG_STORAGE & STORAGE_SD)
 #ifndef SD_THREAD_STACK_SIZE
  #define SD_THREAD_STACK_SIZE      (DEFAULT_STACK_SIZE*2)
 #endif
#endif

static struct event_queue storage_queue SHAREDBSS_ATTR;
static unsigned int storage_thread_id = 0;

static union {
#if (CONFIG_STORAGE & STORAGE_SD)
    long stk_sd[SD_THREAD_STACK_SIZE / sizeof (long)];
#endif
} storage_thread_stack;

static const char storage_thread_name[] =
#if (CONFIG_STORAGE & STORAGE_SD)
    "/sd"
#endif
    ;

/* event is targeted to a specific drive */
#define DRIVE_EVT  (1 << STORAGE_NUM_TYPES)

#ifdef CONFIG_STORAGE_MULTI
static int storage_event_send(unsigned int route, long id, intptr_t data)
{
    /* most events go to everyone */
    if (UNLIKELY(route == DRIVE_EVT)) {
        route = (storage_drivers[data] & DRIVER_MASK) >> DRIVER_OFFSET;
        data  = (storage_drivers[data] & DRIVE_MASK) >> DRIVE_OFFSET;
    }

    int rc = 0;

#if (CONFIG_STORAGE & STORAGE_SD)
    if (route & STORAGE_SD) {
        rc = sd_event(id, data);
    }
#endif

    return rc;
}
#endif /* CONFIG_STORAGE_MULTI */

#ifndef CONFIG_STORAGE_MULTI
static FORCE_INLINE int storage_event_send(unsigned int route, long id,
                                           intptr_t data)
{
    return route ? STORAGE_FUNCTION(event)(id, data) : 0;
}
#endif /* ndef CONFIG_STORAGE_MULTI */

static void NORETURN_ATTR storage_thread(void)
{
    unsigned int bdcast = CONFIG_STORAGE;
    bool usb_mode = false;
    struct queue_event ev;

    while (1)
    {
        queue_wait_w_tmo(&storage_queue, &ev, HZ/2);

        switch (ev.id)
        {
        case SYS_TIMEOUT:;
            /* drivers hold their bit low when they want to
               sleep and keep it high otherwise */
            unsigned int trig = 0;
            storage_event_send(bdcast, Q_STORAGE_TICK, (intptr_t)&trig);
            trig = bdcast & ~trig;
            if (trig) {
                if (!usb_mode) {
                    call_storage_idle_notifys(false);
                }
                storage_event_send(trig, Q_STORAGE_SLEEPNOW, 0);
            }
            break;

        case Q_STORAGE_MEDIUM_INSERTED:
        case Q_STORAGE_MEDIUM_REMOVED:
            storage_event_send(DRIVE_EVT, ev.id, ev.data);
            break;

#ifdef STORAGE_CLOSE
        case Q_STORAGE_CLOSE:
            storage_event_send(CONFIG_STORAGE, ev.id, 0);
            thread_exit();
#endif /* STORAGE_CLOSE */

        case SYS_HOTSWAP_INSERTED:
        case SYS_HOTSWAP_EXTRACTED:
            if (!usb_mode) {
                int drive = IF_MD_DRV(ev.data);
                if (!CHECK_DRV(drive)) {
                    break;
                }

                int umnt = disk_unmount(drive);
                int mnt = 0;
                int rci = storage_event_send(DRIVE_EVT, ev.id, drive);

                if (ev.id == SYS_HOTSWAP_INSERTED && !rci) {
                    mnt = disk_mount(drive);
                }

                if (umnt > 0 || mnt > 0) {
                    /* something was unmounted and/or mounted */
                    queue_broadcast(SYS_FS_CHANGED, drive);
                }
            }
            break;

        case SYS_USB_CONNECTED:
        case SYS_USB_DISCONNECTED:
            bdcast = 0;
            storage_event_send(CONFIG_STORAGE, ev.id, (intptr_t)&bdcast);
            usb_mode = ev.id == SYS_USB_CONNECTED;
            if (usb_mode) {
                usb_acknowledge(SYS_USB_CONNECTED_ACK, ev.data);
            }
            else {
                bdcast = CONFIG_STORAGE;
            }
            break;
        }
    }
}

#ifdef STORAGE_CLOSE
void storage_close(void)
{
    if (storage_thread_id) {
        queue_post(&storage_queue, Q_STORAGE_CLOSE, 0);
        thread_wait(storage_thread_id);
    }
}
#endif /* STORAGE_CLOSE */

void storage_post_event(long event, intptr_t data)
{
    if (storage_thread_id)
        queue_post(&storage_queue, event, data);
}

static inline void storage_thread_init(void)
{
    if (storage_thread_id) {
        return;
    }

    queue_init(&storage_queue, true);
    storage_thread_id = create_thread(storage_thread, &storage_thread_stack,
                                      sizeof (storage_thread_stack),
                                      0, &storage_thread_name[1]
                                      IF_PRIO(, PRIORITY_USER_INTERFACE)
                                      IF_COP(, CPU));
}

int storage_init(void)
{
    int rc=0;

    sdmmc_host_target_init();

#ifdef CONFIG_STORAGE_MULTI
    int i;
    num_drives=0;

#if (CONFIG_STORAGE & STORAGE_SD)
    if ((rc=sd_init())) return rc;

    int sd_drives = sd_num_drives(num_drives);
    for (i=0; i<sd_drives; i++)
    {
        storage_drivers[num_drives++] =
            (STORAGE_SD<<DRIVER_OFFSET) | (i << DRIVE_OFFSET);
    }
#endif

#else /* ndef CONFIG_STORAGE_MULTI */
    rc = STORAGE_FUNCTION(init)();
#endif /* CONFIG_STORAGE_MULTI */

#ifdef HAVE_MULTIVOLUME
    init_volume_names();
#endif

    storage_thread_init();
    return rc;
}

int storage_read_sectors(IF_MD(int drive,) sector_t start, int count,
                         void* buf)
{
#ifdef CONFIG_STORAGE_MULTI
    int driver=(storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    int ldrive=(storage_drivers[drive] & DRIVE_MASK)>>DRIVE_OFFSET;

    switch (driver)
    {

#if (CONFIG_STORAGE & STORAGE_SD)
    case STORAGE_SD:
        return sd_read_sectors(IF_MD(ldrive,) start,count,buf);
#endif

    }

    return -1;
#else /* CONFIG_STORAGE_MULTI */
    return STORAGE_FUNCTION(read_sectors)(IF_MD(drive,)start,count,buf);
#endif /* CONFIG_STORAGE_MULTI */

}

int storage_write_sectors(IF_MD(int drive,) sector_t start, int count,
                          const void* buf)
{
#ifdef CONFIG_STORAGE_MULTI
    int driver=(storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    int ldrive=(storage_drivers[drive] & DRIVE_MASK)>>DRIVE_OFFSET;

    switch (driver)
    {

#if (CONFIG_STORAGE & STORAGE_SD)
    case STORAGE_SD:
        return sd_write_sectors(IF_MD(ldrive,)start,count,buf);
#endif

    }

    return -1;
#else /* CONFIG_STORAGE_MULTI */
    return STORAGE_FUNCTION(write_sectors)(IF_MD(drive,)start,count,buf);
#endif /* CONFIG_STORAGE_MULTI */
}

#ifdef CONFIG_STORAGE_MULTI

#define DRIVER_MASK     0xff000000
#define DRIVER_OFFSET   24
#define DRIVE_MASK      0x00ff0000
#define DRIVE_OFFSET    16
#define PARTITION_MASK  0x0000ff00

static unsigned int storage_drivers[NUM_DRIVES];
static unsigned int num_drives;

int storage_num_drives(void)
{
    return num_drives;
}

int storage_driver_type(int drive)
{
    if ((unsigned int)drive >= num_drives)
        return -1;

    unsigned int bit = (storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    return bit ? find_first_set_bit(bit) : -1;
}

void storage_enable(bool on)
{

#if (CONFIG_STORAGE & STORAGE_SD)
    sd_enable(on);
#endif

}

void storage_sleepnow(void)
{

#if (CONFIG_STORAGE & STORAGE_SD)
    //sd_sleepnow();
#endif

}

bool storage_disk_is_active(void)
{

#if (CONFIG_STORAGE & STORAGE_SD)
    //if (sd_disk_is_active()) return true;
#endif

    return false;
}

int storage_soft_reset(void)
{
    int rc=0;

#if (CONFIG_STORAGE & STORAGE_SD)
    //if ((rc=sd_soft_reset())) return rc;
#endif

    return rc;
}

#ifdef HAVE_STORAGE_FLUSH
int storage_flush(void)
{
    int rc=0;

#if (CONFIG_STORAGE & STORAGE_SD)
    //if ((rc=sd_flush())) return rc;
#endif

    return rc;
}
#endif

void storage_spin(void)
{

#if (CONFIG_STORAGE & STORAGE_SD)
    sd_spin();
#endif

}

void storage_spindown(int seconds)
{

#if (CONFIG_STORAGE & STORAGE_SD)
    sd_spindown(seconds);
#endif

}

long storage_last_disk_activity(void)
{
    long max=0;
    long t;

#if (CONFIG_STORAGE & STORAGE_SD)
    t=sd_last_disk_activity();
    if (t>max) max=t;
#endif

    return max;
}

int storage_spinup_time(void)
{
    int max=0;
    int t;

#if (CONFIG_STORAGE & STORAGE_SD)
    //t=sd_spinup_time();
    //if (t>max) max=t;
    (void)t;
#endif

    return max;
}

#ifdef STORAGE_GET_INFO
void storage_get_info(int drive, struct storage_info *info)
{
    int driver=(storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    int ldrive=(storage_drivers[drive] & DRIVE_MASK)>>DRIVE_OFFSET;

    switch(driver)
    {

#if (CONFIG_STORAGE & STORAGE_SD)
    case STORAGE_SD:
        return sd_get_info(ldrive,info);
#endif

    }
}
#endif /* STORAGE_GET_INFO */

bool storage_removable(int drive)
{
    int driver=(storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    int ldrive=(storage_drivers[drive] & DRIVE_MASK)>>DRIVE_OFFSET;

    switch(driver)
    {

#if (CONFIG_STORAGE & STORAGE_SD)
    case STORAGE_SD:
        return sd_removable(ldrive);
#endif

    default:
        return false;
    }
}

bool storage_present(int drive)
{
    int driver=(storage_drivers[drive] & DRIVER_MASK)>>DRIVER_OFFSET;
    int ldrive=(storage_drivers[drive] & DRIVE_MASK)>>DRIVE_OFFSET;

    switch(driver)
    {

#if (CONFIG_STORAGE & STORAGE_SD)
    case STORAGE_SD:
        return sd_present(ldrive);
#endif

    default:
        return false;
    }
}
#endif /*CONFIG_STORAGE_MULTI*/
