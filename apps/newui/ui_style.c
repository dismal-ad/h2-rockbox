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

/* The look the user chose (Settings > General > Display): the selection
 * as a bar in the accent colour (Classic) or a light card (Minimal), and
 * the accent colour. */

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

unsigned ui_sel_text(void)
{
    return ui_minimal() ? UI_COL_SEL_TEXT : UI_COL_TEXT;
}

unsigned ui_sel_text2(void)
{
    return ui_minimal() ? LCD_RGBPACK(0x48, 0x48, 0x48) : UI_COL_TEXT;
}

void ui_selection(int x, int y, int w, int h)
{
    if (ui_minimal())
        ui_fill_round_rect(x + CARD_INSET, y, w - 2 * CARD_INSET, h,
                           CARD_RADIUS, UI_COL_SEL);
    else
        ui_fill_rect(x, y, w, h, ui_accent());
}
