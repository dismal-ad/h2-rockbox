/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Björn Stenberg
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
#include "kernel.h"
#include "system.h"
#include "string-extra.h"
#include "font.h"
#include "screens.h"
#include "talk.h"
#include "settings.h"
#include "misc.h"
#include "rbunicode.h"
#include "logf.h"
#include "hangul.h"
#include "action.h"
#include "icon.h"
#include "pcmbuf.h"
#include "lang.h"
#include "bidi.h"
#include "keyboard.h"
#include "viewport.h"
#include "file.h"
#include "splash.h"
#include "core_alloc.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif


#define DEFAULT_MARGIN 6
#define KBD_BUF_SIZE 500

/* Sentinel picker cell, drawn as an icon (the UI font has no glyph for it):
 * KBD_SPACE inserts a space. */
#define KBD_SPACE 0x2423

#define CHANGED_PICKER 1
#define CHANGED_CURSOR 2
#define CHANGED_TEXT   3

enum ekbd_viewports
{
    eKBD_VP_TEXT = 0,
    eKBD_VP_PICKER,
    eKBD_VP_MENU,
    eKBD_COUNT_VP_COUNT
};

struct keyboard_parameters
{
    struct viewport *kbd_viewports;
    ucschar_t kbd_buf[KBD_BUF_SIZE];
    ucschar_t *kbd_buf_ptr;
    unsigned short max_line_len;
    int default_lines;
    unsigned int last_k;
    unsigned int last_i;
    unsigned short font_w;
    unsigned short font_h;
    unsigned int text_w;
    int curfont;
    int main_y;
    int max_chars;
    int max_chars_text;
    int lines;
    int pages;
    int keyboard_margin;
    int curpos;
    int leftpos;
    int page;
    int x;
    int y;
    bool line_edit;
};

struct edit_state
{
    char* text;
    int buflen;
    int len_utf8;
    int editpos;        /* Edit position on all screens */
    bool cur_blink;     /* Cursor on/off flag */
    bool hangul;
    ucschar_t hlead, hvowel, htail;
    int changed;
};

static struct keyboard_parameters kbd_param[NB_SCREENS];
static bool kbd_loaded = false;

static void keyboard_layout(struct viewport *kbd_vp,
                        struct keyboard_parameters *pm,
                                     struct screen *sc)
{
     /*Note: viewports are initialized to vp_default by kbd_create_viewports */

    unsigned short sc_w = sc->getwidth();
    unsigned short sc_h = sc->getheight();

    /* TEXT */
    struct viewport *vp = &kbd_vp[eKBD_VP_TEXT];
    /* make sure height is even for the text box */
    unsigned short text_height = (MAX(pm->font_h, (unsigned int)get_icon_height(sc->screen_type)) & ~1) + 2;
    vp->x = 0; /* LEFT */
    vp->y = 0; /* TOP */
    vp->width = sc_w;
    vp->height = text_height;
    vp->font = pm->curfont;
    text_height += vp->x + 3;

    /* MENU */
    vp = &kbd_vp[eKBD_VP_MENU];
    int menu_w = 0;//pm->font_w * MENU_CHARS; /* NOT IMPLEMENTED */
    vp->x = 0; /* LEFT */
    vp->y = text_height; /* TOP */
    vp->width = menu_w;
    vp->height = 0;
    vp->font = pm->curfont;
    menu_w += vp->x;

    /* PICKER */
    vp = &kbd_vp[eKBD_VP_PICKER];
    vp->x = menu_w; /* LEFT */
    vp->y = text_height - 2; /* TOP */
    vp->width = sc_w - menu_w;
    vp->height = sc_h - vp->y; /* (MAX SIZE) - OVERWRITTEN */
    vp->font = pm->curfont;
}

static int kbd_create_viewports(struct keyboard_parameters * kbd_param)
{
    static struct viewport viewports[NB_SCREENS][eKBD_COUNT_VP_COUNT];
    int i;
    FOR_NB_SCREENS(l)
    {
        kbd_param[l].kbd_viewports = viewports[l];
        for (i = 0; i < eKBD_COUNT_VP_COUNT; i++)
        {
            struct viewport *vp = &kbd_param[l].kbd_viewports[i];
            viewport_set_defaults(vp, l);
            /* The keyboard lays out its own cells/grid by absolute x, so the
             * automatic RTL right-alignment (which mirrors every putsxy) would
             * reverse the picker grid and the edit line. Clear it here and do
             * RTL handling explicitly in kbd_draw_edit_line instead. */
            vp->flags &= ~VP_FLAG_ALIGNMENT_MASK;
            vp->font = FONT_UI;
        }
    }

    return sizeof(viewports);
}

/* Loads a custom keyboard into memory
   call with NULL to reset keyboard    */
