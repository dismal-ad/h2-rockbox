/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Kevin Ferrare
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
#include <stdio.h>
#include <string.h>
#include "yesno.h"
#include "system.h"
#include "kernel.h"
#include "misc.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "viewport.h"
#include "splash.h"
#include "backlight.h"
#include "font.h"
#include "newui/ui.h"

/* The new UI's question panel (PLAN.md §4.8): over the dimmed screen, an
 * optional title, the question, and Yes / No as a two-row list. The wheel
 * or Prev/Next choose, Centre confirms, Back answers No. */

#define PANEL_VPAD  14

struct gui_yesno
{
    struct viewport vp;
    const char *title;
    const struct text_message *main_message;
    int sel;                    /* 0: Yes, 1: No */
    /* timeout data */
    long end_tick;
    enum yesno_res tmo_default_res;
};

static void gui_yesno_draw(struct gui_yesno *yn)
{
    static struct ui_lines lines;
    struct viewport *vp = &yn->vp;
    struct viewport *last_vp = lcd_set_viewport(vp);
    const int font = ui_font(UI_FONT_ROW), tfont = ui_font(UI_FONT_TITLE);
    const int lh = font_get(font)->height, th = font_get(tfont)->height;
    const int max_w = vp->width - 2 * UI_MARGIN;

    lines.count = lines.widest = 0;
    for (int i = 0; i < yn->main_message->nb_lines; i++)
        ui_wrap(&lines, P2STR((unsigned char *)yn->main_message->message_lines[i]),
                font, max_w - 2 * UI_POPUP_PAD);
    int title_h = yn->title ? th + 4 : 0;
    int widest = MAX(lines.widest, yn->title ? ui_text_width(tfont, yn->title) : 0);

    int w = MIN(MAX(widest, 180) + 2 * UI_POPUP_PAD, max_w);
    int h = PANEL_VPAD + title_h + lines.count * lh + UI_GAP + 2 * UI_ROW_H + 8;
    if (h > vp->height)
    {
        lines.count -= (h - vp->height + lh - 1) / lh;
        h = vp->height;
    }
    int x = (vp->width - w) / 2, y = (vp->height - h) / 2;
    int tw = w - 2 * UI_POPUP_PAD;

    ui_panel(x, y, w, h);
    int ty = y + PANEL_VPAD;
    if (yn->title)
    {
        ui_text(x + UI_POPUP_PAD, ty, tfont, UI_COL_TEXT, yn->title, tw, 0);
        ty += title_h;
    }
    for (int i = 0; i < lines.count; i++, ty += lh)
        ui_text(x + UI_POPUP_PAD, ty, font, UI_COL_TEXT, lines.line[i], tw, 0);

    /* the answers; a countdown after the one a timeout picks */
    int ry = y + h - 8 - 2 * UI_ROW_H;
    for (int i = 0; i < 2; i++, ry += UI_ROW_H)
    {
        char buf[48];
        const char *label = str(i == 0 ? LANG_SET_BOOL_YES : LANG_SET_BOOL_NO);
        enum yesno_res res = i == 0 ? YESNO_YES : YESNO_NO;
        if (yn->tmo_default_res == res)
        {
            int left = MAX(0, (int)(yn->end_tick - current_tick) / HZ);
            snprintf(buf, sizeof(buf), "%s (%d)", label, left);
            label = buf;
        }
        if (i == yn->sel)
            ui_fill_round_rect(x + 8, ry, w - 16, UI_ROW_H, 6, UI_COL_ACCENT);
        ui_text_vcenter(x + UI_POPUP_PAD, ry, UI_ROW_H, font,
                        i == yn->sel ? UI_COL_TEXT : UI_COL_TEXT2, label, tw, -1);
    }
    lcd_update_viewport();
    lcd_set_viewport(last_vp);
}

static void dim_behind(struct gui_yesno *yn)
{
    struct viewport *last_vp = lcd_set_viewport(&yn->vp);
    ui_dim();
    lcd_update_viewport();
    lcd_set_viewport(last_vp);
}

