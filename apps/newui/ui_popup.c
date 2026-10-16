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

/* Pop-up building blocks (PLAN.md §4.8): the rounded panel splash and
 * yes/no draw on, word wrapping, dimming what is behind. */

#include "config.h"
#include <string.h>
#include "system.h"
#include "lcd.h"
#include "font.h"
#include <stdio.h>
#include "ui.h"

void ui_panel(int x, int y, int w, int h)
{
    ui_fill_round_rect(x, y, w, h, UI_POPUP_RADIUS, UI_COL_PANEL);
}

void ui_dim(void)
{
    struct viewport *vp = lcd_current_viewport;
    for (int y = 0; y < vp->height; y++)
    {
        fb_data *p = FBADDR(vp->x, vp->y + y);
        for (int x = 0; x < vp->width; x++, p++)
        {
            /* RGB565 at 3/8 brightness */
            unsigned c = FB_UNPACK_SCALAR_LCD(*p);
            unsigned r = RGB_UNPACK_RED(c) * 3 / 8;
            unsigned g = RGB_UNPACK_GREEN(c) * 3 / 8;
            unsigned b = RGB_UNPACK_BLUE(c) * 3 / 8;
            *p = FB_SCALARPACK(LCD_RGBPACK(r, g, b));
        }
    }
}

/* adds text[0..len) as a line; false when full */
static bool add_line(struct ui_lines *l, const char *text, int len, int font)
{
    if (l->count >= UI_POPUP_LINES)
        return false;
    if (len >= UI_POPUP_LINE_LEN)
        len = UI_POPUP_LINE_LEN - 1;
    char *line = l->line[l->count++];
    memcpy(line, text, len);
    line[len] = '\0';
    int w = ui_text_width(font, line);
    if (w > l->widest)
        l->widest = w;
    return true;
}

void ui_wrap(struct ui_lines *l, const char *text, int font, int max_w)
{
    char buf[UI_POPUP_LINE_LEN];
    while (*text)
    {
        /* one paragraph: up to a line break */
        int plen = strcspn(text, "\n\t");
        const char *p = text, *end = text + plen;
        while (p < end)
        {
            while (p < end && *p == ' ')
                p++;
            if (p >= end)
                break;
            /* take words while they fit */
            const char *fit = NULL, *q = p;
            while (q < end)
            {
                const char *w = q;
                while (w < end && *w != ' ')
                    w++;
                int len = MIN(w - p, (int)sizeof(buf) - 1);
                memcpy(buf, p, len);
                buf[len] = '\0';
                if (fit && ui_text_width(font, buf) > max_w)
                    break;
                fit = w;
                q = w;
                while (q < end && *q == ' ')
                    q++;
            }
            /* a single word too long is shortened when drawn */
            if (!add_line(l, p, fit - p, font))
                return;
            p = fit;
        }
        if (plen == 0 && !add_line(l, "", 0, font))
            return;
        text = *end ? end + 1 : end;
    }
}

/* A choice panel: the title and the choices as rows, the selected one
 * highlighted, centred in vp over what is behind it. */
void ui_choice_draw(struct viewport *vp, const char *title, int count, int sel,
                    const char *(*label)(int i, void *data, char *buf, size_t len),
                    void *data)
{
    struct viewport *last = lcd_set_viewport(vp);
    const int font = ui_font(UI_FONT_ROW), tfont = ui_font(UI_FONT_TITLE);
    const int th = font_get(tfont)->height + 6;
    char buf[64];

    int widest = title ? ui_text_width(tfont, title) : 0;
    for (int i = 0; i < count; i++)
    {
        int w = ui_text_width(font, label(i, data, buf, sizeof(buf)));
        if (w > widest)
            widest = w;
    }
    int w = MIN(MAX(widest + 2 * UI_POPUP_PAD, 180), vp->width - 2 * UI_MARGIN);
    int h = 12 + (title ? th : 0) + count * UI_ROW_H + 8;
    int x = (vp->width - w) / 2, y = (vp->height - h) / 2;
    int tw = w - 2 * UI_POPUP_PAD;

    ui_panel(x, y, w, h);
    int ry = y + 12;
    if (title)
    {
        ui_text(x + UI_POPUP_PAD, ry, tfont, UI_COL_TEXT, title, tw, 0);
        ry += th;
    }
    for (int i = 0; i < count; i++, ry += UI_ROW_H)
    {
        if (i == sel)
            ui_fill_round_rect(x + 8, ry, w - 16, UI_ROW_H, 6, ui_sel_bg());
        ui_text_vcenter(x + UI_POPUP_PAD, ry, UI_ROW_H, font,
                        i == sel ? ui_sel_text() : UI_COL_TEXT2,
                        label(i, data, buf, sizeof(buf)), tw, -1);
    }
    lcd_update_viewport_rect(x, y, w, h);
    lcd_set_viewport(last);
}
