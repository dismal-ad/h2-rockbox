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

/*
 * Rockbox button functions
 */

#include <stdlib.h>
#include "config.h"
#include "system.h"
#include "button.h"
#include "kernel.h"
#include "thread.h"
#include "backlight.h"
#include "serial.h"
#include "power.h"
#include "powermgmt.h"
#if defined(HAVE_SDL)
#include <SDL.h>
#if (SDL_MAJOR_VERSION > 1)
#include "button-sdl.h"
#else
#include "button-target.h"
#endif
#endif /* HAVE_SDL */

static long lastbtn;   /* Last valid button status */
static long last_read; /* Last button status, for debouncing/filtering */
static bool flipped;  /* buttons can be flipped to match the LCD flip */

static bool (*keypress_filter_fn)(int, int);

static bool enable_sw_poweroff = true;

/* how long until repeat kicks in, in centiseconds */
#define REPEAT_START      (30*HZ/100)

/* The next two make repeat "accelerate", which is nice for lists
 * which begin to scroll a bit faster when holding until the
 * real list acceleration kicks in (this smooths acceleration).
 *
 * Note that touchscreen pointing events are not subject to this
 * acceleration and always use REPEAT_INTERVAL_TOUCH. That value
 * essentially determines the touchscreen polling rate.
 */

/* the speed repeat starts at, in centiseconds */
#define REPEAT_INTERVAL_START   (16*HZ/100)
/* speed repeat finishes at, in centiseconds */
#define REPEAT_INTERVAL_FINISH  (5*HZ/100)
/* repeat interval for touch events */
#define REPEAT_INTERVAL_TOUCH   (4*HZ/100)

static int lastdata = 0;
static int button_read(int *data);

static void button_remote_post(void)
{
#if defined(HAS_SERIAL_REMOTE) && !defined(SIMULATOR)
    /* Post events for the remote control */
    int btn = remote_control_rx();
    if(btn)
        button_queue_try_post(btn, 0);
#endif
}

static int hp_detect_callback(struct timeout *tmo)
{
    /* Try to post only transistions */
    const long id = tmo->data ? SYS_PHONE_PLUGGED : SYS_PHONE_UNPLUGGED;
    button_queue_post_remove_head(id, 0);

    return 0;
    /*misc.c:hp_unplug_change*/
}

static int lo_detect_callback(struct timeout *tmo)
{
    /* Try to post only transistions */
    const long id = tmo->data ? SYS_LINEOUT_PLUGGED : SYS_LINEOUT_UNPLUGGED;
    button_queue_post_remove_head(id, 0);

    return 0;
    /*misc.c:lo_unplug_change*/
}

static void check_audio_peripheral_state(void)
{
    static struct timeout hp_detect_timeout; /* Debouncer for headphone plug/unplug */
    static bool phones_present = false;

    if (headphones_inserted() != phones_present)
    {
        /* Use the autoresetting oneshot to debounce the detection signal */
        phones_present = !phones_present;
        timeout_register(&hp_detect_timeout, hp_detect_callback,
                         HZ/2, phones_present);
    }
    static struct timeout lo_detect_timeout; /* Debouncer for lineout plug/unplug */
    static bool lineout_present = false;

    if (lineout_inserted() != lineout_present)
    {
        /* Use the autoresetting oneshot to debounce the detection signal */
        lineout_present = !lineout_present;
        timeout_register(&lo_detect_timeout, lo_detect_callback,
                         HZ/2, lineout_present);
    }
}

/* disabled function is shared between Main & Remote LCDs */
static bool filter_first_keypress_disabled(int button, int data)
{
    button_queue_try_post(button, data);
    return false;
}

static bool filter_first_keypress_enabled(int button, int data)
{
    if (is_backlight_on(false))
    {
        return filter_first_keypress_disabled(button, data);
    }
    return true;
}

