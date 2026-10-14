/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Daniel Stenberg
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
#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"
#include <stdbool.h>


/* The whole driver should be built */
#define BACKLIGHT_FULL_INIT

bool is_backlight_on(bool ignore_always_off);
void backlight_on_ignore(bool value, int timeout);
void backlight_on(void);
void backlight_off(void);
void backlight_set_timeout(int value);

void backlight_init(void) INIT_ATTR;
void backlight_close(void);
int  backlight_get_current_timeout(void);

void backlight_set_fade_in(bool value);
void backlight_set_fade_out(bool value);

void backlight_set_timeout_plugged(int value);

void backlight_set_on_button_hold(int index);

#ifdef SIMULATOR
void sim_backlight(int value);
void sim_remote_backlight(int value);
#endif

#ifdef BACKLIGHT_FULL_INIT
extern int backlight_brightness;
#else
#define backlight_brightness DEFAULT_BRIGHTNESS_SETTING
#endif
void backlight_set_brightness(int val);

void buttonlight_set_brightness(int val);

void buttonlight_on_ignore(bool value, int timeout);
void buttonlight_on(void);
void buttonlight_off(void);
void buttonlight_set_timeout(int value);

/* Private API for use in target tree backlight code only */

#endif /* BACKLIGHT_H */
