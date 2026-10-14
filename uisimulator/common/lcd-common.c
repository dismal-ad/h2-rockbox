/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Robert E. Hak <rhak@ramapo.edu>
 *
 * Windows Copyright (C) 2002 by Felix Arends
 * X11 Copyright (C) 2002 by Daniel Stenberg <daniel@haxx.se>
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
#include "system.h"
#include "lcd.h"

static bool lcd_enabled = false;

void lcd_set_flip(bool yesno)
{
    (void)yesno;
}

void lcd_set_invert_display(bool invert)
{
    (void)invert;
}

int lcd_default_contrast(void)
{
    return 28;
}

void lcd_enable(bool on)
{
    if (on && !lcd_enabled)
    {
        send_event(LCD_EVENT_ACTIVATION, NULL);
    }
    lcd_enabled = on;
}

bool lcd_active(void)
{
    bool retval = false;
    retval = lcd_enabled;
    return retval;
}

void lcd_shutdown(void)
{
}
