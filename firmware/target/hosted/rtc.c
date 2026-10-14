/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Based upon code (C) 2002 by Björn Stenberg
 * Copyright (C) 2011 by Thomas Jarosch
 * Copyright (C) 2018 by Marcin Bukat
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
#include <time.h>
#include <sys/time.h>
#if !defined(WIN32)
#include <sys/ioctl.h>
#if !defined(__APPLE__)
#include <linux/rtc.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#endif

#include "config.h"

void rtc_init(void)
{
  tzset();
}

int rtc_read_datetime(struct tm *tm)
{
    time_t now = time(NULL);
    *tm = *localtime(&now);

    return 0;
}

int rtc_write_datetime(const struct tm *tm)
{
#if !defined(WIN32) && !defined(__APPLE__)
    struct timeval tv;
    struct tm *tm_time;

    tv.tv_sec = mktime((struct tm *)tm);
    tv.tv_usec = 0;

    if ((int)tv.tv_sec == -1)
        return -1;

    /* set system clock */
    settimeofday(&tv, NULL);

    /* hw clock stores time in UTC */
    time_t now = time(NULL);
    tm_time = gmtime(&now);

    /* Try to write the HW RTC, if present. */
    int rtc = open("/dev/rtc0", O_WRONLY | O_CLOEXEC);
    if (rtc > 0) {
        ioctl(rtc, RTC_SET_TIME, (struct rtc_time *)tm_time);
        close(rtc);
    }
#else
    (void)(*tm);
#endif
    return 0;
}