int load_kbd(unsigned char* filename)
{
    int fd;
    int i, line_len, max_line_len;
    unsigned char buf[4];
    ucschar_t *pbuf;

    if (filename == NULL)
    {
        kbd_loaded = false;
        return 0;
    }

    fd = open_utf8(filename, O_RDONLY|O_BINARY);
    if (fd < 0)
        return 1;

    pbuf = kbd_param[0].kbd_buf;
    line_len = 0;
    max_line_len = 1;
    i = 1;
    while (read(fd, buf, 1) == 1 && i < KBD_BUF_SIZE-1)
    {
        /* check how many bytes to read for this character */
        static const unsigned char sizes[4] = { 0x80, 0xe0, 0xf0, 0xf5 };
        size_t count;
        ucschar_t ch;

        for (count = 0; count < ARRAYLEN(sizes); count++)
        {
            if (buf[0] < sizes[count])
                break;
        }

        if (count >= ARRAYLEN(sizes))
            continue; /* Invalid size. */

        if (read(fd, &buf[1], count) != (ssize_t)count)
        {
            close(fd);
            kbd_loaded = false;
            return 1;
        }

        utf8decode(buf, &ch);
        if (ch != 0xFEFF && ch != '\r') /* skip BOM & carriage returns */
        {
            i++;
            if (ch == '\n')
            {
                if (max_line_len < line_len)
                    max_line_len = line_len;
                *pbuf = line_len;
                pbuf += line_len + 1;
                line_len = 0;
            }
            else
                pbuf[++line_len] = ch;
        }
    }

    close(fd);
    kbd_loaded = true;

    if (max_line_len < line_len)
        max_line_len = line_len;
    if (i == 1 || line_len != 0) /* ignore last empty line */
    {
        *pbuf = line_len;
        pbuf += line_len + 1;
    }
    *pbuf = 0xFEFF; /* mark end of characters */
    i++;
    FOR_NB_SCREENS(l)
    {
        struct keyboard_parameters *pm = &kbd_param[l];
#if NB_SCREENS > 1
        if (l > 0)
            memcpy(pm->kbd_buf, kbd_param[0].kbd_buf, i*sizeof(ucschar_t));
#endif
        /* initialize parameters */
        pm->x = pm->y = pm->page = 0;
        pm->default_lines = 0;
        pm->max_line_len = max_line_len;
    }

    return 0;
}

/* helper function to spell a char */
static void kbd_spellchar(ucschar_t c)
{
    unsigned char tmp[5];
    /* store char to pass to talk_spell */
    unsigned char* utf8 = utf8encode(c, tmp);
    *utf8 = 0;

    if (c == ' ')
        talk_id(VOICE_BLANK, false);
    else
        talk_spell(tmp, false);
}

static void kbd_inschar(struct edit_state *state, ucschar_t ch)
{
    int i, j, len;
    unsigned char tmp[4];
    unsigned char* utf8;

    len = strlen(state->text);
    utf8 = utf8encode(ch, tmp);
    j = (intptr_t)utf8 - (intptr_t)tmp;

    if (len + j < state->buflen)
    {
        i = utf8seek(state->text, state->editpos);
        utf8 = state->text + i;
        memmove(utf8 + j, utf8, len - i + 1);
        memcpy(utf8, tmp, j);
        state->editpos++;
        state->changed = CHANGED_TEXT;
    }
}

static void kbd_delchar(struct edit_state *state)
{
    int i, j, len;
    unsigned char* utf8;

    if (state->editpos > 0)
    {
        state->editpos--;
        len = strlen(state->text);
        i = utf8seek(state->text, state->editpos);
        utf8 = state->text + i;
        j = utf8seek(utf8, 1);
        memmove(utf8, utf8 + j, len - i - j + 1);
        state->changed = CHANGED_TEXT;
    }
}

/* Lookup k value based on state of param (pm) */
static ucschar_t get_kbd_ch(struct keyboard_parameters *pm, int x, int y)
{
    unsigned int n, i = 0, k = pm->page*pm->lines + y;
    ucschar_t *pbuf;
    if (k >= pm->last_k)
    {
        i = pm->last_i;
        k -= pm->last_k;
    }
    for (pbuf = &pm->kbd_buf_ptr[i]; (i = *pbuf) != 0xFEFF; pbuf += i + 1)
    {
        n = i ? (i + pm->max_chars - 1) / pm->max_chars : 1;
        if (k < n) break;
        k -= n;
    }
    if (y == 0 && i != 0xFEFF)
    {
        pm->last_k = pm->page*pm->lines - k;
        pm->last_i = pbuf - pm->kbd_buf_ptr;
    }
    k = k * pm->max_chars + x;
    return (*pbuf != 0xFEFF && k < *pbuf)? pbuf[k+1]: ' ';
}

