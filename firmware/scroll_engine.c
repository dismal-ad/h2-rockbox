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
 * LCD scrolling thread and scheduler
 *
 * Much collected and combined from the various Rockbox LCD drivers.
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
#include "config.h"
#include "gcc_extensions.h"
#include "cpu.h"
#include "kernel.h"
#include "thread.h"
#include "usb.h"
#include "lcd.h"
#include "font.h"
#include "misc.h"
#include "settings.h"
#include "scroll_engine.h"

static const char scroll_tick_table[18] = {
 /* Hz values [f(x)=100.8/(x+.048)]:
    1, 1.25, 1.55, 2, 2.5, 3.12, 4, 5, 6.25, 8.33, 10, 12.5, 16.7, 20, 25, 33, 49.2, 96.2 */
    100, 80, 64, 50, 40, 32, 25, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1
};

#include "drivers/lcd-scroll.c"

static void scroll_thread(void);
static const char scroll_name[] = "scroll";
static void scroll_thread(void) NORETURN_ATTR;

static void scroll_thread(void)
{
    while (1)
    {
        sleep(lcd_scroll_info.ticks);
        if (lcd_active())
            lcd_scroll_worker();
    }
}

void scroll_init(void)
{
    static long scroll_stack[(DEFAULT_STACK_SIZE*3)/sizeof(long)];
    create_thread(scroll_thread, scroll_stack,
                  sizeof(scroll_stack), 0, scroll_name
                  IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
}
