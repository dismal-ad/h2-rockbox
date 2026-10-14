/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Alan Korr
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
#ifndef __ATA_H__
#define __ATA_H__

#include <stdbool.h>
#include "config.h" /* for HAVE_MULTIVOLUME or not */
#include "mv.h" /* for IF_MV() and friends */

struct storage_info;

void ata_enable(bool on);
void ata_spindown(int seconds);
void ata_sleepnow(void);
/* NOTE: DO NOT use this to poll for disk activity.
         If you are waiting for the disk to become active before
         doing something use ata_idle_notify.h
 */
bool ata_disk_is_active(void);
int ata_soft_reset(void);
int ata_init(void) STORAGE_INIT_ATTR;
void ata_close(void);
int ata_read_sectors(IF_MD(int drive,) sector_t start, int count, void* buf);
int ata_write_sectors(IF_MD(int drive,) sector_t start, int count, const void* buf);
void ata_spin(void);
#if (CONFIG_LED == LED_REAL)
void ata_set_led_enabled(bool enabled);
#endif
unsigned short* ata_get_identify(void);

#ifdef STORAGE_GET_INFO
void ata_get_info(IF_MD(int drive,) struct storage_info *info);
#endif
bool ata_removable(IF_MD_NONVOID(int drive));
bool ata_present(IF_MD_NONVOID(int drive));


#ifdef CONFIG_STORAGE_MULTI
int ata_num_drives(int first_drive);
#endif


long ata_last_disk_activity(void);
int ata_spinup_time(void); /* ticks */

/* Returns 1 if drive is solid-state */
static inline int ata_disk_isssd(void)
{
#ifdef SIMULATOR
    return 0;
#else
    unsigned short *identify_info = ata_get_identify();
    /*
       Offset 217 is "Nominal Rotation rate"
        0x0000 == Not reported
        0x0001 == Solid State
        0x0401 -> 0xffe == RPM
        All others reserved

        However, this is a relatively recent change, and we can't rely on it,
        especially for the FC1307A CF->SD adapters!

       Offset 168 is "Nominal Form Factor"
         all values >= 0x06 are guaranteed to be Solid State (mSATA, m.2, etc)

       Offset 169 b0 is set to show device implements TRIM.

    Unreliable mechanisms:

       Offset 83 b2 shows device implements CFA commands.
        However microdrives pose a problem as they support CFA but are not
        SSD.

       Offset 163 shows CF Advanced timing modes; microdrives all seems to
        report 0, but all others (including iFlash) report higher!  This
        is often present even when the "CFA supported" bit is 0.

       Offset 160 b15 indicates support for CF+ power level 1, if not set
        then device is standard flash CF.  However this is not foolproof
        as newer CF cards (and those CF->SD adapters) may report this.

     */
    return ( (identify_info[217] == 0x0001)               /* "Solid state" rotational rate */
             || ((identify_info[168] & 0x0f) >= 0x06)     /* Explicit SSD form factors */
             || (identify_info[169] & (1<<0))             /* TRIM supported */
             || (identify_info[163] > 0)                  /* CF Advanced timing modes */
             || ((identify_info[83] & (1<<2)) &&          /* CFA compliant */
                 ((identify_info[160] & (1<<15)) == 0))   /* CF power level 0 */
           );
#endif
}

/* Returns 1 if the drive supports power management commands */
static inline int ata_disk_can_sleep(void)
{
    unsigned short *identify_info = ata_get_identify();
    /* Only devices that claim to support PM can be safely powered off.
       This notably excludes the various SD adapters! */
    return (identify_info[82] & (1<<3) && identify_info[85] & (1<<3));
}

int ata_flush(void);

#define ATA_IDENTIFY_WORDS 256

#endif /* __ATA_H__ */