static void kbd_calc_pm_params(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state);
static void kbd_calc_vp_params(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state);
static void kbd_draw_picker(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state);
static void kbd_draw_edit_line(struct keyboard_parameters *pm,
                               struct screen *sc, struct edit_state *state);
static void kbd_insert_selected(struct keyboard_parameters *pm,
                                struct edit_state *state);
static void kbd_backspace(struct edit_state *state);
static void kbd_move_cursor(struct edit_state *state, int dir);
static void kbd_move_picker_horizontal(struct keyboard_parameters *pm,
                                       struct edit_state *state, int dir);
static void kbd_move_picker_vertical(struct keyboard_parameters *pm,
                                     struct edit_state *state, int dir);

int kbd_input(char* text, int buflen, ucschar_t *kbd)
{
    bool done = false;
    struct keyboard_parameters * const param = kbd_param;
    struct edit_state state;
    ucschar_t ch;
    int ret = 0; /* assume success */
    FOR_NB_SCREENS(l)
    {
        viewportmanager_theme_enable(l, false, NULL);
    }

    if (kbd_create_viewports(param) <= 0)
    {
        splash(HZ * 2,"Error: No Viewport Allocated, OOM?");
        goto cleanup;
    }

    /* initialize state */
    state.text = text;
    state.buflen = buflen;
    /* Initial edit position is after last character */
    state.editpos = utf8length(state.text);
    state.cur_blink = true;
    state.hangul = false;
    state.changed = 0;

    if (!kbd_loaded)
    {
        /* Copy default keyboard to buffer */
        FOR_NB_SCREENS(l)
        {
            struct keyboard_parameters *pm = &param[l];
            ucschar_t *pbuf;
            const unsigned char *p;
            int len = 0;

            {
            struct screen *sc = &screens[l];

            if (sc->getwidth() >= 160 && sc->getheight() >= 96)
            {
                p = "ABCDEFG abcdefg !?\" @#$%+'\n"
                    "HIJKLMN hijklmn 789 &_()-`\n"
                    "OPQRSTU opqrstu 456 §|{}/<\n"
                    "VWXYZ., vwxyz.,0123 ~=[]*>\n"
                    "ÀÁÂÃÄÅÆ ÌÍÎÏ ÈÉÊË ¢£¤¥¦§©®\n"
                    "àáâãäåæ ìíîï èéêë «»°ºª¹²³\n"
                    "ÓÒÔÕÖØ ÇÐÞÝß ÙÚÛÜ ¯±×÷¡¿µ·\n"
                    "òóôõöø çðþýÿ ùúûü ¼½¾¬¶¨:;";

                pm->default_lines = 8;
                pm->max_line_len = 26;
            }
            else
            {
                p = "ABCDEFG !?\" @#$%+'\n"
                    "HIJKLMN 789 &_()-`\n"
                    "OPQRSTU 456 §|{}/<\n"
                    "VWXYZ.,0123 ~=[]*>\n"

                    "abcdefg ¢£¤¥¦§©®¬\n"
                    "hijklmn «»°ºª¹²³¶\n"
                    "opqrstu ¯±×÷¡¿µ·¨\n"
                    "vwxyz., :;¼½¾    \n"

                    "ÀÁÂÃÄÅÆ ÌÍÎÏ ÈÉÊË\n"
                    "àáâãäåæ ìíîï èéêë\n"
                    "ÓÒÔÕÖØ ÇÐÞÝß ÙÚÛÜ\n"
                    "òóôõöø çðþýÿ ùúûü";

                pm->default_lines = 4;
                pm->max_line_len = 18;
            }
            }
            pbuf = pm->kbd_buf;
            while (*p)
            {
                p = utf8decode(p, &pbuf[len+1]);
                if (pbuf[len+1] == '\n')
                {
                    *pbuf = len;
                    pbuf += len+1;
                    len = 0;
                }
                else
                    len++;
            }
            *pbuf = len;
            pbuf[len+1] = 0xFEFF;   /* mark end of characters */

            /* initialize parameters */
            pm->x = pm->y = pm->page = 0;
        }
        kbd_loaded = true;
    }

    FOR_NB_SCREENS(l)
    {
        struct keyboard_parameters *pm = &param[l];

        if(kbd) /* user supplied custom layout */
            pm->kbd_buf_ptr = kbd;
        else
            pm->kbd_buf_ptr = pm->kbd_buf; /* internal layout buffer */

        struct screen *sc = &screens[l];

        kbd_calc_pm_params(pm, sc, &state);

        keyboard_layout(pm->kbd_viewports, pm, sc);
        /* have all the params we need lets set up our viewports */
        kbd_calc_vp_params(pm, sc, &state);
        /* We want these the same height */
        pm->kbd_viewports[eKBD_VP_MENU].height = pm->main_y;
        pm->kbd_viewports[eKBD_VP_PICKER].height = pm->main_y;
    }

    if (global_settings.talk_menu)      /* voice UI? */
        talk_spell(state.text, true);   /* spell initial text */

    while (!done)
    {
        /* These declarations are assigned to the screen on which the key
           action occurred - pointers save a lot of space over array notation
           when accessing the same array element countless times */
        int button;
#if NB_SCREENS > 1
        int button_screen;
#else
        const int button_screen = 0;
#endif
        struct keyboard_parameters *pm;

        state.len_utf8 = utf8length(state.text);

        FOR_NB_SCREENS(l)
        {
            /* declare scoped pointers inside screen loops - hide the
               declarations from previous block level */
            struct screen *sc = &screens[l];
            pm = &param[l];
            sc->clear_display();
            kbd_draw_picker(pm, sc, &state);
            kbd_draw_edit_line(pm, sc, &state);
        }

        FOR_NB_SCREENS(l)
            screens[l].update();

        state.cur_blink = !state.cur_blink;

        button = get_action(
                            CONTEXT_KEYBOARD, HZ/2);
#if NB_SCREENS > 1
        button_screen = (get_action_statuscode(NULL) & ACTION_REMOTE) ? 1 : 0;
#endif
        pm = &param[button_screen];

        /* Remap some buttons to allow to move
         * cursor in line edit mode and morse mode. */
        if (pm->line_edit
            )
        {
            if (button == ACTION_KBD_LEFT)
                button = ACTION_KBD_CURSOR_LEFT;
            if (button == ACTION_KBD_RIGHT)
                button = ACTION_KBD_CURSOR_RIGHT;
        }

        switch ( button )
        {
            case ACTION_KBD_DONE:
                /* accepts what was entered and continues */
                ret = 0;
                done = true;
                break;

            case ACTION_KBD_ABORT:
                ret = -1;
                done = true;
                break;

            case ACTION_KBD_PAGE_FLIP:
                if (++pm->page >= pm->pages)
                    pm->page = 0;

                state.changed = CHANGED_PICKER;
                break;

            case ACTION_KBD_RIGHT:
                kbd_move_picker_horizontal(pm, &state, 1);
                break;

            case ACTION_KBD_LEFT:
                kbd_move_picker_horizontal(pm, &state, -1);
                break;

            case ACTION_KBD_DOWN:
                kbd_move_picker_vertical(pm, &state, 1);
                break;

            case ACTION_KBD_UP:
                kbd_move_picker_vertical(pm, &state, -1);
                break;

            case ACTION_KBD_SELECT:
                /* select doubles as backspace in line_edit */
                if (pm->line_edit)
                    kbd_backspace(&state);
                else
                    kbd_insert_selected(pm, &state);
                break;

            case ACTION_KBD_BACKSPACE:
                kbd_backspace(&state);
                break;

            case ACTION_KBD_CURSOR_RIGHT:
                /* move the caret to the visual right: one position forward in
                 * LTR text, one position back in RTL text */
                kbd_move_cursor(&state,
                    text_is_rtl((const unsigned char *)state.text) ? -1 : 1);
                break;

            case ACTION_KBD_CURSOR_LEFT:
                kbd_move_cursor(&state,
                    text_is_rtl((const unsigned char *)state.text) ? 1 : -1);
                break;

            case ACTION_NONE:
                break;

            default:
                if (default_event_handler(button) == SYS_USB_CONNECTED)
                {
                    FOR_NB_SCREENS(l)
                        screens[l].setfont(FONT_SYSFIXED);
                }
                break;

        } /* end switch */

        if (button != ACTION_NONE)
        {
            state.cur_blink = true;
        }
        if (global_settings.talk_menu)  /* voice UI? */
        {
            if (state.changed == CHANGED_PICKER)
            {
                if (pm->line_edit)
                {
                    talk_id(VOICE_EDIT, false);
                }
                else
                {
                    ch = get_kbd_ch(pm, pm->x, pm->y);
                    kbd_spellchar(ch);
                }
            }
            else if (state.changed == CHANGED_CURSOR)
            {
                int c = utf8seek(state.text, state.editpos);
                ucschar_t ch;
                utf8decode(&state.text[c], &ch);
                kbd_spellchar(ch);
            }
            else if (state.changed == CHANGED_TEXT)
                talk_spell(state.text, false);  /* speak revised text */
        }
        state.changed = 0;
    }

    if (ret < 0)
        splash(HZ/2, ID2P(LANG_CANCEL));

cleanup:
    FOR_NB_SCREENS(l)
    {
        screens[l].setfont(FONT_UI);
        viewportmanager_theme_undo(l, false);
    }
    return ret;
}
static void kbd_calc_pm_params(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state)
{
    struct font* font;
    const unsigned char *p;
    ucschar_t ch, *pbuf;
    unsigned int i, w;

    pm->curfont = pm->default_lines ? FONT_SYSFIXED : sc->getuifont();
    font = font_get(pm->curfont);
    pm->font_h = font->height;

    /* check if FONT_UI fits the screen */
    if (pm->font_h*2 + 3 > sc->getheight())
    {
        pm->curfont = FONT_SYSFIXED;
        font = font_get(FONT_SYSFIXED);
        pm->font_h = font->height;
    }

    /* find max width of keyboard glyphs.
     * since we're going to be adding spaces,
     * max width is at least their width */
    pm->font_w = font_get_width(font, ' ');
    for (pbuf = pm->kbd_buf_ptr; *pbuf != 0xFEFF; pbuf += i)
    {
        for (i = 0; ++i <= *pbuf; )
        {
            w = font_get_width(font, pbuf[i]);
            if (pm->font_w < w)
                pm->font_w = w;
        }
    }

    /* Find max width for text string */
    pm->text_w = pm->font_w;
    p = state->text;
    while (*p)
    {
        p = utf8decode(p, &ch);
        w = font_get_width(font, ch);
        if (pm->text_w < w)
            pm->text_w = w;
    }

}

