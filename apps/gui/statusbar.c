/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) Robert E. Hak (2002), Linus Nielsen Feltzing (2002)
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
#include "font.h"
#include "kernel.h"
#include "string.h" /* for memcmp oO*/
#include "settings.h"
#include "viewport.h"
#include "powermgmt.h"
#include "usb.h"
#include "led.h"
#include "screen_access.h"

#include "status.h" /* needed for battery_state global var */
#include "action.h" /* for keys_locked */
#include "statusbar.h"
#include "appevents.h"
#include "timefuncs.h"
#ifdef HAVE_BLUETOOTH
#include "bluetooth/bluetooth.h"
#endif
#include "newui/ui.h"

/* The built-in status bar, drawn by the new UI (apps/newui/ui_status.c).
 * This file gathers the state and decides when to redraw. */

struct gui_syncstatusbar statusbars;

static void gui_statusbar_init(struct screen * display, struct gui_statusbar * bar)
{
    bar->display = display;
    bar->redraw_volume = true;
    bar->volume_icon_switch_tick = bar->battery_icon_switch_tick = current_tick;
    memset((void*)&(bar->lastinfo), 0, sizeof(struct status_info));
    bar->last_tm_min = 0;
}

static struct screen * sb_fill_bar_info(struct gui_statusbar * bar)
{
    struct screen *display = bar->display;

    if (!display)
        return display;

    bar->info.battlevel = battery_level();
#ifdef HAVE_USB_POWER
    bar->info.usb_inserted = usb_inserted();
#endif
    bar->info.inserted = (charger_input_state == CHARGER);
    if (bar->info.inserted)
    {
        bar->info.battery_state = true;

        /* zero battery run time if charging */
        if (charge_state > DISCHARGING)
            zero_runtime();

        /* animate battery if charging */
        if ((charge_state == DISCHARGING) || (charge_state == TRICKLE))
        {
            bar->info.batt_charge_step = -1;
        }
        else
        {
            /* animate in (max.) 4 steps, starting near the current charge level */
            if (TIME_AFTER(current_tick, bar->battery_icon_switch_tick))
            {
                if (++bar->info.batt_charge_step > 3)
                    bar->info.batt_charge_step = bar->info.battlevel / 34;
                bar->battery_icon_switch_tick = current_tick + HZ;
            }
        }
    }
    else
    {
        bar->info.batt_charge_step = -1;
        if (battery_level_safe())
            bar->info.battery_state = true;
        else
            /* blink battery if level is low */
            if (TIME_AFTER(current_tick, bar->battery_icon_switch_tick) &&
               (bar->info.battlevel > -1))
            {
                bar->info.battery_state = !bar->info.battery_state;
                bar->battery_icon_switch_tick = current_tick + HZ;
            }
    }
    bar->info.volume = global_status.volume;
    bar->info.shuffle = global_settings.playlist_shuffle;
    bar->info.keylock = is_keys_locked();
#ifdef HAS_REMOTE_BUTTON_HOLD
    bar->info.keylockremote = remote_button_hold();
#endif
    bar->info.repeat = global_settings.repeat_mode;
    bar->info.playmode = current_playmode();
    bar->time = get_time();
#ifdef HAVE_BLUETOOTH
    {
        enum bt_state st = bluetooth_get_state();
        bar->info.bluetooth = st == BT_STATE_CONNECTED ? 2 :
                              st == BT_STATE_OFF ? 0 : 1;
    }
#endif
    if(!display->has_disk_led)
        bar->info.led = led_read(HZ/2); /* delay should match polling interval */

    return display;
}

bool gui_statusbar_draw(struct gui_statusbar * bar, bool force_redraw, struct viewport *vp)
{
    struct screen * display = sb_fill_bar_info(bar);
    if (!display)
        return false;

    /* only redraw if forced to, or info has changed */
    if (force_redraw ||
        (bar->time->tm_min != bar->last_tm_min) ||
        memcmp(&(bar->info), &(bar->lastinfo), sizeof(struct status_info)))
    {
        struct ui_status st = {
            .playmode = bar->info.playmode,
            .keylock = bar->info.keylock,
            .time = valid_time(bar->time) ? bar->time : NULL,
            .clock_24h = !global_settings.timeformat,
            .battery = bar->info.battlevel,
            .battery_visible = bar->info.battery_state,
            .charging = bar->info.inserted,
#ifdef HAVE_BLUETOOTH
            .bluetooth = bar->info.bluetooth,
#endif
        };
        struct viewport *last_vp = display->set_viewport(vp);
        ui_status_draw(&st);
        display->set_viewport(last_vp);
        bar->last_tm_min = bar->time->tm_min;
        bar->lastinfo = bar->info;
        return true;
    }
    return false;
}

void statusbar_update(bool force)
{
    static long next_update;
    if (!force && TIME_BEFORE(current_tick, next_update))
        return;
    next_update = current_tick + HZ / 7; /* don't check too often */
    if (!lcd_active())
        return;

    struct viewport vp = { .buffer = NULL, .flags = VP_DEFAULT_FLAGS };
    viewport_set_fullscreen(&vp, SCREEN_MAIN);
    vp.height = UI_STATUS_H;
    if (gui_statusbar_draw(&statusbars.statusbars[SCREEN_MAIN], force, &vp))
        lcd_update_rect(0, 0, LCD_WIDTH, UI_STATUS_H);
}

void gui_syncstatusbar_init(struct gui_syncstatusbar * bars)
{
    FOR_NB_SCREENS(i) {
        gui_statusbar_init(&(screens[i]), &(bars->statusbars[i]));
    }
}
