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

/* Home (PLAN.md §4.2): the menu in the left half, the playing album's
 * cover in the right half. Replaces the root menu's list.
 *
 * Wheel/Prev/Next move (wrapping), Centre opens, Back returns to the first
 * row, holding Back opens Now Playing. */

#include "config.h"
#include "action.h"
#include "audio.h"
#include "font.h"
#include "kernel.h"
#include "lang.h"
#include "language.h"
#include "lcd.h"
#include "menu.h"
#include "misc.h"
#include "root_menu.h"
#include "settings.h"
#include "viewport.h"
#include "ui.h"
#ifdef HAVE_BLUETOOTH
#include "menus/exported_menus.h"
#endif

#define HOME_MENU_W   160
#define HOME_ART      148
#define CHEVRON       "\xe2\x80\xba"

enum home_target {
    HOME_NOW_PLAYING,
    HOME_MUSIC,
    HOME_COVERFLOW,
    HOME_FILES,
    HOME_PLAYLISTS,
#ifdef HAVE_BLUETOOTH
    HOME_BLUETOOTH,
#endif
    HOME_SETTINGS,
    HOME_SYSTEM,
    HOME_COUNT
};

struct home_item {
    int lang_id;
    int screen;     /* GO_TO_* to return, or -1: handled here */
};

static const struct home_item items[HOME_COUNT] = {
    [HOME_NOW_PLAYING] = { LANG_NOW_PLAYING,  GO_TO_WPS },
    [HOME_MUSIC]       = { LANG_NEWUI_MUSIC,  GO_TO_DBBROWSER },
    [HOME_COVERFLOW]   = { LANG_NEWUI_COVERFLOW, -1 },
    [HOME_FILES]       = { LANG_DIR_BROWSER,  GO_TO_FILEBROWSER },
    [HOME_PLAYLISTS]   = { LANG_PLAYLISTS,    GO_TO_PLAYLISTS_SCREEN },
#ifdef HAVE_BLUETOOTH
    [HOME_BLUETOOTH]   = { LANG_BLUETOOTH,    -1 },
#endif
    [HOME_SETTINGS]    = { LANG_SETTINGS,     GO_TO_MAINMENU },
    [HOME_SYSTEM]      = { LANG_SYSTEM,       GO_TO_SYSTEM_SCREEN },
};

/* Now Playing is listed while a track is loaded, or one can be resumed */
static bool can_play(void)
{
    return audio_status() || global_status.resume_index != -1;
}

/* true when audio status or the playing track differs from the last call */
static bool state_changed(void)
{
    static int last_status = -1;
    static unsigned long last_hash;
    struct mp3entry *id3 = audio_status() ? audio_current_track() : NULL;
    unsigned long hash = 5381;
    if (id3)
        for (const char *p = id3->path; *p; p++)
            hash = hash * 33 + (unsigned char)*p;
    bool changed = audio_status() != last_status || hash != last_hash;
    last_status = audio_status();
    last_hash = hash;
    return changed;
}

static int visible_items(enum home_target *out)
{
    int n = 0;
    for (int i = 0; i < HOME_COUNT; i++)
    {
        if (i == HOME_NOW_PLAYING && !can_play())
            continue;
        out[n++] = i;
    }
    return n;
}

static const char *item_label(enum home_target t)
{
    if (t == HOME_NOW_PLAYING && !audio_status())
        return str(LANG_RESUME_PLAYBACK);
    return str(items[t].lang_id);
}

static void draw(struct viewport *vp, const enum home_target *list, int n,
                 int sel)
{
    struct viewport *last = lcd_set_viewport(vp);
    int font = ui_resolve_font(FONT_UI);

    ui_fill_rect(0, 0, vp->width, vp->height, UI_COL_BG);

    /* menu: Classic accent bar on the selection, chevron on it */
    for (int i = 0; i < n; i++)
    {
        int y = i * UI_ROW_H;
        bool selected = i == sel;
        unsigned fg = selected ? ui_sel_text() : UI_COL_TEXT;
        if (selected)
            ui_selection(0, y, HOME_MENU_W, UI_ROW_H);
        ui_text_vcenter(UI_MARGIN, y, UI_ROW_H, font, fg,
                        item_label(list[i]),
                        HOME_MENU_W - UI_MARGIN - 18, -1);
        if (selected)
            ui_text_vcenter(HOME_MENU_W - 14, y, UI_ROW_H, font, fg,
                            CHEVRON, 0, -1);
    }

    /* right half: the playing album's cover */
    int ax = HOME_MENU_W + (vp->width - HOME_MENU_W - HOME_ART) / 2;
    int ay = (vp->height - HOME_ART) / 2;
    const struct bitmap *art = ui_art_current(HOME_ART);
    if (art)
        ui_art_draw(art, ax, ay, HOME_ART, 8);
    else
        ui_art_placeholder(ax, ay, HOME_ART, 8);

    lcd_update_viewport();
    lcd_set_viewport(last);
}

/* *selected holds the selected item (enum home_target), not its row, so
 * the selection stays on the same item when Now Playing comes and goes */
int ui_home(int *selected)
{
    enum home_target list[HOME_COUNT];
    struct viewport vp;
    int n, sel = 0;
    int target = selected ? *selected : 0;
    bool redraw = true;
    int ret = GO_TO_ROOT;

    viewport_set_defaults(&vp, SCREEN_MAIN);

    while (true)
    {
        n = visible_items(list);
        sel = 0;
        for (int i = 0; i < n; i++)
            if ((int)list[i] == target)
                sel = i;
        target = list[sel];

        if (redraw)
        {
            draw(&vp, list, n, sel);
            redraw = false;
        }

        int action = get_action(CONTEXT_MAINMENU, HZ);
        switch (action)
        {
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                target = list[(sel + 1) % n];
                redraw = true;
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                target = list[(sel + n - 1) % n];
                redraw = true;
                break;
            case ACTION_STD_CANCEL:
                target = list[0];
                redraw = true;
                break;
            case ACTION_TREE_WPS:
                if (can_play())
                {
                    ret = GO_TO_WPS;
                    goto out;
                }
                break;
            case ACTION_STD_OK:
            {
                enum home_target t = list[sel];
                if (t == HOME_COVERFLOW)
                {
                    int r = ui_coverflow();
                    if (r != GO_TO_ROOT)
                    {
                        ret = r;
                        goto out;
                    }
                    redraw = true;
                    break;
                }
#ifdef HAVE_BLUETOOTH
                if (t == HOME_BLUETOOTH)
                {
                    if (do_menu(&bluetooth_menu, NULL, NULL, false) == MENU_ATTACHED_USB)
                    {
                        ret = MENU_ATTACHED_USB;
                        goto out;
                    }
                    redraw = true;
                    break;
                }
#endif
                ret = items[t].screen;
                goto out;
            }
            case ACTION_NONE:
                /* the status bar redraws itself; redraw Home when the
                   track or the play state changed (Now Playing, art) */
                if (state_changed())
                    redraw = true;
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = MENU_ATTACHED_USB;
                    goto out;
                }
                break;
        }
    }
out:
    if (selected)
        *selected = target;
    return ret;
}