static void kbd_calc_vp_params(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state)
{
    (void) state;
    struct viewport *vp = &pm->kbd_viewports[eKBD_VP_PICKER];
    unsigned int icon_w, sc_w, sc_h;
    int i, total_lines;
    ucschar_t *pbuf;
    /* calculate how many characters to put in a row. */
    icon_w = get_icon_width(sc->screen_type);

    sc_w = vp->width; /**sc->getwidth();**/
    if (pm->font_w < sc_w / pm->max_line_len)
        pm->font_w = sc_w / pm->max_line_len;
    pm->max_chars = sc_w / pm->font_w;
    pm->max_chars_text = (sc_w - icon_w * 2 - 2) / pm->text_w;
    if (pm->max_chars_text < 3 && icon_w > pm->text_w)
        pm->max_chars_text = sc_w / pm->text_w - 2;

    /* calculate pm->pages and pm->lines */
    sc_h = vp->height;/**sc->getheight()**/;
    pm->lines = sc_h / pm->font_h - 1;

    if (pm->default_lines && pm->lines > pm->default_lines)
        pm->lines = pm->default_lines;

    pm->keyboard_margin = sc_h - (pm->lines+1)*pm->font_h;

    if (pm->keyboard_margin < 3 && pm->lines > 1)
    {
        pm->lines--;
        pm->keyboard_margin += pm->font_h;
    }

    if (pm->keyboard_margin > DEFAULT_MARGIN)
        pm->keyboard_margin = DEFAULT_MARGIN;

    total_lines = 0;
    for (pbuf = pm->kbd_buf_ptr; (i = *pbuf) != 0xFEFF; pbuf += i + 1)
        total_lines += (i ? (i + pm->max_chars - 1) / pm->max_chars : 1);

    pm->pages = (total_lines + pm->lines - 1) / pm->lines;
    pm->lines = (total_lines + pm->pages - 1) / pm->pages;
    if (pm->page >= pm->pages)
        pm->x = pm->y = pm->page = 0;

    pm->main_y = pm->font_h*pm->lines + pm->keyboard_margin;
    pm->keyboard_margin -= pm->keyboard_margin/2;

}

