/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * Tuner abstraction layer
 *
 * Copyright (C) 2004 Jörg Hohensohn
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
#ifndef __TUNER_H__
#define __TUNER_H__

#include <stdbool.h>

#include "config.h"

/** Settings to the tuner layer **/
enum
{
    RADIO_ALL = -1, /* debug */
    RADIO_SLEEP,
    RADIO_FREQUENCY,
    RADIO_MUTE,
    RADIO_FORCE_MONO,
    RADIO_SCAN_FREQUENCY,
    
    /* Put new general-purpose settings above this line */
    __RADIO_SET_STANDARD_LAST
};

/** Readback from the tuner layer **/
enum
{
    RADIO_PRESENT = 0,
    RADIO_TUNED,
    RADIO_STEREO,
    RADIO_RSSI,
    RADIO_RSSI_MIN,
    RADIO_RSSI_MAX,

    /* Put new general-purpose readback values above this line */
    __RADIO_GET_STANDARD_LAST
};

/** Tuner regions **/

/* Basic region information */
enum
{
    REGION_EUROPE = 0,
    REGION_US_CANADA,
    REGION_JAPAN,
    REGION_KOREA,
    REGION_ITALY,
    REGION_OTHER,

    /* Add new regions above this line */
    TUNER_NUM_REGIONS
};

struct fm_region_data
{
    int freq_min;
    int freq_max;
    int freq_step;
    int deemphasis; /* in microseconds, usually 50 or 75 */
};

extern const struct fm_region_data fm_region_data[TUNER_NUM_REGIONS];

#endif /* __TUNER_H__ */
