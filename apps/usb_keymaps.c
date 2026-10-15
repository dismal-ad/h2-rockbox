/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2009 Tomer Shalev
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

#include "action.h"
#include "lang.h"
#include "misc.h"
#include "usbstack/usb_hid.h"
#include "usb_keymaps.h"
/*#define LOGF_ENABLE*/
#include "logf.h"

#define MODIFIER(modifier, key) ((key) | (modifier << 8))

#define ALT(key) MODIFIER(HID_KEYBOARD_LEFT_ALT, key)
#define CTRL(key) MODIFIER(HID_KEYBOARD_LEFT_CONTROL, key)
#define SHIFT(key) MODIFIER(HID_KEYBOARD_LEFT_SHIFT, key)

typedef struct
{
    int action;
    int id;
} mapping_t;

typedef struct
{
    int lang_name;
    int context;
    usage_page_t usage_page;
    mapping_t mapping[];
} hid_key_mapping_t;

static const hid_key_mapping_t hid_key_mapping_multimedia = {
    LANG_MULTIMEDIA_MODE,
    CONTEXT_USB_HID_MODE_MULTIMEDIA,
    HID_USAGE_PAGE_CONSUMER,
    {
        { ACTION_USB_HID_MULTIMEDIA_VOLUME_UP,           HID_CONSUMER_USAGE_VOLUME_INCREMENT },
        { ACTION_USB_HID_MULTIMEDIA_VOLUME_DOWN,         HID_CONSUMER_USAGE_VOLUME_DECREMENT },
        { ACTION_USB_HID_MULTIMEDIA_VOLUME_MUTE,         HID_CONSUMER_USAGE_MUTE },
        { ACTION_USB_HID_MULTIMEDIA_PLAYBACK_PLAY_PAUSE, HID_CONSUMER_USAGE_PLAY_PAUSE },
        { ACTION_USB_HID_MULTIMEDIA_PLAYBACK_STOP,       HID_CONSUMER_USAGE_STOP },
        { ACTION_USB_HID_MULTIMEDIA_PLAYBACK_TRACK_PREV, HID_CONSUMER_USAGE_SCAN_PREVIOUS_TRACK },
        { ACTION_USB_HID_MULTIMEDIA_PLAYBACK_TRACK_NEXT, HID_CONSUMER_USAGE_SCAN_NEXT_TRACK },
        { 0,                     0 },
    }
};

static const hid_key_mapping_t hid_key_mapping_presentation = {
    LANG_PRESENTATION_MODE,
    CONTEXT_USB_HID_MODE_PRESENTATION,
    HID_USAGE_PAGE_KEYBOARD_KEYPAD,
    {
        { ACTION_USB_HID_PRESENTATION_SLIDESHOW_START, HID_KEYBOARD_F5 },
        { ACTION_USB_HID_PRESENTATION_SLIDESHOW_LEAVE, HID_KEYBOARD_ESCAPE },
        { ACTION_USB_HID_PRESENTATION_SLIDE_PREV,      HID_KEYBOARD_P },
        { ACTION_USB_HID_PRESENTATION_SLIDE_NEXT,      HID_KEYBOARD_N },
        { ACTION_USB_HID_PRESENTATION_SLIDE_FIRST,     HID_KEYBOARD_HOME },
        { ACTION_USB_HID_PRESENTATION_SLIDE_LAST,      HID_KEYBOARD_END },
        { ACTION_USB_HID_PRESENTATION_SCREEN_BLACK,    HID_KEYBOARD_DOT },
        { ACTION_USB_HID_PRESENTATION_SCREEN_WHITE,    HID_KEYBOARD_COMMA },
        { ACTION_USB_HID_PRESENTATION_LINK_PREV,       SHIFT(HID_KEYBOARD_TAB) },
        { ACTION_USB_HID_PRESENTATION_LINK_NEXT,       HID_KEYBOARD_TAB },
        { ACTION_USB_HID_PRESENTATION_MOUSE_CLICK,     HID_KEYBOARD_RETURN },
        { ACTION_USB_HID_PRESENTATION_MOUSE_OVER,      SHIFT(HID_KEYBOARD_RETURN) },
        { 0,                          0 },
    }
};

