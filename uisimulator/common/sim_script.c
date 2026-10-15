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

/* Scripted input for the simulator, for repeatable UI tests.
 *
 * If RB_SCRIPT names a file, a thread runs its commands once the UI is up:
 *
 *   wait <ms>               sleep
 *   press <button> [ms]     press and release (held 60 ms by default)
 *   hold <button> <ms>      press, hold, release
 *   wheel fwd|back [n]      n scroll wheel ticks, 40 ms apart
 *   shot <name>             write the screen to $RB_SHOTDIR/<name>.png
 *   quit                    exit the simulator
 *
 * Buttons: power menu back play prev next volup voldown.
 * Blank lines and lines starting with '#' are ignored. Run headless with
 * SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <SDL.h>
#include "config.h"
#include "kernel.h"
#include "thread.h"
#include "lcd.h"
#include "button.h"
#include "backlight.h"
#include "powermgmt.h"
#include "sim_script.h"

/* host stdio, not the simulator's virtual disk */
#undef fopen
#undef fclose
#undef fgets
#undef fwrite

int sim_script_buttons = 0;

static long script_stack[DEFAULT_STACK_SIZE / sizeof(long)];
static FILE *script_file;

static const struct { const char *name; int button; } buttons[] = {
    { "power",   BUTTON_POWER },
    { "menu",    BUTTON_MENU },
    { "back",    BUTTON_BACK },
    { "play",    BUTTON_PLAY },
    { "prev",    BUTTON_PREV },
    { "next",    BUTTON_NEXT },
    { "volup",   BUTTON_VOL_UP },
    { "voldown", BUTTON_VOL_DOWN },
};

static void wait_ms(long ms)
{
    sleep((ms * HZ + 999) / 1000);
}

static int button_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
        if (!strcmp(buttons[i].name, name))
            return buttons[i].button;
    return 0;
}

/* --- PNG writer: RGB, uncompressed deflate blocks --- */

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    put32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len)
        fwrite(data, 1, len, f);
    uint32_t crc = crc32_update(crc32_update(0, (const uint8_t *)type, 4), data, len);
    uint8_t c[4];
    put32(c, crc);
    fwrite(c, 1, 4, f);
}

static void write_png(const char *path)
{
    enum { ROW = 1 + LCD_WIDTH * 3, RAW = ROW * LCD_HEIGHT };
    static uint8_t raw[RAW];
    /* deflate stored blocks: 5 bytes overhead per 65535, plus zlib 2+4 */
    static uint8_t zbuf[RAW + (RAW / 65535 + 1) * 5 + 6];

    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        uint8_t *r = raw + y * ROW;
        fb_data *src = FBADDR(0, y);
        *r++ = 0; /* filter: none */
        for (int x = 0; x < LCD_WIDTH; x++)
        {
            unsigned px = FB_UNPACK_SCALAR_LCD(src[x]);
            *r++ = RGB_UNPACK_RED(px);
            *r++ = RGB_UNPACK_GREEN(px);
            *r++ = RGB_UNPACK_BLUE(px);
        }
    }

    size_t z = 0, left = RAW;
    const uint8_t *p = raw;
    uint32_t a = 1, b = 0;
    zbuf[z++] = 0x78; zbuf[z++] = 0x01;
    while (left)
    {
        size_t n = left > 65535 ? 65535 : left;
        left -= n;
        zbuf[z++] = left ? 0 : 1;
        zbuf[z++] = n & 0xff; zbuf[z++] = n >> 8;
        zbuf[z++] = ~n & 0xff; zbuf[z++] = (~n >> 8) & 0xff;
        for (size_t i = 0; i < n; i++)
        {
            a = (a + p[i]) % 65521;
            b = (b + a) % 65521;
        }
        memcpy(zbuf + z, p, n);
        z += n; p += n;
    }
    put32(zbuf + z, (b << 16) | a);
    z += 4;

    FILE *f = fopen(path, "wb");
    if (!f)
    {
        printf("script: cannot write %s\n", path);
        return;
    }
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    uint8_t ihdr[13];
    put32(ihdr, LCD_WIDTH);
    put32(ihdr + 4, LCD_HEIGHT);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    fwrite(sig, 1, 8, f);
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", zbuf, z);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    printf("script: wrote %s\n", path);
}

/* --- script thread --- */

static void tick_wheel(int button)
{
    backlight_on();
    reset_poweroff_timer();
    button_queue_post(button, 1 << 24);
}

static void script_thread(void)
{
    char line[256], cmd[32], arg[200];
    const char *dir = getenv("RB_SHOTDIR");
    long lineno = 0;

    if (!dir)
        dir = ".";

    while (fgets(line, sizeof(line), script_file))
    {
        lineno++;
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '#' || *s == '\n' || *s == '\0')
            continue;

        long n = 0;
        int fields = sscanf(s, "%31s %199s %ld", cmd, arg, &n);
        if (!strcmp(cmd, "wait") && fields >= 2)
        {
            wait_ms(atol(arg));
        }
        else if ((!strcmp(cmd, "press") || !strcmp(cmd, "hold")) && fields >= 2)
        {
            int b = button_by_name(arg);
            if (!b)
            {
                printf("script:%ld: unknown button '%s'\n", lineno, arg);
                continue;
            }
            backlight_on();
            sim_script_buttons |= b;
            wait_ms(fields >= 3 ? n : 60);
            sim_script_buttons &= ~b;
            wait_ms(40);
        }
        else if (!strcmp(cmd, "wheel") && fields >= 2)
        {
            int b = !strcmp(arg, "back") ? BUTTON_SCROLL_BACK : BUTTON_SCROLL_FWD;
            for (long i = 0; i < (fields >= 3 ? n : 1); i++)
            {
                tick_wheel(b);
                wait_ms(40);
            }
        }
        else if (!strcmp(cmd, "shot") && fields >= 2)
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s.png", dir, arg);
            write_png(path);
        }
        else if (!strcmp(cmd, "quit"))
        {
            break;
        }
        else
        {
            printf("script:%ld: cannot parse: %s", lineno, s);
        }
    }

    fclose(script_file);
    SDL_Event quit = { .type = SDL_QUIT };
    SDL_PushEvent(&quit);
    while (1)
        sleep(HZ);
}

void sim_script_init(void)
{
    const char *path = getenv("RB_SCRIPT");
    if (!path)
        return;
    script_file = fopen(path, "r");
    if (!script_file)
    {
        printf("script: cannot open %s\n", path);
        return;
    }
    create_thread(script_thread, script_stack, sizeof(script_stack), 0,
                  "script" IF_PRIO(, PRIORITY_BACKGROUND) IF_COP(, CPU));
}