static void kbd_draw_picker(struct keyboard_parameters *pm,
                            struct screen *sc, struct edit_state *state)
{
    struct viewport *last;
    struct viewport *vp = &pm->kbd_viewports[eKBD_VP_PICKER];
    last = sc->set_viewport(vp);
    sc->clear_viewport();

    char outline[8];
    (void) state;
    {
        /* draw page */
        int i, j;
        int w, h;
        ucschar_t ch;
        unsigned char *utf8;

        sc->setfont(pm->curfont);

        for (j = 0; j < pm->lines; j++)
        {
            for (i = 0; i < pm->max_chars; i++)
            {
                int cx = i*pm->font_w, cy = j*pm->font_h;
                ch = get_kbd_ch(pm, i, j);
                if (ch == KBD_SPACE)
                {
                    /* space bar: a bar with short upturned ends. Cells join;
                     * the run's first/last cell adds the vertical end tick. */
                    int th = MAX(2, pm->font_h/14);
                    int by = cy + (pm->font_h*2)/3;
                    int tk = pm->font_h/6;
                    int rx = cx + pm->font_w - th;
                    sc->fillrect(cx, by, pm->font_w, th);
                    if (i == 0 || get_kbd_ch(pm, i-1, j) != KBD_SPACE)
                        sc->fillrect(cx, by - tk, th, tk + th);
                    if (get_kbd_ch(pm, i+1, j) != KBD_SPACE)
                        sc->fillrect(rx, by - tk, th, tk + th);
                    continue;
                }
                utf8 = utf8encode(ch, outline);
                *utf8 = 0;

                sc->getstringsize(outline, &w, &h);
                sc->putsxy(cx + (pm->font_w-w) / 2,
                           cy + (pm->font_h-h) / 2, outline);
            }
        }

        if (!pm->line_edit)
        {
            /* highlight the focused key; the space bar spans several cells and
             * highlights as one */
            int hx = pm->x, hw = 1;
            if (get_kbd_ch(pm, hx, pm->y) == KBD_SPACE)
            {
                while (hx > 0 && get_kbd_ch(pm, hx - 1, pm->y) == KBD_SPACE)
                    hx--;
                while (get_kbd_ch(pm, hx + hw, pm->y) == KBD_SPACE)
                    hw++;
            }
            sc->set_drawmode(DRMODE_COMPLEMENT);
            sc->fillrect(pm->font_w*hx, pm->font_h*pm->y,
                         pm->font_w*hw, pm->font_h);
            sc->set_drawmode(DRMODE_SOLID);
        }
    }
    sc->set_viewport(last);
}

