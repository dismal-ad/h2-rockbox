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

#include <stdio.h>
#include <stdbool.h>
#include "action.h"
#include "font.h"
#include "lang.h"
#include "usb.h"
#if defined(HAVE_USBSTACK)
#include "usb_core.h"
#ifdef USB_ENABLE_HID
#include "usb_keymaps.h"
#endif
#endif
#include "settings.h"
#include "appevents.h"
#include "usb_screen.h"
#include "viewport.h"
#include "playlist.h"
#include "misc.h"
#include "screen_access.h"
#include "newui/ui.h"

/* The new UI's USB screen (PLAN.md §4.8): a USB symbol, "Connected" and
 * a reminder to eject; with HID on, the keypad mode at the bottom (the
 * wheel changes it).
 *
 * The fonts' files are closed while the computer has the disk, so every
 * string is measured (its glyphs cached) before that. */

#ifdef USB_ENABLE_HID
int usb_keypad_mode;
#endif

/* the USB trident: stem with an arrowhead, a round foot, a branch ending in
 * a circle on the left and one ending in a square on the right */
static void draw_usb_symbol(int cx, int top)
{
    const unsigned c = UI_COL_TEXT;
    for (int i = 0; i < 12; i++)
        ui_fill_rect(cx - i * 2 / 3 - 1, top + i, i * 4 / 3 + 2, 1, c);
    ui_fill_rect(cx - 2, top + 12, 4, 50, c);
    ui_fill_round_rect(cx - 9, top + 58, 18, 18, 9, c);
    /* left: down from a circle, across to the stem */
    ui_fill_round_rect(cx - 27, top + 20, 12, 12, 6, c);
    ui_fill_rect(cx - 23, top + 30, 4, 16, c);
    ui_fill_round_rect(cx - 23, top + 42, 23, 4, 2, c);
    /* right: down from a square, across to the stem */
    ui_fill_rect(cx + 14, top + 14, 11, 11, c);
    ui_fill_rect(cx + 18, top + 24, 4, 14, c);
    ui_fill_round_rect(cx, top + 34, 22, 4, 2, c);
}

static void draw_usb_screen(struct viewport *vp)
{
    struct viewport *last_vp = lcd_set_viewport(vp);
    const int cx = vp->width / 2;
    const int title = ui_font(UI_FONT_TITLE), small = ui_font(UI_FONT_SMALL);
    const int w = vp->width - 2 * UI_MARGIN;

    ui_fill_rect(0, 0, vp->width, vp->height, UI_COL_BG);
    draw_usb_symbol(cx, 22);
    ui_text(UI_MARGIN, 112, title, UI_COL_TEXT,
            str(LANG_NEWUI_USB_CONNECTED), w, 0);
    ui_text(UI_MARGIN, 112 + font_get(title)->height + 4, small, UI_COL_TEXT2,
            str(LANG_NEWUI_USB_EJECT), w, 0);
#ifdef USB_ENABLE_HID
    if (global_settings.usb_hid)
        ui_text(UI_MARGIN, vp->height - font_get(small)->height - 6, small,
                UI_COL_MUTED, str(keypad_mode_name_get(usb_keypad_mode)), w, 0);
#endif
    lcd_update_viewport();
    lcd_set_viewport(last_vp);
}

/* caches the glyphs of everything draw_usb_screen() may show */
static void cache_strings(void)
{
    const int title = ui_font(UI_FONT_TITLE), small = ui_font(UI_FONT_SMALL);
    ui_text_width(title, str(LANG_NEWUI_USB_CONNECTED));
    ui_text_width(small, str(LANG_NEWUI_USB_EJECT));
#ifdef USB_ENABLE_HID
    int i = 0, langid = LANG_USB_KEYPAD_MODE;
    while (langid >= 0)
    {
        ui_text_width(small, str(langid));
        langid = keypad_mode_name_get(i++);
    }
#endif
}

static void handle_usb_events(struct viewport *vp)
{
    int button;

    /* Don't return until we get SYS_USB_DISCONNECTED */
    while(1)
    {
#ifdef USB_ENABLE_HID
        if (global_settings.usb_hid)
        {
            button = get_hid_usb_action();
            if (button == ACTION_USB_HID_MODE_SWITCH_NEXT ||
                button == ACTION_USB_HID_MODE_SWITCH_PREV)
                draw_usb_screen(vp);
        }
        else
#endif
        {
            (void)vp;
            /* hid emits the event in get_action */
            send_event(GUI_EVENT_ACTIONUPDATE, NULL);
            button = button_get_w_tmo(HZ/2);
        }
        if (button == SYS_USB_DISCONNECTED)
            return;
        if (button == SYS_CHARGER_DISCONNECTED)
            reset_runtime();
    }
}

void gui_usb_screen_run(bool early_usb, intptr_t seqnum)
{
#ifdef SIMULATOR /* the sim allows toggling USB fast enough to overflow viewportmanagers stack */
    static bool in_usb_screen = false;
    if (in_usb_screen)
        return;
    in_usb_screen = true;
#endif

    struct viewport vp;

    push_current_activity(ACTIVITY_USBSCREEN);

#ifdef USB_ENABLE_HID
    usb_keypad_mode = global_settings.usb_keypad_mode;
#endif

    /* we might be coming from anywhere, and the originating screen
     * can't be practically expected to cleanup the UI because
     * we're invoked via default_event_handler(), therefore we make a
     * generic cleanup here */
    lcd_set_viewport(NULL);
    lcd_scroll_stop();
    viewportmanager_theme_enable(SCREEN_MAIN, true, &vp);
    cache_strings();
    screens[SCREEN_MAIN].backlight_on();

    if(!early_usb)
    {
        /* The font system leaves the .fnt fd's open, so we need for force close them all */
        font_disable_all();
    }
    usb_acknowledge(SYS_USB_CONNECTED_ACK, seqnum);
    draw_usb_screen(&vp);
    handle_usb_events(&vp);

#if defined(USB_ENABLE_HID)
    if (global_settings.usb_keypad_mode != usb_keypad_mode)
    {
        global_settings.usb_keypad_mode = usb_keypad_mode;
        settings_save();
    }
#endif

    if(!early_usb)
    {
        font_enable_all();
        /* Not pretty, reload all settings so fonts are loaded again correctly */
        settings_apply(true);
        /* Reload playlist */
        playlist_resume();
    }

    screens[SCREEN_MAIN].backlight_on();
    viewportmanager_theme_undo(SCREEN_MAIN, false);

    pop_current_activity();
#ifdef SIMULATOR
    in_usb_screen = false;
#endif
}