/* the answer's message, as a splash */
static void show_result(const struct text_message *message)
{
    char buf[256] = "";
    for (int i = 0; i < message->nb_lines; i++)
    {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "%s%s", i ? "\n" : "",
                 P2STR((unsigned char *)message->message_lines[i]));
    }
    splash(HZ, buf);
}

/* Display a YES_NO prompt to the user
 *
 * ticks < HZ will be ignored and the prompt will be blocking
 * tmo_default_res is the answer that is returned when the timeout expires
 * a default result of YESNO_TMO will also make the prompt blocking
 * if tmo_default_res is YESNO_YES or YESNO_NO a seconds countdown will
 * be present next to the default option
 *
 *        ticks - timeout if (>=HZ) otherwise ignored
 *  default_res - result returned on timeout YESNO_TMO creates a blocking prompt
 * main_message - prompt to the user
 *  yes_message - displayed when YESNO_YES is choosen
 *   no_message - displayed when YESNO_NO is choosen
*/
enum yesno_res gui_syncyesno_run_w_tmo(int ticks, enum yesno_res tmo_default_res,
                                       const char * title,
                                       const struct text_message * main_message,
                                       const struct text_message * yes_message,
                                       const struct text_message * no_message)
{
    #define YESNO_NONE (-1)
    int action;
    int result = YESNO_NONE;
    struct gui_yesno yn;

    if (ticks < HZ) /* Display a prompt with NO timeout to the user */
        tmo_default_res = YESNO_TMO;

    yn.end_tick = current_tick + ticks;
    yn.tmo_default_res = tmo_default_res;
    yn.title = title;
    yn.main_message = main_message;
    yn.sel = tmo_default_res == YESNO_NO ? 1 : 0;
    viewport_set_defaults(&yn.vp, SCREEN_MAIN);

    lcd_scroll_stop();
    dim_behind(&yn);

    /* make sure to eat any extranous keypresses */
    action_wait_for_release();

    while (result == YESNO_NONE)
    {
        gui_yesno_draw(&yn);

        bool backlight_on = is_backlight_on(false);
        action = get_action(CONTEXT_YESNOSCREEN, HZ / 2); /* for the countdown */
        switch (action)
        {
            case ACTION_YESNO_ACCEPT:
                result = yn.sel == 0 ? YESNO_YES : YESNO_NO;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                yn.sel = 1;
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                yn.sel = 0;
                break;
            case ACTION_STD_CANCEL:
                result = YESNO_NO;
                break;
            case ACTION_NONE:
                if (tmo_default_res != YESNO_TMO && TIME_AFTER(current_tick, yn.end_tick))
                {
                    splash(HZ/2, ID2P(LANG_TIMEOUT));
                    result = tmo_default_res;
                    goto exit;
                }
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    result = YESNO_USB;
                    goto exit;
                }
                break;
        }

        if (!backlight_on)
            result = YESNO_NONE; /* don't allow results if the screen is off */
    }

exit:
    if (result == YESNO_YES && yes_message)
        show_result(yes_message);
    else if (result == YESNO_NO && no_message)
        show_result(no_message);

    return result;
}

enum yesno_res gui_syncyesno_run(const struct text_message * main_message,
                                 const struct text_message * yes_message,
                                 const struct text_message * no_message)
{
    return gui_syncyesno_run_w_tmo(TIMEOUT_BLOCK, YESNO_TMO, NULL,
                                   main_message, yes_message, no_message);
}

extern enum yesno_res gui_syncyesno_run_w_title(
                                 const char * title,
                                 const struct text_message * main_message,
                                 const struct text_message * yes_message,
                                 const struct text_message * no_message)
{
    return gui_syncyesno_run_w_tmo(TIMEOUT_BLOCK, YESNO_TMO, title,
                                   main_message, yes_message, no_message);
}

static bool yesno_pop_lines(const char *lines[], int line_cnt)
{
    const struct text_message message={lines, line_cnt};
    return gui_syncyesno_run(&message,NULL,NULL) == YESNO_YES;
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