static void button_tick(void)
{
    static int count = 0;
    static int repeat_speed = REPEAT_INTERVAL_START;
    static int repeat_count = 0;
    static bool repeat = false;
    static bool post = false;
    static bool skip_release = false;
    int diff;
    int btn;
    int data = 0;

    button_remote_post();

    btn = button_read(&data);

    check_audio_peripheral_state();

    /* Find out if a key has been released */
    diff = btn ^ lastbtn;
    if(diff && (btn & diff) == 0)
    {
            if(!skip_release)
                button_queue_try_post(BUTTON_REL | diff, data);
            else
                skip_release = false;
    }
    else
    {
        if ( btn )
        {
            /* normal keypress */
            if ( btn != lastbtn )
            {
                post = true;
                repeat = false;
                repeat_speed = REPEAT_INTERVAL_START;
            }
            else /* repeat? */
            {

                if ( repeat )
                {
                    if (!post)
                        count--;
                    if (count == 0) {
                        post = true;
                        /* yes we have repeat */
                        if (repeat_speed > REPEAT_INTERVAL_FINISH)
                            repeat_speed--;

                        count = repeat_speed;

                        repeat_count++;
                        /* Send a SYS_POWEROFF event if we have a device
                           which doesn't shut down easily with the OFF
                           key */

                        if (enable_sw_poweroff &&
                            (btn & POWEROFF_BUTTON
#ifdef RC_POWEROFF_BUTTON
                                    || btn == RC_POWEROFF_BUTTON
#endif
                                    ) &&
                                repeat_count > POWEROFF_COUNT)
                        {
                            /* Tell the main thread that it's time to
                               power off */
                            sys_poweroff();

                            /* Safety net for players without hardware
                               poweroff */
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
                            if(repeat_count > POWEROFF_COUNT * 10)
                                power_off();
#endif
                        }
                    }
                }
                else
                {
                    ++count;
                    if (count > REPEAT_START
                        )
                    {
                        post = true;
                        repeat = true;
                        repeat_count = 0;
                        /* initial repeat */
                        {
                            count = REPEAT_INTERVAL_START;
                        }
                    }
                }
            }
            if ( post )
            {
                if (repeat)
                {
                    /* Only post repeat events if the queue is empty,
                     * to avoid afterscroll effects. */
                    if (button_queue_try_post(BUTTON_REPEAT | btn, data))
                    {
                        skip_release = false;
                        post = false;
                        /* Need to post back/buttonlight_on() on repeat buttons */
                        {
                            backlight_on();
                            buttonlight_on();
                        }
                    }
                }
                else
                {
                    {
                        skip_release = keypress_filter_fn(btn, data);
                        backlight_on();
                        buttonlight_on();
                    }
                    post = false;
                }
                reset_poweroff_timer();
            }
        }
        else
        {
            repeat = false;
            count = 0;
        }
    }
    lastbtn = btn & ~(BUTTON_REL | BUTTON_REPEAT);

    lastdata = data;
}

void button_init(void)
{
    int temp;
    /* Init used objects first */
    button_queue_init();

    /* hardware inits */
    button_init_device();

    button_read(&temp);
    lastbtn = button_read(&temp);

    reset_poweroff_timer();

    flipped = false;
    set_backlight_filter_keypress(false);
    /* Start polling last */
    tick_add_task(button_tick);
}

void set_backlight_filter_keypress(bool value)
{
    if (!value)
        keypress_filter_fn = filter_first_keypress_disabled;
    else
        keypress_filter_fn = filter_first_keypress_enabled;
}

/*
 * Get button pressed from hardware
 */

static int button_read(int *data)
{
    (void) data;
    int btn = button_read_device();
    int retval;

    /* Filter the button status. It is only accepted if we get the same
       status twice in a row. */
    if (btn != last_read)
        retval = lastbtn;
    else
        retval = btn;
    last_read = btn;

    return retval;
}

int button_status(void)
{
    return lastbtn;
}

void button_set_sw_poweroff_state(bool en) {
    enable_sw_poweroff = en;
}

bool button_get_sw_poweroff_state() {
    return enable_sw_poweroff;
}
