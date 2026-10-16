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

/* The new code-drawn UI (see docs/newui/PLAN.md). */

#ifndef NEWUI_UI_H
#define NEWUI_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "lcd.h"

/* --- design tokens (PLAN.md §2) --- */

#define UI_STATUS_H     22  /* status bar height */
#define UI_MARGIN       12  /* side margin */
#define UI_GAP           8
#define UI_ROW_H        27  /* text row */
#define UI_ART_ROW_H    54  /* row with a cover */

#define UI_COL_BG       LCD_RGBPACK(0x00, 0x00, 0x00)
#define UI_COL_TEXT     LCD_RGBPACK(0xff, 0xff, 0xff)
#define UI_COL_TEXT2    LCD_RGBPACK(0xaa, 0xaa, 0xaa) /* secondary */
#define UI_COL_MUTED    LCD_RGBPACK(0x8c, 0x8c, 0x8c)
#define UI_COL_SEL      LCD_RGBPACK(0xee, 0xee, 0xec) /* Minimal selection */
#define UI_COL_SEL_TEXT LCD_RGBPACK(0x17, 0x17, 0x17)
#define UI_COL_TRACK    LCD_RGBPACK(0x1c, 0x1c, 0x1c) /* progress track */
#define UI_COL_ACCENT   ui_accent()  /* the user's accent colour */
#define UI_COL_PANEL    LCD_RGBPACK(0x26, 0x26, 0x26) /* pop-up panels */

/* --- the chosen look (ui_style.c) --- */

unsigned ui_accent(void);
/* Minimal style: the selection is a light card, else an accent bar */
bool ui_minimal(void);
/* Draws the selection behind a row at x, y, w, h */
void ui_selection(int x, int y, int w, int h);
/* the selection's colour, and the text on it (main, secondary) */
unsigned ui_sel_bg(void);
unsigned ui_sel_text(void);
unsigned ui_sel_text2(void);

/* --- transitions (ui_transition.c) --- */

/* Before a list changes level: keeps the screen to slide from, dir +1
 * (enter, from the right) or -1 (back, from the left) */
void ui_transition(int dir);
/* Called with the area a list just drew: slides to it if a transition is
 * pending and the area is the whole screen below the status bar. True if
 * it did (and updated the display). */
bool ui_transition_run(int x, int y, int w, int h);

/* --- fonts (ui_font.c) --- */

enum ui_font {
    UI_FONT_SMALL,  /* 14: times, counts, status bar */
    UI_FONT_BODY,   /* 18: secondary text (artist, album, setting values) */
    UI_FONT_ROW,    /* 20 bold: list rows (also the default UI font) */
    UI_FONT_TITLE,  /* 22 bold: titles */
    UI_FONT_BIG,    /* 40 bold, ASCII: fast-scroll letter, volume */
    UI_FONT_COUNT
};

/* Loads the fonts once, at the end of boot; safe to call again. Missing
 * fonts fall back to the system font, as does ui_font() before this. */
void ui_font_init(void);
/* Rockbox font id for one of the UI fonts */
int ui_font(enum ui_font f);
/* FONT_UI as the theme's actual UI font id; other ids unchanged */
int ui_resolve_font(int font);

/* --- drawing (ui_draw.c); coordinates relative to the current viewport --- */

void ui_blend_pixel(int x, int y, unsigned color, int alpha); /* alpha 0..256 */
void ui_fill_rect(int x, int y, int w, int h, unsigned color);
void ui_fill_round_rect(int x, int y, int w, int h, int r, unsigned color);
void ui_stroke_round_rect(int x, int y, int w, int h, int r, int stroke,
                          unsigned color);
void ui_fill_triangle_right(int x, int y, int w, int h, unsigned color);
/* Rounds the corners of what is drawn at x,y,w,h, painting outside them in bg */
void ui_round_corners(int x, int y, int w, int h, int r, unsigned bg);
int ui_text_width(int font, const char *text);
/* Transparent text, shortened with an ellipsis to max_w (0: no limit);
 * align -1 left, 0 centre, 1 right within max_w. Returns the drawn width. */
int ui_text(int x, int y, int font, unsigned color, const char *text,
            int max_w, int align);
/* Same, vertically centred in the band [y, y + h) */
int ui_text_vcenter(int x, int y, int h, int font, unsigned color,
                    const char *text, int max_w, int align);

/* --- pop-ups (ui_popup.c) --- */

