/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2010 Jonathan Gordon
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
#include "config.h"

static bool yesno_pop_lines(const char *lines[], int line_cnt)
{
    const struct text_message message={lines, line_cnt};
    bool ret = (gui_syncyesno_run(&message,NULL,NULL)== YESNO_YES);
    FOR_NB_SCREENS(i)
        screens[i].clear_viewport();
    return ret;
}

/* YES/NO dialog, uses text parameter as prompt */
bool yesno_pop(const char* text)
{
    const char *lines[]= {text};
    return yesno_pop_lines(lines, 1);
}

/* YES/NO dialog, asks "Are you sure?", displays
   text parameter on second line.

   Says "Cancelled" if answered negatively.
*/
bool yesno_pop_confirm(const char* text)
{
    bool confirmed;
    const char *lines[] = {ID2P(LANG_ARE_YOU_SURE), text};
    confirmed = yesno_pop_lines(lines, 2);

    if (!confirmed)
        splash(HZ, ID2P(LANG_CANCEL));

    return confirmed;
}
