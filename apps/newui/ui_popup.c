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
