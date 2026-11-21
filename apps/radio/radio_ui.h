/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
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

#ifndef RADIO_UI_H
#define RADIO_UI_H

#include <stdbool.h>

/* The Radio screen (Home). Returns 0, GO_TO_WPS when a station started,
 * or GO_TO_ROOT after USB. */
int radio_screen(void);
/* The Wi-Fi networks screen */
int wifi_screen(void);

/* After playlist_resume(): true if the track to resume is a station */
bool radio_resume_is_station(void);
/* Plays the last station again; true once it plays */
bool radio_resume_last(void);

#endif /* RADIO_UI_H */
