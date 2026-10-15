/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026
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
#include "font.h"
#include "rbpaths.h"
#include "settings.h"
#include "ui.h"

/* Source Sans 3, installed by buildzip from fonts/newui */
static const char * const font_files[UI_FONT_COUNT] = {
    [UI_FONT_SMALL] = FONT_DIR "/newui-14.fnt",
    [UI_FONT_BODY]  = FONT_DIR "/newui-18.fnt",
    [UI_FONT_ROW]   = FONT_DIR "/newui-20bold.fnt",
    [UI_FONT_TITLE] = FONT_DIR "/newui-22bold.fnt",
    [UI_FONT_BIG]   = FONT_DIR "/newui-40bold.fnt",
};

static int font_ids[UI_FONT_COUNT];
static bool loaded;

void ui_font_init(void)
{
    if (loaded)
        return;
    for (int i = 0; i < UI_FONT_COUNT; i++)
    {
        int id = font_load(font_files[i]);
        font_ids[i] = id >= 0 ? id : FONT_SYSFIXED;
    }
    loaded = true;
    /* Unless pinned, FONT_UI means "the highest loaded font", which would
     * now be one of ours: pin it to the theme's UI font again */
    set_ui_font(global_status.font_id[SCREEN_MAIN]);
}

int ui_resolve_font(int font)
{
    return font == FONT_UI ? global_status.font_id[SCREEN_MAIN] : font;
}

int ui_font(enum ui_font f)
{
    if (!loaded)
        ui_font_init();
    return font_ids[f];
}
