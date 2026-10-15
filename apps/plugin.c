/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 Björn Stenberg
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
#include "plugin.h"
#include "open_plugin.h"
#include "lang.h"
#include "splash.h"

/* Plugins are not supported in this tree. What remains is the plugin buffer,
 * which core code borrows as scratch memory, and loader entry points that
 * report failure. */

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)
static unsigned char pluginbuf[PLUGIN_BUFFER_SIZE];
#else
extern unsigned char pluginbuf[];
#endif

int plugin_load(const char* plugin, const void* parameter)
{
    (void)plugin;
    (void)parameter;
    splashf(HZ*2, "%s", str(LANG_PLUGIN_ERROR));
    return PLUGIN_ERROR;
}

/* Kept for API compatibility; no plugin is ever loaded, so nothing is locked */
size_t plugin_reserve_buffer(size_t buffer_size)
{
    (void)buffer_size;
    return PLUGIN_BUFFER_SIZE;
}

/* Returns the entire plugin buffer (no plugin is ever loaded) */
void* plugin_get_buffer(size_t *buffer_size)
{
    *buffer_size = PLUGIN_BUFFER_SIZE;
    return pluginbuf;
}

int plugin_open(const char *plugin, const char *parameter)
{
    open_plugin_add_path(ID2P(LANG_OPEN_PLUGIN), plugin, parameter);
    return PLUGIN_GOTO_PLUGIN;
}

char *plugin_get_current_filename(void)
{
    return "";
}
