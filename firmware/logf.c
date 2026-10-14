/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Daniel Stenberg
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

/*
 * logf() logs entries in a circular buffer. Each logged string is null-terminated.
 *
 * When the length of log exceeds MAX_LOGF_SIZE bytes, the buffer wraps.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "system.h"
#include "font.h"
#include "lcd.h"
#include "logf.h"
#include "serial.h"
#include "vuprintf.h"

#ifdef HAVE_USBSTACK
#include "usb_core.h"
#include "usbstack/usb_serial.h"
#else
#undef USB_ENABLE_SERIAL
#endif

#ifdef ROCKBOX_HAS_LOGDISKF
#include "logdiskf.h"
#include "file.h"
#include "rbpaths.h"
#include "ata_idle_notify.h"

unsigned char logdiskfbuffer[MAX_LOGDISKF_SIZE];
static int logdiskfindex;
#endif

/* Only provide all this if asked to */
#ifdef ROCKBOX_HAS_LOGF

#ifndef __PCTOOL__
unsigned char logfbuffer[MAX_LOGF_SIZE + 1];
int logfindex;
bool logfwrap;
bool logfenabled = true;
#endif

#define displayremote()

#ifdef __PCTOOL__
void _logf(const char *format, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, format);

    vsnprintf(buf, sizeof buf, format, ap);
    printf("DEBUG: %s\n", buf);
}
#else
static void check_logfindex(void)
{
    if(logfindex >= MAX_LOGF_SIZE)
    {
        /* wrap */
        logfwrap = true;
        logfindex = 0;
    }
}

static int logf_push(void *userp, int c)
{
    (void)userp;

    logfbuffer[logfindex++] = c;
    check_logfindex();

    return 1;
}

void _logf(const char *fmt, ...)
{
    if (!logfenabled)
        return;

    #ifdef USB_ENABLE_SERIAL
    int old_logfindex = logfindex;
    #endif
    va_list ap;

    va_start(ap, fmt);

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    DEBUGF("%s\n", buf);
    /* restart va_list otherwise the result if undefined when vuprintf is called */
    va_end(ap);
    va_start(ap, fmt);
#endif

    vuprintf(logf_push, NULL, fmt, ap);
    va_end(ap);

    /* add trailing zero */
    logf_push(NULL, '\0');

#ifdef USB_ENABLE_SERIAL

    if(logfindex < old_logfindex)
    {
        usb_serial_send(logfbuffer + old_logfindex, MAX_LOGF_SIZE - old_logfindex);
        usb_serial_send(logfbuffer, logfindex - 1);
    }
    else
        usb_serial_send(logfbuffer + old_logfindex, logfindex - old_logfindex - 1);
    usb_serial_send("\r\n", 2);
#endif

    displayremote();
}
#endif

void logf_panic_dump(int *y)
{
    int i;
    /* nothing to print ? */
    if(logfindex == 0 && !logfwrap)
    {
        lcd_puts(1, (*y)++, "no logf data");
        lcd_update();
        return;
    }

    /* Explicitly null-terminate our buffer */
    logfbuffer[MAX_LOGF_SIZE] = 0;

    lcd_puts(1, (*y)++, "start of logf data");
    lcd_update();

    /* The intent is to dump the newest log entries first! */
    i = logfindex - 2; /* The last actual characer (i.e. not '\0') */
    while(i >= 0)
    {
        while(logfbuffer[i] != 0 && i>=0)
        {
            i--;
        }
        if(strlen( &logfbuffer[i + 1]) > 0)
        {
            lcd_puts(1, (*y)++, &logfbuffer[i + 1]);
            lcd_update();
        }
        i--;
    }
    if(logfwrap)
    {
        i = MAX_LOGF_SIZE - 1;
        while(i >= logfindex)
        {
            while(logfbuffer[i] != 0 && i >= logfindex)
            {
                i--;
            }
            if(strlen( &logfbuffer[i + 1]) > 0)
            {
                lcd_putsf(1, (*y)++, "%*s", (MAX_LOGF_SIZE-i), &logfbuffer[i + 1]);
                lcd_update();
            }
        }
        i--;
    }

    lcd_puts(1, (*y)++, "end of logf data");
    lcd_update();
}
#endif

#ifdef ROCKBOX_HAS_LOGDISKF
static int logdiskf_push(void *userp, int c)
{
    (void)userp;

    /*just stop logging if out of space*/
    if(logdiskfindex>=MAX_LOGDISKF_SIZE-1)
    {
        strcpy(&logdiskfbuffer[logdiskfindex-8], "LOGFULL");
        logdiskfindex=MAX_LOGDISKF_SIZE;
        return 0;
    }
    logdiskfbuffer[logdiskfindex++] = c;

    return 1;
}

static void flush_buffer(void);

void _logdiskf(const char* file, const char level, const char *fmt, ...)
{

    va_list ap;

    va_start(ap, fmt);
    int len =strlen(file);
    if(logdiskfindex +len + 4 > MAX_LOGDISKF_SIZE-1)
    {
        strcpy(&logdiskfbuffer[logdiskfindex-8], "LOGFULL");
        logdiskfindex=MAX_LOGDISKF_SIZE;
        va_end(ap);
        return;
    }

    logdiskf_push(NULL, level);
    logdiskf_push(NULL, ' ');
    logdiskf_push(NULL, '[');
    strcpy(&logdiskfbuffer[logdiskfindex], file);
    logdiskfindex += len;
    logdiskf_push(NULL, ']');

    vuprintf(logdiskf_push, NULL, fmt, ap);
    va_end(ap);
    register_storage_idle_func(flush_buffer);
}

static void flush_buffer(void)
{
    int fd;
    if(logdiskfindex < 1)
        return;

    fd = open(HOME_DIR"/rockbox_log.txt", O_RDWR | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    write(fd, logdiskfbuffer, logdiskfindex);
    close(fd);

    logdiskfindex = 0;
}

#endif
