/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Björn Stenberg <bjorn@haxx.se>
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
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include "debug.h"

#include "screens.h"
#include "button.h"

#include "string.h"
#include "lcd.h"

#include "power.h"

#include "ata.h" /* for volume definitions */

static bool storage_spinning = false;

#include "devicedata.h"
struct device_data_t device_data =
    {.length = DEVICE_DATA_PAYLOAD_SIZE};

int fat_startsector(void)
{
    return 63;
}

int ata_spinup_time(void)
{
    return 100;
}

int storage_spinup_time(void)
{
    return 0;
}

int storage_init(void)
{
    return 1;
}

int storage_write_sectors(IF_MV(int drive,)
                      unsigned long start,
                      int count,
                      const void* buf)
{
    IF_MV((void)drive;)
    int i;

    for (i=0; i<count; i++ ) {
        FILE* f;
        char name[32];

        sprintf(name,"sector%lX.bin",start+i);
        f=fopen(name,"wb");
        if (f) {
            if(fwrite(buf,512,1,f) != 1) {
                fclose(f);
                return -1;
            }
            fclose(f);
        }
        else {
            return -1;
        }
    }
    return 0;
}

int storage_read_sectors(IF_MV(int drive,)
                     unsigned long start,
                     int count,
                     void* buf)
{
    IF_MV((void)drive;)
    int i;
    size_t ret;

    for (i=0; i<count; i++ ) {
        FILE* f;
        char name[32];

        DEBUGF("Reading sector %lX\n",start+i);
        sprintf(name,"sector%lX.bin",start+i);
        f=fopen(name,"rb");
        if (f) {
            ret = fread(buf,512,1,f);
            fclose(f);
            if (ret != 1)
                return -1;
        }
    }
    return 0;
}

void storage_spin(void)
{
    storage_spinning = true;
}

void storage_sleep(void)
{
}

bool storage_disk_is_active(void)
{
    return storage_spinning;
}

void storage_spindown(int s)
{
    (void)s;
    storage_spinning = false;
}

int rtc_read(int address)
{
    return address ^ 0x55;
}

int rtc_write(int address, int value)
{
    (void)address;
    (void)value;
    return 0;
}

#ifdef HAVE_USB_POWER
bool usb_powered_only(void)
{
    return false;
}

bool usb_charging_enable(bool on)
{
    (void)on;
    return false;
}
#endif

void lcd_set_contrast( int x )
{
    (void)x;
}

void cpu_sleep(bool enabled)
{
    (void)enabled;
}

