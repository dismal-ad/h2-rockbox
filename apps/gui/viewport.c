/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2008 by Jonathan Gordon
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

#include <stdlib.h>
#include "config.h"
#include "lcd.h"
#include "font.h"
#include "viewport.h"
#include "screen_access.h"
#include "settings.h"
#include "misc.h"
#include "list.h"

/*some short cuts for fg/bg/line selector handling */
#define FG_FALLBACK global_settings.fg_color
#define BG_FALLBACK global_settings.bg_color

/* all below isn't needed for pc tools (i.e. checkwps/wps editor)
 * only viewport_parse_viewport() is */
#ifndef __PCTOOL__
#include "string.h"
#include "kernel.h"
#include "system.h"
#include "statusbar.h"
#include "appevents.h"
#include "panic.h"
#include "language.h"
#include "statusbar-skinned.h"
#include "skin_engine/skin_engine.h"
#include "debug.h"

#define VPSTACK_DEPTH 16
struct viewport_stack_item
{
    struct  viewport* vp;
    bool   enabled;
};

static void viewportmanager_redraw(unsigned short id, void* data);

static int theme_stack_top[NB_SCREENS]; /* the last item added */
static struct viewport_stack_item theme_stack[NB_SCREENS][VPSTACK_DEPTH];
static bool is_theme_enabled(enum screen_type screen);

static void evt_toggle(bool enable, unsigned short id,
                         void (*handler)(unsigned short id, void *data))
{
    if (enable)
        add_event(id, handler);
    else
        remove_event(id, handler);
}

static void toggle_events(bool enable)
{
    evt_toggle(enable, GUI_EVENT_ACTIONREDRAW, viewportmanager_redraw);
    evt_toggle(enable, PLAYBACK_EVENT_TRACK_CHANGE, do_sbs_update_callback);
    evt_toggle(enable, PLAYBACK_EVENT_NEXTTRACKID3_AVAILABLE, do_sbs_update_callback);
    evt_toggle(enable, LCD_EVENT_ACTIVATION, do_sbs_update_callback);
}

static void set_clear_valid_vp(enum screen_type screen, struct viewport *vp)
{
    if (vp->width && vp->height)
    {
        screens[screen].set_viewport(vp);
        screens[screen].clear_viewport();
    }
}

static void toggle_theme(enum screen_type screen, bool force)
{
    bool enable_event = false;
    static bool was_enabled[NB_SCREENS] = {false};
    static bool after_boot[NB_SCREENS] = {false};
    struct viewport *last_vp;

    FOR_NB_SCREENS(i)
    {
        enable_event = enable_event || is_theme_enabled(i);
        if (!sb_get_persistent_title(i))
            sb_set_title_text(NULL, Icon_NOICON, i);
    }
    skinlist_set_cfg(screen, NULL);
    toggle_events(enable_event);

    if (is_theme_enabled(screen))
    {
        last_vp = screens[screen].set_viewport(NULL);
        bool first_boot = theme_stack_top[screen] == 0;
        /* Remove the left overs from the previous screen */
        skin_backdrop_show(sb_get_backdrop(screen));
        if (LIKELY(after_boot[screen]) && (!was_enabled[screen] || force))
        {
            struct viewport deadspace, user;
            viewport_set_defaults(&user, screen);
            deadspace = user; /* get colours and everything */
            /* above */
            deadspace.x = 0;
            deadspace.y = 0;
            deadspace.width = screens[screen].lcdwidth;
            deadspace.height = user.y;
            set_clear_valid_vp(screen, &deadspace);
            /* below */
            deadspace.y = user.y + user.height;
            deadspace.height = screens[screen].lcdheight - deadspace.y;
            set_clear_valid_vp(screen, &deadspace);
            /* left */
            deadspace.x = 0;
            deadspace.y = 0;
            deadspace.width = user.x;
            deadspace.height = screens[screen].lcdheight;
            set_clear_valid_vp(screen, &deadspace);
            /* below */
            deadspace.x = user.x + user.width;
            deadspace.width = screens[screen].lcdwidth - deadspace.x;
            set_clear_valid_vp(screen, &deadspace);

            screens[screen].set_viewport(last_vp);
        }
        intptr_t force = first_boot?0:1;

        send_event(GUI_EVENT_ACTIONREDRAW, (void*)force);
        if (!first_boot)
            sb_skin_force_next_update();
    }
    else
    {
        screens[screen].backdrop_show(NULL);
        screens[screen].scroll_stop();
        skin_is_dirty(screen); /* remove dirty flag */
    }
    /* let list initialize viewport in case viewport dimensions is changed. */
    send_event(GUI_EVENT_THEME_CHANGED, NULL);
    FOR_NB_SCREENS(i)
        was_enabled[i] = is_theme_enabled(i);
    after_boot[screen] = true;
}