/* Pixel width of the single UTF-8 character that starts at `p`. */
static int kbd_char_width(struct screen *sc, const unsigned char *p)
{
    ucschar_t c;
    unsigned char tmp[8];
    unsigned char *e;
    int w;
    utf8decode(p, &c);
    e = utf8encode(c, tmp);
    *e = '\0';
    sc->getstringsize((char *)tmp, &w, NULL);
    return w;
}

/* Visual x-offset (pixels, from the start of the drawn line) of the caret that
 * sits at logical character index `cpos` of the visible string `vis`. It mirrors
 * how the text engine lays the line out: bidi with LTR base orientation, where
 * non-RTL runs keep their order while Hebrew/Arabic runs are reversed in place.
 * The string is scanned once, run by run, accumulating a running offset and
 * stopping at the run that holds the caret, so no per-character buffer is kept.
 * (Like the rest of the edit line, it does not handle diacritics that combine
 * several codepoints into a single displayed glyph.) */
static int kbd_caret_visual_x(struct screen *sc, const char *vis, int cpos)
{
    const unsigned char *p = (const unsigned char *)vis;
    int x = 0;          /* offset contributed by runs to the left of the caret */
    int pos = 0;        /* logical index of the next character */

    while (*p)
    {
        ucschar_t c;
        utf8decode(p, &c);
        bool heb = is_rtl_char(c);
        int run_start = pos;
        int w_run = 0;      /* total width of this run */
        int w_before = 0;   /* width of this run's chars before the caret */

        while (*p)
        {
            const unsigned char *next = utf8decode(p, &c);
            /* Spaces/tabs are direction-neutral: they stay inside the current
             * run so a multi-word Hebrew string is one reversed run (matching
             * the bidi engine) instead of splitting at every space, which would
             * misplace the caret. */
            if ((bool)is_rtl_char(c) != heb && c != ' ' && c != '\t')
                break;       /* end of run */
            int w = kbd_char_width(sc, p);
            if (pos < cpos)
                w_before += w;
            w_run += w;
            pos++;
            if (next == p)   /* guard against a malformed byte */
                break;
            p = next;
        }

        if (cpos <= run_start)          /* caret is before this run */
            break;
        if (cpos >= pos)                /* caret is past this whole run */
        {
            x += w_run;
            continue;
        }
        /* caret is inside this run */
        x += heb ? (w_run - w_before)   /* reversed: later chars sit to the left */
                 : w_before;            /* in order: earlier chars sit to the left */
        break;
    }
    return x;
}

static void kbd_draw_edit_line(struct keyboard_parameters *pm,
                               struct screen *sc, struct edit_state *state)
{
    int w;
    int icon_w, icon_y;
    struct viewport *last;
    struct viewport *vp = &pm->kbd_viewports[eKBD_VP_TEXT];
    last = sc->set_viewport(vp);
    sc->clear_viewport();
    sc->hline(1, vp->width - 1, vp->height - 1);

    int sc_w = vp->width;
    int y = (vp->height - pm->font_h) / 2;

    sc->setfont(pm->curfont);

    /* Base direction follows the text itself (first strong character): a
     * Hebrew/Arabic name is laid out right-to-left while a Latin name stays
     * readable left-to-right, even under an RTL UI language. */
    bool rtl = text_is_rtl((const unsigned char *)state->text);

    icon_w = get_icon_width(sc->screen_type);
    icon_y = (vp->height - get_icon_height(sc->screen_type)) / 2;

