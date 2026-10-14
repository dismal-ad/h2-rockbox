/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2009 by Thomas Martitz
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

#include <stdbool.h>
#include "config.h"
#include "backlight.h"
#include "lcd.h"

/* in uisimulator/sdl/lcd-bitmap.c */
extern void sim_backlight(int value);

bool backlight_hw_init(void)
{
    return true;
}


static inline int normalize_backlight(int val)
{
    /* normalize to xx% brightness for sdl */
    return ((val - MIN_BRIGHTNESS_SETTING + 1) * 100) /
           (MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING + 1);
}

void backlight_hw_brightness(int val)
{
    sim_backlight(normalize_backlight(val));
}


void backlight_hw_on(void)
{
    lcd_enable(true);
#if (CONFIG_BACKLIGHT_FADING != BACKLIGHT_FADING_SW_SETTING)
    /* if we set the brightness to the settings value, then fading up
     * is glitchy */
    sim_backlight(normalize_backlight(backlight_brightness));

#endif
}

void backlight_hw_off(void)
{
    sim_backlight(0);
    lcd_enable(false);
}