static const hid_key_mapping_t hid_key_mapping_browser = {
    LANG_BROWSER_MODE,
    CONTEXT_USB_HID_MODE_BROWSER,
    HID_USAGE_PAGE_KEYBOARD_KEYPAD,
    {
        { ACTION_USB_HID_BROWSER_SCROLL_UP,        HID_KEYBOARD_UP_ARROW },
        { ACTION_USB_HID_BROWSER_SCROLL_DOWN,      HID_KEYBOARD_DOWN_ARROW },
        { ACTION_USB_HID_BROWSER_SCROLL_UP,        HID_KEYBOARD_PAGE_UP },
        { ACTION_USB_HID_BROWSER_SCROLL_DOWN,      HID_KEYBOARD_PAGE_DOWN },
        { ACTION_USB_HID_BROWSER_ZOOM_IN,          CTRL(HID_KEYPAD_PLUS) },
        { ACTION_USB_HID_BROWSER_ZOOM_OUT,         CTRL(HID_KEYPAD_MINUS) },
        { ACTION_USB_HID_BROWSER_ZOOM_RESET,       CTRL(HID_KEYPAD_0_AND_INSERT) },
        { ACTION_USB_HID_BROWSER_TAB_PREV,         CTRL(HID_KEYBOARD_PAGE_UP) },
        { ACTION_USB_HID_BROWSER_TAB_NEXT,         CTRL(HID_KEYBOARD_PAGE_DOWN) },
        { ACTION_USB_HID_BROWSER_TAB_CLOSE,        CTRL(HID_KEYBOARD_W) },
        { ACTION_USB_HID_BROWSER_HISTORY_BACK,     ALT(HID_KEYBOARD_LEFT_ARROW) },
        { ACTION_USB_HID_BROWSER_HISTORY_FORWARD,  ALT(HID_KEYBOARD_RIGHT_ARROW) },
        { ACTION_USB_HID_BROWSER_VIEW_FULL_SCREEN, HID_KEYBOARD_F11 },
        { 0,                          0 },
    }
};

static const hid_key_mapping_t *hid_key_mappings[] =
{
    &hid_key_mapping_multimedia,
    &hid_key_mapping_presentation,
    &hid_key_mapping_browser,
};

extern int usb_keypad_mode;

int get_hid_usb_action(void)
{
    int action, step;
    const hid_key_mapping_t *key_mapping = hid_key_mappings[usb_keypad_mode];

    step = -1;
    action = get_action(key_mapping->context, HZ/4);
    switch (action)
    {
        case ACTION_USB_HID_MODE_SWITCH_NEXT:
            step = 1;
            /* fallthrough */
        case ACTION_USB_HID_MODE_SWITCH_PREV:
            /* Switch key mappings in a cyclic way */
            usb_keypad_mode = clamp_value_wrap(usb_keypad_mode + step,
                    ARRAYLEN(hid_key_mappings) - 1, 0);
            break;
        default:
            {
                const mapping_t *mapping;
                const hid_key_mapping_t *key_mapping =
                    hid_key_mappings[usb_keypad_mode];

                if (action <= ACTION_USB_HID_FIRST ||
                        action >= ACTION_USB_HID_LAST)
                {
                    break;
                }

                for (mapping = key_mapping->mapping; mapping->action; mapping++)
                {
                    if (action == mapping->action)
                    {
                        logf("Action %d", action);
                        usb_hid_send(key_mapping->usage_page, mapping->id);
                        break;
                    }
                }
#ifdef DEBUG
                if (!mapping->action)
                    logf("Action %d not found", action);
#endif
                break;
            }
    }

    return action;
}

int keypad_mode_name_get(unsigned int mode)
{
    if (mode >= ARRAYLEN(hid_key_mappings))
        return -1;
    return hid_key_mappings[mode]->lang_name;
}
