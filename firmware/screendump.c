/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2009 by Jens Arnold
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
#include "screendump.h"
#include "rbpaths.h"

#include "file.h"
#include "general.h"
#include "lcd.h"
#include "stdlib.h"
#include "string.h"
#include "system.h"

#define BMP_COMPRESSION 3 /* BI_BITFIELDS */
#define BMP_NUMCOLORS 3

#define BMP_HEADERSIZE (54 + 4 * BMP_NUMCOLORS)
#define BMP_DATASIZE   (DUMP_BMP_LINESIZE * (LCD_HEIGHT+LCD_SPLIT_LINES))
#define BMP_TOTALSIZE  (BMP_HEADERSIZE + BMP_DATASIZE)

static const unsigned char bmpheader[] =
{
    0x42, 0x4d,                 /* 'BM' */
    LE32_CONST(BMP_TOTALSIZE),  /* Total file size */
    0x00, 0x00, 0x00, 0x00,     /* Reserved */
    LE32_CONST(BMP_HEADERSIZE), /* Offset to start of pixel data */

    0x28, 0x00, 0x00, 0x00,     /* Size of (2nd) header */
    LE32_CONST(LCD_WIDTH),      /* Width in pixels */
    LE32_CONST(LCD_HEIGHT+LCD_SPLIT_LINES),  /* Height in pixels */
    0x01, 0x00,                 /* Number of planes (always 1) */
    LE16_CONST(DUMP_BMP_BPP),   /* Bits per pixel 1/4/8/16/24 */
    LE32_CONST(BMP_COMPRESSION),/* Compression mode */
    LE32_CONST(BMP_DATASIZE),   /* Size of bitmap data */
    0xc4, 0x0e, 0x00, 0x00,     /* Horizontal resolution (pixels/meter) */
    0xc4, 0x0e, 0x00, 0x00,     /* Vertical resolution (pixels/meter) */
    LE32_CONST(BMP_NUMCOLORS),  /* Number of used colours */
    LE32_CONST(BMP_NUMCOLORS),  /* Number of important colours */

    0x00, 0xf8, 0x00, 0x00,     /* red bitfield mask */
    0xe0, 0x07, 0x00, 0x00,     /* green bitfield mask */
    0x1f, 0x00, 0x00, 0x00,     /* blue bitfield mask */
};

static void (*screen_dump_hook)(int fh) = NULL;

void screen_dump(void)
{
    int fd, y;
    char filename[MAX_PATH];

    fb_data *src;
    unsigned short *dst, *dst_end;
    unsigned short linebuf[DUMP_BMP_LINESIZE/2];

    create_datetime_filename(filename, HOME_DIR, "dump ", ".bmp", false);

    fd = creat(filename, 0666);
    if (fd < 0)
        return;

    if (screen_dump_hook)
    {
        screen_dump_hook(fd);
    }
    else
    {
        if(write(fd, bmpheader, sizeof(bmpheader)) != sizeof(bmpheader))
        {
            close(fd);
            return;
        }

        /* BMP image goes bottom up */
        for (y = LCD_HEIGHT - 1; y >= 0; y--)
        {
            memset(linebuf, 0, DUMP_BMP_LINESIZE);

            dst = linebuf;

            dst_end = dst + LCD_WIDTH;
            src = FBADDR(0, y);

            do
            {
                *dst++ = htole16(*src++);
            }
            while (dst < dst_end);
            if(write(fd, linebuf, DUMP_BMP_LINESIZE) != DUMP_BMP_LINESIZE)
            {
                close(fd);
                return;
            }
        }
    }
    close(fd);
}

void screen_dump_set_hook(void (*hook)(int fh))
{
    screen_dump_hook = hook;
}