    /* Decide how much of the text to show. If the whole name fits we show it
     * all using (almost) the full width; only when it is genuinely too long do
     * we scroll and reserve a slot on each side for the scroll indicators.
     * Widths are measured per glyph (not from a fixed representative width), so
     * every pixel is used and a name that fits is never shown as "scrolled". */
    int total_w;
    sc->getstringsize(state->text, &total_w, NULL);

    int pad = 2;
    bool overflow = total_w > sc_w - 2 * pad;
    int margin = overflow ? (icon_w + 2) : pad;
    int area_l = margin;
    int area_r = sc_w - margin;
    int avail  = area_r - area_l;
    if (avail < 1)
        avail = sc_w;

    int leftpos, endchar;
    if (!overflow)
    {
        leftpos = 0;
        endchar = state->len_utf8;
    }
    else
    {
        /* keep the caret in view: take the characters just before it until the
         * line is full, then extend forward as far as still fits */
        int acc = 0;
        leftpos = state->editpos;
        while (leftpos > 0)
        {
            int cw = kbd_char_width(sc,
                        state->text + utf8seek(state->text, leftpos - 1));
            if (acc + cw > avail)
                break;
            acc += cw;
            leftpos--;
        }
        endchar = state->editpos;
        while (endchar < state->len_utf8)
        {
            int cw = kbd_char_width(sc,
                        state->text + utf8seek(state->text, endchar));
            if (acc + cw > avail)
                break;
            acc += cw;
            endchar++;
        }
    }
    pm->leftpos = leftpos;
    pm->curpos = state->editpos - leftpos;

    /* The whole visible window is drawn as ONE string so the bidi engine lays
     * out mixed Hebrew/Latin runs correctly (splitting it at the caret would
     * break a run and garble the text). The caret is placed from the width of
     * the logical text before it: exact for uniform-direction text, and a
     * sensible approximation inside mixed runs. */
    int startbyte = utf8seek(state->text, leftpos);
    int endbyte   = utf8seek(state->text, endchar);

    char linebuf[256];
    int vlen = endbyte - startbyte;
    if (vlen < 0) vlen = 0;
    if (vlen >= (int)sizeof(linebuf)) vlen = sizeof(linebuf) - 1;
    memcpy(linebuf, state->text + startbyte, vlen);
    linebuf[vlen] = '\0';

    int vis_w = 0;
    sc->getstringsize(linebuf, &vis_w, NULL);

    /* Place the line: right-aligned for an RTL name, left-aligned otherwise. */
    int textx = rtl ? (area_r - vis_w) : area_l;
    if (textx < area_l)
        textx = area_l;
    sc->putsxy(textx, y, linebuf);

    /* Caret = line origin + the visual offset of the logical caret position,
     * computed the same way bidi lays the line out (so it is correct for mixed
     * Hebrew/Latin too). */
    int cursor_x = textx + kbd_caret_visual_x(sc, linebuf, pm->curpos);

    /* indicator: earlier text scrolled out of view (opposite side for RTL) */
    if (pm->leftpos > 0)
    {
        if (margin >= icon_w)
            screen_put_icon_with_offset(sc, 0, 0,
                rtl ? (sc_w - icon_w) : 0, icon_y,
                rtl ? Icon_Cursor : Icon_Reverse_Cursor);
        else
        {
            sc->getstringsize(rtl ? ">" : "<", &w, NULL);
            sc->putsxy(rtl ? (sc_w - w) : 0, y, rtl ? ">" : "<");
        }
    }

    /* indicator: later text scrolled out of view */
    if (endchar < state->len_utf8)
    {
        if (margin >= icon_w)
            screen_put_icon_with_offset(sc, 0, 0,
                rtl ? 0 : (sc_w - icon_w), icon_y,
                rtl ? Icon_Reverse_Cursor : Icon_Cursor);
        else
        {
            sc->getstringsize(rtl ? "<" : ">", &w, NULL);
            sc->putsxy(rtl ? 0 : (sc_w - w), y, rtl ? "<" : ">");
        }
    }

    /* caret */
    if (state->cur_blink)
        sc->vline(cursor_x, y, y + pm->font_h - 1);

    if (state->hangul) /* draw underbar */
        sc->hline(cursor_x - pm->text_w, cursor_x, y + pm->font_h - 1);

    if (pm->line_edit)
    {
        sc->set_drawmode(DRMODE_COMPLEMENT);
        sc->fillrect(0, y - 1, sc_w, pm->font_h + 2);
        sc->set_drawmode(DRMODE_SOLID);
    }

    sc->set_viewport(last);
}

/* inserts the selected char */
static void kbd_insert_selected(struct keyboard_parameters *pm,
                                struct edit_state *state)
{
    /* find input char */
    ucschar_t ch = get_kbd_ch(pm, pm->x, pm->y);

    if (ch == KBD_SPACE)
        ch = ' ';

