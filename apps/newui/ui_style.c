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

/* The look the user chose (Settings > General > Display): the theme (Dark,
 * or Light like the iPod), the selection as a bar in the accent colour
 * (Classic) or a card (Minimal), and the accent colour. */

#include "config.h"
#include "settings.h"
#include "ui.h"

static const unsigned accents[] = {
    LCD_RGBPACK(0x6e, 0x6e, 0x6e),  /* Graphite */
    LCD_RGBPACK(0x2f, 0x6f, 0xdb),  /* Blue */
    LCD_RGBPACK(0xc8, 0x32, 0x3c),  /* Red */
    LCD_RGBPACK(0x2e, 0x9e, 0x5b),  /* Green */
    LCD_RGBPACK(0x7b, 0x4f, 0xc9),  /* Purple */
    LCD_RGBPACK(0xe0, 0x7a, 0x1f),  /* Orange */
};

#define RGB LCD_RGBPACK
static const unsigned themes[2][UI_C_COUNT] = {
    [0] = {     /* Dark */
        [UI_C_BG] = RGB(0x00, 0x00, 0x00),       [UI_C_TEXT] = RGB(0xff, 0xff, 0xff),
        [UI_C_TEXT2] = RGB(0xaa, 0xaa, 0xaa),    [UI_C_MUTED] = RGB(0x8c, 0x8c, 0x8c),
        [UI_C_SEL] = RGB(0xee, 0xee, 0xec),      [UI_C_SEL_TEXT] = RGB(0x17, 0x17, 0x17),
        [UI_C_TRACK] = RGB(0x1c, 0x1c, 0x1c),    [UI_C_PANEL] = RGB(0x26, 0x26, 0x26),
        [UI_C_ARTIST] = RGB(0xcc, 0xcc, 0xcc),   [UI_C_ALBUM] = RGB(0xbb, 0xbb, 0xbb),
        [UI_C_FILL] = RGB(0xee, 0xee, 0xec),
        [UI_C_BAR_TOP] = RGB(0x00, 0x00, 0x00),  [UI_C_BAR_BOTTOM] = RGB(0x00, 0x00, 0x00),
    },
    [1] = {     /* Light: white, dark text, a silver status bar */
        [UI_C_BG] = RGB(0xff, 0xff, 0xff),       [UI_C_TEXT] = RGB(0x14, 0x14, 0x14),
        [UI_C_TEXT2] = RGB(0x5e, 0x5e, 0x5e),    [UI_C_MUTED] = RGB(0x8c, 0x8c, 0x8c),
        [UI_C_SEL] = RGB(0xd6, 0xd6, 0xd6),      [UI_C_SEL_TEXT] = RGB(0x14, 0x14, 0x14),
        [UI_C_TRACK] = RGB(0xdc, 0xdc, 0xdc),    [UI_C_PANEL] = RGB(0xf2, 0xf2, 0xf2),
        [UI_C_ARTIST] = RGB(0x33, 0x33, 0x33),   [UI_C_ALBUM] = RGB(0x4a, 0x4a, 0x4a),
        [UI_C_FILL] = 0,                         /* the accent */
        [UI_C_BAR_TOP] = RGB(0xf4, 0xf4, 0xf4),  [UI_C_BAR_BOTTOM] = RGB(0xbe, 0xbe, 0xc2),
    },
};

bool ui_light(void)
{
    return global_settings.newui_theme == 1;
}

unsigned ui_col(enum ui_col c)
{
    if (c == UI_C_FILL && ui_light())
        return ui_accent();
    return themes[ui_light()][c];
}

#define CARD_INSET  6
#define CARD_RADIUS 6

unsigned ui_accent(void)
{
    unsigned i = global_settings.newui_accent;
    return accents[i < ARRAYLEN(accents) ? i : 0];
}

bool ui_minimal(void)
{
    return global_settings.newui_style == 1;
}

unsigned ui_sel_bg(void)
{
    return ui_minimal() ? UI_COL_SEL : ui_accent();
}

/* the accent bar is dark in both themes: white text on it */
unsigned ui_sel_text(void)
{
    return ui_minimal() ? UI_COL_SEL_TEXT : LCD_RGBPACK(0xff, 0xff, 0xff);
}

unsigned ui_sel_text2(void)
{
    return ui_minimal() ? LCD_RGBPACK(0x48, 0x48, 0x48) : LCD_RGBPACK(0xff, 0xff, 0xff);
}

/* Light theme: the iPod's glossy bar, from a lighter accent at the top to
 * the accent at the bottom */
unsigned ui_sel_top(void)
{
    unsigned c = ui_accent();
    if (!ui_light())
        return c;
    int r = RGB_UNPACK_RED(c), g = RGB_UNPACK_GREEN(c), b = RGB_UNPACK_BLUE(c);
    return LCD_RGBPACK(r + (255 - r) * 2 / 5, g + (255 - g) * 2 / 5,
                       b + (255 - b) * 2 / 5);
}

void ui_selection(int x, int y, int w, int h)
{
    if (ui_minimal())
        ui_fill_round_rect(x + CARD_INSET, y, w - 2 * CARD_INSET, h,
                           CARD_RADIUS, UI_COL_SEL);
    else if (ui_light())
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_gradient_fillrect(x, y, w, h, ui_sel_top(), ui_accent());
    }
    else
        ui_fill_rect(x, y, w, h, ui_accent());
}
