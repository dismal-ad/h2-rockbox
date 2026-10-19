/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Kevin Ferrare
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

#ifndef _GUI_STATUSBAR_H_
#define _GUI_STATUSBAR_H_

#include "config.h"
#include "button.h"
#include "status.h"
#include "screen_access.h"
#include "events.h"

struct status_info {
    int battlevel;
    int batt_charge_step;
    int volume;
    int playmode;
    int repeat;
    bool inserted;
    bool charging; /* the battery is taking charge, not just powered */
#ifdef HAVE_USB_POWER
    bool usb_inserted;
#endif
    bool battery_state;
    bool shuffle;
    bool keylock;
#ifdef HAS_REMOTE_BUTTON_HOLD
    bool keylockremote;
#endif
    bool led; /* disk LED simulation in the status bar */
#ifdef HAVE_BLUETOOTH
    int bluetooth; /* 0 off, 1 on, 2 connected */
#endif

};

/* statusbar visibility/position, used for settings also */
enum statusbar_values { STATUSBAR_OFF = 0, STATUSBAR_TOP, STATUSBAR_BOTTOM };

struct gui_statusbar
{
    struct status_info info;
    struct status_info lastinfo;

    bool redraw_volume; /* true if the volume gauge needs updating */
    int last_volume;
    long volume_icon_switch_tick;

    long battery_icon_switch_tick;

    struct tm *time;
    int last_tm_min;
    struct screen * display;
};


extern struct gui_syncstatusbar statusbars;

/*
 * Attach the status bar to a screen
 * (The previous screen attachement is lost)
 *  - bar : the statusbar structure
 *  - display : the screen to attach
 */
#define gui_statusbar_set_screen(gui_statusbar, _display) \
    (gui_statusbar)->display = (_display);


/*
 * Draws the status bar on the attached screen
 * - bar : the statusbar structure
 */
/* true if it drew (something changed, or forced) */
extern bool gui_statusbar_draw(struct gui_statusbar * bar,
                               bool force_redraw, struct viewport *vp);

/* new UI: redraws the status bar on top of the screen if what it shows
 * changed (checked at most every HZ/7) or force */
void statusbar_update(bool force);


struct gui_syncstatusbar
{
    struct gui_statusbar statusbars[NB_SCREENS];
};

extern void gui_syncstatusbar_init(struct gui_syncstatusbar * bars) INIT_ATTR;


#endif /*_GUI_STATUSBAR_H_*/
