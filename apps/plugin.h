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
#ifndef _PLUGIN_H_
#define _PLUGIN_H_

/* instruct simulator code to not redefine any symbols when compiling plugins.
   (the PLUGIN macro is defined in apps/plugins/Makefile) */
#ifdef PLUGIN
#define NO_REDEFINES_PLEASE
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string-extra.h"
#include "gcc_extensions.h"



/* on some platforms strcmp() seems to be a tricky define which
 * breaks if we write down strcmp's prototype */
#undef strcmp
#undef strncmp
#undef strchr
#undef strstr
#undef strrchr
#undef strtok_r
#ifdef __APPLE__
#undef strncpy
#undef snprintf
#undef strcpy
#undef strcat
#undef memset
#undef memcpy
#undef memmove
#undef vsnprintf
#undef vsprintf
#endif

#define splash(__ticks, __str) splashf(__ticks, __str)

void* plugin_get_buffer(size_t *buffer_size);
size_t plugin_reserve_buffer(size_t buffer_size);
int plugin_open(const char *plugin, const char *parameter);

#ifndef __PCTOOL__
#include "config.h"
#include "system.h"
#include "dir.h"
#include "general.h"
#include "kernel.h"
#include "thread.h"
#include "button.h"
#include "action.h"
#include "gesture.h"
#include "load_code.h"
#include "usb.h"
#include "font.h"
#include "lcd.h"
#include "scroll_engine.h"
#include "metadata.h"
#include "sound.h"
#include "audio.h"
#include "hw_h264.h"
#include "voice_thread.h"
#include "root_menu.h"
#include "talk.h"
#include "lang_enum.h"
#ifdef RB_PROFILE
#include "profile.h"
#endif
#include "misc.h"
#include "pathfuncs.h"
#include "pcm_mixer.h"
#include "pcm_sink.h"
#include "dsp-util.h"
#include "dsp_core.h"
#include "dsp_proc_settings.h"
#include "codecs.h"
#include "playback.h"
#include "codec_thread.h"
#include "settings.h"
#include "timer.h"
#include "playlist.h"
#include "screendump.h"
#include "scrollbar.h"
#include "jpeg_load.h"
#include "../recorder/bmp.h"
#include "menu.h"
#include "rbunicode.h"
#include "list.h"
#include "statusbar-skinned.h"
#include "tree.h"
#include "color_picker.h"
#include "buflib.h"
#include "buffering.h"
#include "tagcache.h"
#include "tagtree.h"
#include "viewport.h"
#include "ata_idle_notify.h"
#include "settings_list.h"
#include "timefuncs.h"
#include "crc32.h"
#include "rbpaths.h"
#include "core_alloc.h"
#include "screen_access.h"
#include "onplay.h"
#include "screens.h"
#include "vuprintf.h"

#ifdef HAVE_ALBUMART
#include "albumart.h"
#endif

#include "yesno.h"

#include "filetypes.h"

#ifdef USB_ENABLE_HID
#include "usbstack/usb_hid_usage_tables.h"
#endif

/* plugin return codes */
/* internal returns start at 0x100 to make exit(1..255) work */
#define INTERNAL_PLUGIN_RETVAL_START 0x100
enum plugin_status {
    PLUGIN_OK = 0, /* PLUGIN_OK == EXIT_SUCCESS */
    /* 1...255 reserved for exit() */
    PLUGIN_USB_CONNECTED = INTERNAL_PLUGIN_RETVAL_START,
    PLUGIN_POWEROFF,
    PLUGIN_GOTO_WPS,
    PLUGIN_GOTO_PLUGIN,
    PLUGIN_GOTO_ROOT,
    PLUGIN_ERROR = -1,
};

/*
 * The str() macro/functions is how to access strings that might be
 * translated. Use it like str(MACRO) and expect a string to be
 * returned!
 */
#define str(x) language_strings[x]

/* Plugins are not supported in this tree: plugin_load() always fails.
 * The plugin buffer remains available as scratch memory for core code. */
int plugin_load(const char* plugin, const void* parameter);
char *plugin_get_current_filename(void);

#endif /* __PCTOOL__ */
#endif
