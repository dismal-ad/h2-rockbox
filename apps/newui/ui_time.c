/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026
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

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "ui.h"

#ifdef SIMULATOR
#include <sys/time.h>

uint32_t ui_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000000ull + tv.tv_usec);
}
#elif CONFIG_CPU == X1000
uint32_t ui_time_us(void)
{
    /* the 32-bit OST count wraps every ~12 minutes; divide the 64-bit one */
    return (uint32_t)(__ost_read64() / OST_TICKS_PER_US);
}
#else
uint32_t ui_time_us(void)
{
    return current_tick * (1000000 / HZ);
}
#endif
