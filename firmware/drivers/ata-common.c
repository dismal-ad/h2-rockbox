/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2024 Solomon Peachy
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

/* This is intended to be #included into the ATA driver */

static sector_t total_sectors;
static uint32_t log_sector_size;
static uint16_t identify_info[ATA_IDENTIFY_WORDS] STORAGE_ALIGN_ATTR;
static bool canflush = true;
static int spinup_time = 0;
static struct mutex ata_mutex SHAREDBSS_ATTR;

int ata_spinup_time(void)
{
    return spinup_time;
}

#ifdef STORAGE_GET_INFO
void ata_get_info(IF_MD(int drive,)struct storage_info *info)
{
    unsigned short *src,*dest;
    static char vendor[8];
    static char product[16];
    static char revision[4];
#ifdef HAVE_MULTIDRIVE
    (void)drive; /* unused for now */
#endif
    int i;

    info->sector_size = log_sector_size;
    info->num_sectors = total_sectors;

    src = (unsigned short*)&identify_info[27];
    dest = (unsigned short*)vendor;
    for (i=0;i<4;i++)
        dest[i] = htobe16(src[i]);
    info->vendor=vendor;

    src = (unsigned short*)&identify_info[31];
    dest = (unsigned short*)product;
    for (i=0;i<8;i++)
        dest[i] = htobe16(src[i]);
    info->product=product;

    src = (unsigned short*)&identify_info[23];
    dest = (unsigned short*)revision;
    for (i=0;i<2;i++)
        dest[i] = htobe16(src[i]);
    info->revision=revision;
}
#endif