    /* check for hangul input */
    if (ch >= 0x3131 && ch <= 0x3163)
    {
        ucschar_t tmp;

        if (!state->hangul)
        {
            state->hlead = state->hvowel = state->htail = 0;
            state->hangul = true;
        }

        if (!state->hvowel)
        {
            state->hvowel = ch;
        }
        else if (!state->htail)
        {
            state->htail = ch;
        }
        else
        {
            /* previous hangul complete */
            /* check whether tail is actually lead of next char */
            tmp = hangul_join(state->htail, ch, 0);

            if (tmp != 0xfffd)
            {
                tmp = hangul_join(state->hlead, state->hvowel, 0);
                kbd_delchar(state);
                kbd_inschar(state, tmp);
                /* insert dummy char */
                kbd_inschar(state, ' ');
                state->hlead = state->htail;
                state->hvowel = ch;
                state->htail = 0;
            }
            else
            {
                state->hvowel = state->htail = 0;
                state->hlead = ch;
            }
        }

        /* combine into hangul */
        tmp = hangul_join(state->hlead, state->hvowel, state->htail);

        if (tmp != 0xfffd)
        {
            kbd_delchar(state);
            ch = tmp;
        }
        else
        {
            state->hvowel = state->htail = 0;
            state->hlead = ch;
        }
    }
    else
    {
        state->hangul = false;
    }

    /* insert char */
    kbd_inschar(state, ch);
}

static void kbd_backspace(struct edit_state *state)
{
    ucschar_t ch;
    if (state->hangul)
    {
        if (state->htail)
            state->htail = 0;
        else if (state->hvowel)
            state->hvowel = 0;
        else
            state->hangul = false;
    }

    kbd_delchar(state);

    if (state->hangul)
    {
        if (state->hvowel)
            ch = hangul_join(state->hlead, state->hvowel, state->htail);
        else
            ch = state->hlead;
        kbd_inschar(state, ch);
    }
}

static void kbd_move_cursor(struct edit_state *state, int dir)
{
    state->hangul = false;
    state->editpos += dir;

    if (state->editpos >= 0 && state->editpos <= state->len_utf8)
    {
        state->changed = CHANGED_CURSOR;
    }
    else if (global_settings.list_wraparound && state->editpos > state->len_utf8)
    {
        state->editpos = 0;
        if (global_settings.talk_menu) beep_play(1000, 150, 1500);
    }
    else if (global_settings.list_wraparound && state->editpos < 0)
    {
        state->editpos = state->len_utf8;
        if (global_settings.talk_menu) beep_play(1000, 150, 1500);
    }
    else if (!global_settings.list_wraparound)
        state->editpos -= dir;
}

static void kbd_move_picker_horizontal(struct keyboard_parameters *pm,
                                       struct edit_state *state, int dir)
{
    state->changed = CHANGED_PICKER;

    pm->x += dir;
    /* The space bar spans several cells but highlights and inserts as one key,
     * so step over the whole run instead of stopping inside it, which looks
     * like the cursor is stuck. */
    if (pm->x >= 0 && pm->x < pm->max_chars &&
        get_kbd_ch(pm, pm->x, pm->y) == KBD_SPACE &&
        get_kbd_ch(pm, pm->x - dir, pm->y) == KBD_SPACE)
    {
        while (pm->x >= 0 && pm->x < pm->max_chars &&
               get_kbd_ch(pm, pm->x, pm->y) == KBD_SPACE)
            pm->x += dir;
    }

    if (pm->x < 0)
    {
        if (!global_settings.list_wraparound && pm->page == 0)
        {
            pm->x = 0;
            return;
        }
        if (--pm->page < 0)
            pm->page = pm->pages - 1;
        pm->x = pm->max_chars - 1;
    }
    else if (pm->x >= pm->max_chars)
    {
        if (!global_settings.list_wraparound && pm->page == pm->pages - 1)
        {
            pm->x = pm->max_chars - 1;
            return;
        }
        if (++pm->page >= pm->pages)
            pm->page = 0;
        pm->x = 0;
    }
}

static void kbd_move_picker_vertical(struct keyboard_parameters *pm,
                                     struct edit_state *state, int dir)
{
    state->changed = CHANGED_PICKER;

    pm->y += dir;

    if (!global_settings.list_wraparound)
    {
        if (pm->y >= pm->lines)
        {
            pm->y = pm->lines;
        }
        else if (pm->y < 0)
        {
            pm->line_edit = true;
            pm->y = 0;
        }
        else if (pm->line_edit)
        {
            pm->line_edit = false;
            pm->y = 0;
        }
        return;
    }

    if (pm->line_edit)
    {
        pm->y = (dir > 0 ? 0 : pm->lines - 1);
        pm->line_edit = false;
    }
    else if (pm->y < 0 || pm->y >= pm->lines)
    {
        pm->line_edit = true;
    }
}
