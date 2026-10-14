/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 Ray Lambert
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
#ifndef _ABREPEAT_H_
#define _ABREPEAT_H_
#include <stdbool.h>

static inline bool ab_repeat_mode_enabled(void)
{
    return false;
}
static inline bool ab_bool_dummy_marker(unsigned int song_position)
{
    (void) song_position;
    return false;
}
static inline void ab_void_dummy_marker(unsigned int song_position)
{
    (void) song_position;
}
static inline void ab_dummy_voidfn(void){}

#define ab_before_A_marker     ab_bool_dummy_marker
#define ab_after_A_marker      ab_bool_dummy_marker
#define ab_jump_to_A_marker    ab_dummy_voidfn
#define ab_reset_markers       ab_dummy_voidfn
#define ab_set_A_marker        ab_void_dummy_marker
#define ab_set_B_marker        ab_void_dummy_marker
#define ab_get_A_marker        ab_bool_dummy_marker
#define ab_get_B_marker        ab_bool_dummy_marker
#define ab_end_of_track_report ab_dummy_voidfn
#define ab_reached_B_marker    ab_bool_dummy_marker
#define ab_position_report     ab_void_dummy_marker

#endif /* _ABREPEAT_H_ */
