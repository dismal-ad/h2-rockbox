/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Gadi Cohen
 *
 * Largely based on php_hebrev by Zeev Suraski <zeev@php.net>
 * Heavily modified by Gadi Cohen aka Kinslayer <dragon@wastelands.net>
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "file.h"
#include "lcd.h"
#include "rbunicode.h"
#include "arabjoin.h"
#include "scroll_engine.h"
#include "bidi.h"

/* #define _HEB_BUFFER_LENGTH (MAX_PATH + LCD_WIDTH/2 + 3 + 2 + 2) * 2 */
#define _HEB_BLOCK_TYPE_ENG 1
#define _HEB_BLOCK_TYPE_HEB 0
#define _HEB_ORIENTATION_LTR 1
#define _HEB_ORIENTATION_RTL 0

/* ischar() now lives in bidi.h as the shared is_rtl_char() macro */
#define _isblank(c) ((c==' ' || c=='\t') ? 1 : 0)
#define _isnewline(c) ((c=='\n' || c=='\r') ? 1 : 0)
/* ctype ispunct() is only defined for unsigned char values; feeding it a
 * decoded Unicode codepoint (e.g. Hebrew final kaf U+05DA, gimel U+05D2) is
 * undefined and some libcs then mis-report it as punctuation, which trims the
 * final letter off its RTL block and throws it to the wrong end of the line.
 * Restrict the punctuation test to ASCII. */
#define _ispunct(c) ((c) < 0x80 && ispunct((int)(c)))
#define XOR(a,b) ((a||b) && !(a&&b))

/* True if the first strongly-directional character of the string is RTL, i.e.
 * the string should be laid out right-to-left. */
bool text_is_rtl(const unsigned char *str)
{
    while (*str)
    {
        ucschar_t c;
        const unsigned char *next = utf8decode(str, &c);
        if (is_rtl_char(c))
            return true;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            return false;
        if (next == str)
            break;
        str = next;
    }
    return false;
}

static const arab_t * arab_lookup(ucschar_t uchar)
{
    if (uchar >= 0x621 && uchar <= 0x63a)
        return &(jointable[uchar - 0x621]);
    if (uchar >= 0x640 && uchar <= 0x64a)
        return &(jointable[uchar - 0x621 - 5]);
    if (uchar >= 0x671 && uchar <= 0x6d5)
        return &(jointable[uchar - 0x621 - 5 - 38]);
    if (uchar == 0x200D) /* Support for the zero-width joiner */
        return &zwj;
    return 0;
}

static void arabjoin(ucschar_t *stringprt, int length)
{
    bool connected = false;
    ucschar_t *writeprt = stringprt;

    const arab_t * prev = 0;
    const arab_t * cur;
    const arab_t * ligature = 0;
    ucschar_t uchar;

    int i;
    for (i = 0; i <= length; i++) {
        cur = arab_lookup(uchar = *stringprt++);

        /* Skip non-arabic chars */
        if (cur == 0) {
            if (prev) {
                /* Finish the last char */
                if (connected) {
                    *writeprt++ = prev->final;
                    connected = false;
                } else
                    *writeprt++ = prev->isolated;
                prev = 0;
                *writeprt++ = uchar;
            } else {
                *writeprt++ = uchar;
            }
            continue;
        }

        /* nothing to do for arabic char if the previous was non-arabic */
        if (prev == 0) {
            prev = cur;
            continue;
        }

        /* if it's LAM, check for LAM+ALEPH ligatures */
        if (prev->isolated == 0xfedd) {
            switch (cur->isolated) {
                case 0xfe8d:
                    ligature = &(lamaleph[0]);
                    break;
                case 0xfe87:
                    ligature = &(lamaleph[1]);
                    break;
                case 0xfe83:
                    ligature = &(lamaleph[2]);
                    break;
                case 0xfe81:
                    ligature = &(lamaleph[3]);
            }
        }

        if (ligature) { /* replace the 2 glyphs by their ligature */
            prev = ligature;
            ligature = 0;
        } else {
            if (connected) { /* previous char has something connected to it */
                if (prev->medial && cur->final) /* Can we connect to it? */
                    *writeprt++ = prev->medial;
                else {
                    *writeprt++ = prev->final;
                    connected = false;
                }
            } else {
                if (prev->initial && cur->final) { /* Can we connect to it? */
                    *writeprt++ = prev->initial;
                    connected = true;
                } else
                    *writeprt++ = prev->isolated;
            }
            prev = cur;
        }
    }
}

ucschar_t *bidi_l2v(const unsigned char *str, int orientation)
{
    static ucschar_t utf_buf[SCROLL_LINE_SIZE];
    ucschar_t *target, *tmp;
    static ucschar_t bidi_buf[SCROLL_LINE_SIZE];
    ucschar_t *heb_str; /* *broken_str */
    int block_start, block_end, block_type, block_length, i;
    int length = utf8length(str);
    length=length>=SCROLL_LINE_SIZE?SCROLL_LINE_SIZE-1:length;
    /*
    long max_chars=0;
    int begin, end, char_count, orig_begin;

    tmp = str;
    */
    target = tmp = utf_buf;
    while (*str && target < &utf_buf[SCROLL_LINE_SIZE-1])
        str = utf8decode(str, target++);
    *target = 0;

    if (target == utf_buf) /* empty string */
        return target;

    /* properly join any arabic chars */
    arabjoin(utf_buf, length);

    block_start=block_end=block_length=0;

    heb_str = bidi_buf;
    if (orientation) {
        target = heb_str;
    } else {
        target = heb_str + length;
        *target = 0;
        target--;
    }

    if (is_rtl_char(*tmp))
        block_type = _HEB_BLOCK_TYPE_HEB;
    else
        block_type = _HEB_BLOCK_TYPE_ENG;

    do {
        while((XOR(is_rtl_char(*(tmp+1)),block_type)
               || _isblank(*(tmp+1)) || _ispunct(*(tmp+1))
               || *(tmp+1)=='\n')
              && block_end < length-1) {
                tmp++;
                block_end++;
                block_length++;
        }

        if (block_type != orientation) {
            while ((_isblank(*tmp) || _ispunct(*tmp))
                   && *tmp!='/' && *tmp!='-' && block_end>block_start) {
                tmp--;
                block_end--;
            }
        }

        for (i=block_start; i<=block_end; i++) {
            *target = (block_type == orientation) ?
                      *(utf_buf+i) : *(utf_buf+block_end-i+block_start);
            if (block_type!=orientation) {
                switch (*target) {
                case '(':
                    *target = ')';
                    break;
                case ')':
                    *target = '(';
                    break;
                default:
                    break;
                }
            }
            target += orientation ? 1 : -1;
        }
        block_type = !block_type;
        block_start=block_end+1;
    } while(block_end<length-1);

    *target = 0;

    return heb_str;
}
