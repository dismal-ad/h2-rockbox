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

/* Drawing primitives for the new UI. Coordinates are relative to the
 * current viewport and clipped to it. Colours are native (LCD_RGBPACK). */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "system.h"
#include "lcd.h"
#include "font.h"
#include "ui.h"

static inline unsigned blend565(unsigned dst, unsigned src, int alpha)
{
    /* alpha 0..256 */
    int dr = RGB_UNPACK_RED(dst), dg = RGB_UNPACK_GREEN(dst), db = RGB_UNPACK_BLUE(dst);
    int sr = RGB_UNPACK_RED(src), sg = RGB_UNPACK_GREEN(src), sb = RGB_UNPACK_BLUE(src);
    return LCD_RGBPACK(dr + (((sr - dr) * alpha) >> 8),
                       dg + (((sg - dg) * alpha) >> 8),
                       db + (((sb - db) * alpha) >> 8));
}

void ui_blend_pixel(int x, int y, unsigned color, int alpha)
{
    struct viewport *vp = lcd_current_viewport;
    if (x < 0 || y < 0 || x >= vp->width || y >= vp->height || alpha <= 0)
        return;
    fb_data *p = FBADDR(vp->x + x, vp->y + y);
    if (alpha >= 256)
        *p = FB_SCALARPACK(color);
    else
        *p = FB_SCALARPACK(blend565(FB_UNPACK_SCALAR_LCD(*p), color, alpha));
}

void ui_fill_rect(int x, int y, int w, int h, unsigned color)
{
    if (w <= 0 || h <= 0)
        return;
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(color);
    lcd_fillrect(x, y, w, h);
}

/* Coverage (0..256) of pixel (cx, cy) of a corner square by a circle of
 * radius rad centred at (r, r), the square's inner corner. Sampled on a 4x4
 * grid; positions are in eighths of a pixel. */
static int corner_coverage(int cx, int cy, int r, int rad)
{
    if (rad <= 0)
        return 0;
    int c = r * 8, lim = rad * 8 * rad * 8, hits = 0;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++)
        {
            int dx = cx * 8 + sx * 2 + 1 - c;
            int dy = cy * 8 + sy * 2 + 1 - c;
            if (dx * dx + dy * dy <= lim)
                hits++;
        }
    return hits * 16;
}

void ui_fill_round_rect(int x, int y, int w, int h, int r, unsigned color)
{
    if (w <= 0 || h <= 0)
        return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0)
    {
        ui_fill_rect(x, y, w, h, color);
        return;
    }

    ui_fill_rect(x + r, y, w - 2 * r, h, color);
    ui_fill_rect(x, y + r, r, h - 2 * r, color);
    ui_fill_rect(x + w - r, y + r, r, h - 2 * r, color);

    for (int cy = 0; cy < r; cy++)
        for (int cx = 0; cx < r; cx++)
        {
            int a = corner_coverage(cx, cy, r, r);
            if (!a)
                continue;
            ui_blend_pixel(x + cx,         y + cy,         color, a);
            ui_blend_pixel(x + w - 1 - cx, y + cy,         color, a);
            ui_blend_pixel(x + cx,         y + h - 1 - cy, color, a);
            ui_blend_pixel(x + w - 1 - cx, y + h - 1 - cy, color, a);
        }
}

/* Outline of a rounded rectangle, 'stroke' pixels wide */
void ui_stroke_round_rect(int x, int y, int w, int h, int r, int stroke,
                          unsigned color)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    /* straight edges */
    ui_fill_rect(x + r, y, w - 2 * r, stroke, color);
    ui_fill_rect(x + r, y + h - stroke, w - 2 * r, stroke, color);
    ui_fill_rect(x, y + r, stroke, h - 2 * r, color);
    ui_fill_rect(x + w - stroke, y + r, stroke, h - 2 * r, color);
    /* corners: outer circle minus inner circle, same centre */
    for (int cy = 0; cy < r; cy++)
        for (int cx = 0; cx < r; cx++)
        {
            int a = corner_coverage(cx, cy, r, r) -
                    corner_coverage(cx, cy, r, r - stroke);
            if (a <= 0)
                continue;
            ui_blend_pixel(x + cx,         y + cy,         color, a);
            ui_blend_pixel(x + w - 1 - cx, y + cy,         color, a);
            ui_blend_pixel(x + cx,         y + h - 1 - cy, color, a);
            ui_blend_pixel(x + w - 1 - cx, y + h - 1 - cy, color, a);
        }
}

/* Rounds the corners of an already drawn rectangle by painting the outside
 * of each corner in the background colour (anti-aliased) */
void ui_round_corners(int x, int y, int w, int h, int r, unsigned bg)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (int cy = 0; cy < r; cy++)
        for (int cx = 0; cx < r; cx++)
        {
            int a = 256 - corner_coverage(cx, cy, r, r);
            if (a <= 0)
                continue;
            ui_blend_pixel(x + cx,         y + cy,         bg, a);
            ui_blend_pixel(x + w - 1 - cx, y + cy,         bg, a);
            ui_blend_pixel(x + cx,         y + h - 1 - cy, bg, a);
            ui_blend_pixel(x + w - 1 - cx, y + h - 1 - cy, bg, a);
        }
}

