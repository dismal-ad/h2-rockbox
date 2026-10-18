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

/* Wheel speed-up and the fast-scroll letter (PLAN.md §2 Motion).
 *
 * Steps closer than SPIN_GAP_US apart, in the same direction, are one spin.
 * After SPIN_RAMP1_US of spinning each step moves 2 rows, then 4 and 8.
 * Only lists of more than MIN_ITEMS rows speed up. While the step is above
 * 1, the selected row's first letter is shown large; it hides LETTER_HOLD_US
 * after the last fast step. */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include "strcasecmp.h"
#include "lcd.h"
#include "font.h"
#include "ui.h"

#define SPIN_GAP_US     80000
#define SPIN_RAMP1_US  300000
#define SPIN_RAMP2_US  800000
#define SPIN_RAMP3_US 1500000
#define LETTER_HOLD_US 400000
#define MIN_ITEMS          16

static uint32_t last_step_us, spin_start_us, last_fast_us;
static int last_dir;
static bool letter_shown;

int ui_wheel_step(int dir, int nb_items)
{
    uint32_t now = ui_time_us();
    bool spinning = dir == last_dir && (now - last_step_us) < SPIN_GAP_US;

    if (!spinning)
        spin_start_us = now;
    last_step_us = now;
    last_dir = dir;

    if (nb_items <= MIN_ITEMS)
        return 1;

    uint32_t spun = now - spin_start_us;
    int step = spun >= SPIN_RAMP3_US ? 8 :
               spun >= SPIN_RAMP2_US ? 4 :
               spun >= SPIN_RAMP1_US ? 2 : 1;
    if (step > 1)
        last_fast_us = now;
    return step;
}

bool ui_wheel_letter_visible(void)
{
    return last_fast_us && (ui_time_us() - last_fast_us) < LETTER_HOLD_US;
}

bool ui_wheel_letter_expired(void)
{
    if (letter_shown && !ui_wheel_letter_visible())
    {
        letter_shown = false;
        return true;
    }
    return false;
}

/* The letter to show for a title: first character, skipping spaces and a
 * leading "The ", "A " or "An "; ASCII letters in capitals */
static void title_letter(const char *s, char *out)
{
    static const char * const articles[] = { "the ", "a ", "an " };
    while (*s == ' ')
        s++;
    for (size_t i = 0; i < sizeof(articles) / sizeof(articles[0]); i++)
    {
        size_t n = strlen(articles[i]);
        if (strncasecmp(s, articles[i], n) == 0 && s[n])
        {
            s += n;
            break;
        }
    }
    size_t len = 1;
    if ((unsigned char)*s >= 0x80)
        while (((unsigned char)s[len] & 0xc0) == 0x80 && len < 4)
            len++;
    memcpy(out, s, len);
    out[len] = '\0';
    if (len == 1)
        out[0] = toupper((unsigned char)out[0]);
}

void ui_wheel_draw_letter(const char *title)
{
    struct viewport *vp = lcd_current_viewport;
    char letter[5];
    title_letter(title, letter);
    if (!letter[0])
        return;

    /* the big font is ASCII only */
    int font = (unsigned char)letter[0] < 0x80 ? ui_font(UI_FONT_BIG)
                                               : ui_font(UI_FONT_TITLE);
    const int size = 64;
    int x = (vp->width - size) / 2, y = (vp->height - size) / 2;
    ui_fill_round_rect(x, y, size, size, 10, UI_COL_PANEL);
    ui_text_vcenter(x, y, size, font, UI_COL_TEXT, letter, size, 0);
    letter_shown = true;
}