#define UI_POPUP_RADIUS   12
#define UI_POPUP_PAD      16  /* panel's inner margin */
#define UI_POPUP_LINES     8
#define UI_POPUP_LINE_LEN 96
struct ui_lines {
    int count;
    int widest;     /* pixels */
    char line[UI_POPUP_LINES][UI_POPUP_LINE_LEN];
};
/* The pop-up panel's rounded rectangle */
void ui_panel(int x, int y, int w, int h);
/* Darkens what is in the current viewport, behind a pop-up */
void ui_dim(void);
/* Appends text to l, word wrapped to max_w; '\n' and '\t' break lines */
void ui_wrap(struct ui_lines *l, const char *text, int font, int max_w);

/* --- album art (ui_art.c) --- */

#define UI_ART_MAX 160 /* largest cover size decoded */

struct bitmap;
/* The playing track's cover scaled to fit size x size (aspect kept), or
 * NULL when nothing plays or it has no art. Cached until the track or the
 * size changes. */
const struct bitmap *ui_art_current(int size);
/* Draws a cover centred in the size x size square, corners rounded */
void ui_art_draw(const struct bitmap *bm, int x, int y, int size, int radius);
/* The no-art placeholder: a dark rounded square with a note */
void ui_art_placeholder(int x, int y, int size, int radius);
struct mp3entry;
/* Decodes id3's cover into out (size x size, centred on black); invalidates
 * the cached current art. False if there is none. */
bool ui_art_decode_to(const struct mp3entry *id3, int size, fb_data *out);

/* --- thumbnails (ui_thumb.c) --- */

#define UI_THUMB 46 /* cover size in list rows */
/* The cover of the album of track_path at UI_THUMB size, or NULL. Cached in
 * RAM and in /.rockbox/newui/thumbs, so each album is decoded once. */
const fb_data *ui_thumb_get(const char *track_path);

#define UI_COVER 120 /* Cover Flow's cover size */
/* The UI_COVER cover from RAM only: NULL with *known false if it is not
 * loaded, else the cover or NULL for none */
const fb_data *ui_cover_peek(const char *track_path, bool *known);
/* Loads the cover into RAM if it was saved on the card (fast); false if
 * it still has to be built */
bool ui_cover_load_saved(const char *track_path);
/* True if the cover is saved on the card */
bool ui_cover_saved(const char *track_path);
/* Decodes the cover (slow) and saves it, and its list thumbnail, on the
 * card, without touching the covers in RAM */
void ui_cover_build(const char *track_path);

/* --- screens --- */

/* Home (scr_home.c). Returns the root menu screen to open (GO_TO_*),
 * or MENU_ATTACHED_USB. *selected keeps the row between visits. */
int ui_home(int *selected);

/* Cover Flow (scr_coverflow.c). Returns GO_TO_WPS when an album was
 * started, MENU_ATTACHED_USB, or GO_TO_ROOT for Back. */
int ui_coverflow(void);

/* The lock screen (scr_lock.c), shown by action.c while the keys are
 * locked. Returns 0 once unlocked, or a system event (SYS_*) that ended it
 * with the keys still locked. */
int ui_lock_screen(void);

/* Now Playing's drawing (scr_np.c), called by apps/gui/wps.c: full redraw,
 * or just the progress bar and times */
void ui_np_draw(bool full);
/* Shows the volume overlay for a moment */
void ui_np_volume_changed(void);
/* Next page: cover and title, details, big cover */
void ui_np_next_page(void);
/* The progress bar as the scrub bar (white, thicker) or back */
void ui_np_set_scrub(bool on);

/* --- status bar (ui_status.c) --- */

struct tm;
struct ui_status {
    int playmode;          /* enum playmode (status.h) */
    bool keylock;
    const struct tm *time; /* NULL: no clock */
    bool clock_24h;
    int battery;           /* percent, -1 unknown */
    bool battery_pct;      /* the percentage left of the battery */
    bool battery_visible;  /* false while blinking off (low battery) */
    bool charging;
    int bluetooth;         /* 0 off, 1 on but not connected, 2 connected */
};
/* Draws the bar into the current viewport (UI_STATUS_H high) */
void ui_status_draw(const struct ui_status *st);

/* --- wheel (ui_wheel.c) --- */

/* Rows to move for one step in direction dir (+1/-1) of a list of nb_items,
 * growing while the wheel spins fast */
int ui_wheel_step(int dir, int nb_items);
/* The fast-scroll letter should be on screen */
bool ui_wheel_letter_visible(void);
/* True once after the letter has been drawn and its time ran out */
bool ui_wheel_letter_expired(void);
/* Draws the letter for 'title' centred in the current viewport */
void ui_wheel_draw_letter(const char *title);

/* --- time (ui_time.c) --- */

/* Free-running microsecond counter for measuring frame times */
uint32_t ui_time_us(void);

/* --- gallery (ui_gallery.c) --- */

/* Debug menu: shows the UI's building blocks and measures drawing speed.
 * Returns true if USB was connected. */
bool ui_gallery(void);

#endif /* NEWUI_UI_H */
