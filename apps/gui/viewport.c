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
#include "kernel.h"
#include "panic.h"
#include "language.h"
#include "statusbar.h"
#include "appevents.h"
#include "newui/ui.h"

/* The new UI has one layout: the status bar (UI_STATUS_H) on top and the
 * screen's area below it. A screen can hide the bar and take the whole
 * display (viewportmanager_theme_enable(screen, false, ...)); the calls
 * nest, the latest one counts. While the bar is shown it is redrawn when
 * what it shows changes (statusbar_update(), on every get_action()). */

#define FG_FALLBACK global_settings.fg_color
#define BG_FALLBACK global_settings.bg_color

#define VPSTACK_DEPTH 16
static int bar_stack_top = -1;
static bool bar_stack[VPSTACK_DEPTH];

static bool bar_shown(void)
{
    return bar_stack_top >= 0 && bar_stack[bar_stack_top];
}

static void bar_changed(bool force)
{
    static bool was_shown;
    bool shown = bar_shown();
    if (shown && (!was_shown || force))
        statusbar_update(true);
    else if (!shown)
        lcd_scroll_stop();
    was_shown = shown;
    /* lists set their viewports up again */
    send_event(GUI_EVENT_THEME_CHANGED, NULL);
}

void viewportmanager_theme_enable(enum screen_type screen, bool enable,
                                 struct viewport *viewport)
{
    if (++bar_stack_top >= VPSTACK_DEPTH-1)
        panicf("Stack overflow... viewportmanager");
    bar_stack[bar_stack_top] = enable;
    bar_changed(false);
    if (viewport)
        viewport_set_defaults(viewport, screen);
}

void viewportmanager_theme_undo(enum screen_type screen, bool force_redraw)
{
    (void)screen;
    if (--bar_stack_top < 0)
        panicf("Stack underflow... viewportmanager");
    bar_changed(force_redraw);
}

int viewport_get_nb_lines(const struct viewport *vp)
{
    return vp->height/font_get(vp->font)->height;
}

static void viewportmanager_update(unsigned short id, void* data)
{
    (void)id;
    (void)data;
    if (bar_shown())
        statusbar_update(false);
}

void viewportmanager_init(void)
{
    /* the bar is shown by default */
    viewportmanager_theme_enable(SCREEN_MAIN, true, NULL);
    add_event(GUI_EVENT_ACTIONUPDATE, viewportmanager_update);
}

void viewportmanager_theme_changed(const int which)
{
    (void)which;
    bar_changed(true);
}

void viewport_set_fullscreen(struct viewport *vp,
                              const enum screen_type screen)
{
    screens[screen].init_viewport(vp);
    vp->x = 0;
    vp->y = 0;
    vp->width = screens[screen].lcdwidth;
    vp->height = screens[screen].lcdheight;
    vp->flags &= ~VP_FLAG_ALIGNMENT_MASK;
    if (UNLIKELY(lang_is_rtl()))
        vp->flags |= VP_FLAG_ALIGN_RIGHT;
    vp->font = screens[screen].getuifont();
    vp->drawmode = DRMODE_SOLID;
    vp->fg_pattern = FG_FALLBACK;
    vp->bg_pattern = BG_FALLBACK;
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
    viewport_set_fullscreen(vp, screen);
    if (bar_shown())
    {
        vp->y = UI_STATUS_H;
        vp->height -= UI_STATUS_H;
    }
}
