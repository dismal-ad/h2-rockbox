/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2006 Dan Everton
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

#include "debug.h"
#include "sim-ui-defines.h"
#include "system.h"
#include "button-sdl.h"
#include "lcd-sdl.h"
#include "screendump.h"

SDL_Surface* lcd_surface;

#define BACKLIGHT_OFF_ALPHA 0  /* pitch black */

static unsigned long get_lcd_pixel(int x, int y)
{
    return *FBADDR(x, y);
}

void lcd_update(void)
{
    /* update a full screen rect */
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

void lcd_update_rect(int x_start, int y_start, int width, int height)
{

    sdl_update_rect(lcd_surface, x_start, y_start, width, height,
                    LCD_WIDTH, LCD_HEIGHT, get_lcd_pixel);
    sdl_gui_update(lcd_surface, x_start, y_start, width,
                   height + LCD_SPLIT_LINES, SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                   background ? UI_LCD_POSX : 0, background? UI_LCD_POSY : 0);
}

#if (SDL_MAJOR_VERSION > 1)
void sim_backlight(int value)
{
    SDL_SetSurfaceAlphaMod(lcd_surface, (value * 255) / 100);

    sdl_gui_update(lcd_surface, 0, 0, SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                   SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                   background ? UI_LCD_POSX : 0, background? UI_LCD_POSY : 0);
}
#endif /* HAVE_BACKLIGHT */

/* initialise simulator lcd driver */
void lcd_init_device(void)
{
    lcd_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                                       LCD_DEPTH, 0, 0, 0, 0);
#if SDL_MAJOR_VERSION > 1
    SDL_SetSurfaceBlendMode(lcd_surface, SDL_BLENDMODE_BLEND);
#endif
}