void viewportmanager_theme_enable(enum screen_type screen, bool enable,
                                 struct viewport *viewport)
{
    int top = ++theme_stack_top[screen];
    if (top >= VPSTACK_DEPTH-1)
        panicf("Stack overflow... viewportmanager");
    theme_stack[screen][top].enabled = enable;
    theme_stack[screen][top].vp = viewport;
    toggle_theme(screen, false);
    /* then be nice and set the viewport up */
    if (viewport)
        viewport_set_defaults(viewport, screen);
}

void viewportmanager_theme_undo(enum screen_type screen, bool force_redraw)
{
    int top = --theme_stack_top[screen];
    if (top < 0)
        panicf("Stack underflow... viewportmanager");

    toggle_theme(screen, force_redraw);
    if (sb_get_persistent_title(screen))
        screens[screen].scroll_stop();
}


static bool is_theme_enabled(enum screen_type screen)
{
    int top = theme_stack_top[screen];
    return theme_stack[screen][top].enabled;
}

int viewport_get_nb_lines(const struct viewport *vp)
{
    return vp->height/font_get(vp->font)->height;
}

static void viewportmanager_redraw(unsigned short id, void* data)
{
    (void)id;
    FOR_NB_SCREENS(i)
        if (is_theme_enabled(i))
            sb_skin_update(i, NULL != data);
}

static void viewportmanager_update(unsigned short id, void* data)
{
    viewportmanager_redraw(id, data);
    FOR_NB_SCREENS(i)
        if (skin_is_dirty(i))
            screens[i].update();
}

void viewportmanager_init(void)
{
    FOR_NB_SCREENS(i)
    {
        theme_stack_top[i] = -1; /* the next call fixes this to 0 */
        /* We always want the theme enabled by default... */
        viewportmanager_theme_enable(i, true, NULL);
    }
    add_event(GUI_EVENT_ACTIONUPDATE, viewportmanager_update);
}

void viewportmanager_theme_changed(const int which)
{
    if (which & THEME_LANGUAGE)
    {
    }
    if (which & (THEME_STATUSBAR|THEME_UI_VIEWPORT))
    {
        FOR_NB_SCREENS(i)
        {
            /* This can probably be done better...
             * disable the theme so it's forced to do a full redraw  */
            viewportmanager_theme_enable(i, false, NULL);
            viewportmanager_theme_undo(i, true);
        }
    }
    send_event(GUI_EVENT_THEME_CHANGED, NULL);
}

static void set_default_align_flags(struct viewport *vp)
{
    vp->flags &= ~VP_FLAG_ALIGNMENT_MASK;
    if (UNLIKELY(lang_is_rtl()))
        vp->flags |= VP_FLAG_ALIGN_RIGHT;
}

#endif /* __PCTOOL__ */

void viewport_set_fullscreen(struct viewport *vp,
                              const enum screen_type screen)
{
    screens[screen].init_viewport(vp);
    vp->x = 0;
    vp->y = 0;
    vp->width = screens[screen].lcdwidth;
    vp->height = screens[screen].lcdheight;
#ifndef __PCTOOL__
    set_default_align_flags(vp);
#endif
    vp->font = screens[screen].getuifont();
    vp->drawmode = DRMODE_SOLID;
    {
        vp->fg_pattern = FG_FALLBACK;
        vp->bg_pattern = BG_FALLBACK;
    }

}

void viewport_set_buffer(struct viewport *vp, struct frame_buffer_t *buffer,
                                                const enum screen_type screen)
{
    if (!vp) /* NULL vp grabs current framebuffer */
        vp = *(screens[screen].current_viewport);

    /* NULL sets default buffer */
    if (buffer && buffer->elems == 0)
        vp->buffer = NULL;
    else
        vp->buffer = buffer;
    screens[screen].init_viewport(vp);
}

void viewport_set_defaults(struct viewport *vp,
                            const enum screen_type screen)
{
    vp->buffer = NULL; /* use default frame_buffer */
    vp->flags = VP_DEFAULT_FLAGS;
#if !defined(__PCTOOL__)
    struct viewport *sbs_area = NULL;
    if (!is_theme_enabled(screen))
    {
        viewport_set_fullscreen(vp, screen);
        return;
    }
    sbs_area = sb_skin_get_info_vp(screen);

    if (sbs_area)
        *vp = *sbs_area;
    else
#endif /* !__PCTOOL__ */
        viewport_set_fullscreen(vp, screen);
}