/* Filled triangle pointing right (play glyph) inside w x h */
void ui_fill_triangle_right(int x, int y, int w, int h, unsigned color)
{
    for (int row = 0; row < h; row++)
    {
        /* distance of the row's centre from the nearer end, doubled:
         * 1, 3, 5, ... up to ~h at the middle, then back down */
        int twice = 2 * row + 1;
        int dist = twice <= h ? twice : 2 * h - twice;
        int len = dist * w;               /* row length times h */
        int full = len / h;
        int frac = (len % h) * 256 / h;
        ui_fill_rect(x, y + row, full, 1, color);
        ui_blend_pixel(x + full, y + row, color, frac);
    }
}

int ui_text_width(int font, const char *text)
{
    int w = 0, h;
    font = ui_resolve_font(font);
    font_getstringsize(text, &w, &h, font);
    return w;
}

/* Draws text with a transparent background. If it is wider than max_w
 * (0 = no limit) it is shortened with an ellipsis. align: -1 left,
 * 0 centre, 1 right (within max_w, starting at x). Returns the width drawn. */
int ui_text(int x, int y, int font, unsigned color, const char *text,
            int max_w, int align)
{
    static const char ellipsis[] = "\xe2\x80\xa6";
    char buf[256];
    font = ui_resolve_font(font);
    const char *out = text;
    int w = ui_text_width(font, text);

    if (max_w > 0 && w > max_w)
    {
        int ew = ui_text_width(font, ellipsis);
        size_t len = strlen(text);
        if (len > sizeof(buf) - sizeof(ellipsis))
            len = sizeof(buf) - sizeof(ellipsis);
        memcpy(buf, text, len);
        buf[len] = '\0';
        while (len > 0)
        {
            /* drop one UTF-8 character */
            do
                len--;
            while (len > 0 && (buf[len] & 0xc0) == 0x80);
            buf[len] = '\0';
            if (ui_text_width(font, buf) + ew <= max_w)
                break;
        }
        while (len > 0 && buf[len - 1] == ' ')
            buf[--len] = '\0';
        strcat(buf, ellipsis);
        out = buf;
        w = ui_text_width(font, out);
    }

    if (max_w > 0 && align == 0)
        x += (max_w - w) / 2;
    else if (max_w > 0 && align > 0)
        x += max_w - w;

    /* lcd_setfont() sets the current viewport's font: put it back */
    int vp_font = lcd_current_viewport->font;
    lcd_setfont(font);
    lcd_set_drawmode(DRMODE_FG);
    lcd_set_foreground(color);
    lcd_putsxy(x, y, out);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_setfont(vp_font);
    return w;
}

/* Text vertically centred in a band of height h starting at y */
/* The top row of the font's capitals ('H'), or -1 if unknown (1-bit
 * fonts). Measured once per font: the line box of some fonts has more room
 * below the letters than above, so centring it puts the text too high. */
static int cap_top(int font)
{
    static signed char cache[MAXFONTS];
    static bool known[MAXFONTS];
    if (font < 0 || font >= MAXFONTS)
        return -1;
    if (known[font])
        return cache[font];

    struct font *pf = font_get(font);
    int top = -1;
    if (pf->depth)
    {
        /* 4-bit alpha, pixels in a stream row by row, low nibble first;
           0 is full ink, 15 none */
        const unsigned char *bits = font_get_bits(pf, 'H');
        int w = font_get_width(pf, 'H');
        for (int r = 0; r < (int)pf->height && top < 0; r++)
            for (int c = 0; c < w; c++)
            {
                int p = r * w + c;
                int a = (bits[p >> 1] >> ((p & 1) * 4)) & 15;
                if (a < 8)
                {
                    top = r;
                    break;
                }
            }
    }
    known[font] = true;
    cache[font] = top;
    return top;
}

int ui_text_y(int font, int y, int h)
{
    font = ui_resolve_font(font);
    struct font *pf = font_get(font);
    int top = cap_top(font);
    if (top < 0 || top >= pf->ascent)
        return y + (h - (int)pf->height) / 2;
    /* centre the capitals, from their top to the baseline */
    int cap_h = pf->ascent - top;
    return y + (h - cap_h + 1) / 2 - top;
}

int ui_text_vcenter(int x, int y, int h, int font, unsigned color,
                    const char *text, int max_w, int align)
{
    return ui_text(x, ui_text_y(font, y, h), font, color, text, max_w, align);
}

/* The iPod's ">": two bold strokes meeting at a point, drawn with
 * anti-aliased edges, its right end at x_right, centred in [y, y + h).
 * Each row is a span T wide (in 1/256 px) along the stroke. */
void ui_chevron(int x_right, int y, int h, unsigned color)
{
    const int W = UI_CHEVRON_W, H = UI_CHEVRON_H, T = 3 * 256 + 128;
    const int x0 = x_right - W, y0 = y + (h - H) / 2;
    for (int r = 0; r < H; r++)
    {
        /* the stroke's centre: out to the right at the middle row */
        int d = abs(2 * r + 1 - H);                     /* 0..H-1 */
        int xc = (W * 256 - T / 2) - (W * 256 - T) * d / H;
        int l = xc - T / 2, rr = xc + T / 2;            /* in 1/256 px */
        for (int px = l >> 8; px <= (rr - 1) >> 8; px++)
        {
            int p0 = MAX(px * 256, l), p1 = MIN(px * 256 + 256, rr);
            ui_blend_pixel(x0 + px, y0 + r, color, p1 - p0);
        }
    }
}
