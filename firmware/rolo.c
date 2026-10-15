/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 Randy D. Wood
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
#include "lcd.h"
#include "scroll_engine.h"
#include "thread.h"
#include "kernel.h"
#include "button.h"
#include "file.h"
#include "system.h"
#include "i2c.h"
#include "adc.h"
#include "string.h"
#include "core_alloc.h"
#include "storage.h"
#include "rolo.h"

#include "loader_strerror.h"
#include "rb-loader.h"
#define LOAD_FIRMWARE(a,b,c) load_firmware(a,b,c)

#if !defined(SIMULATOR)
#include "multiboot.h"
#include "bootdata.h"
#include "crc32.h"
#endif

#if !defined(SIMULATOR)
#include "devicedata.h"
#endif

#define IRQ0_EDGE_TRIGGER 0x80

static int rolo_handle;

static void rolo_error(const char *text)
{
    rolo_handle = core_free(rolo_handle);
    lcd_clear_display();
    lcd_puts(0, 0, "ROLO error:");
    lcd_puts_scroll(0, 1, text);
    lcd_update();
    button_get(true);
    button_get(true);
    button_get(true);
    lcd_scroll_stop();
}

#if CONFIG_CPU == IMX31L || CONFIG_CPU == RK27XX || CONFIG_CPU == X1000
/* this is in firmware/target/arm/imx31/rolo_restart.c for IMX31 */
/* this is in firmware/target/arm/rk27xx/rolo_restart.c for rk27xx */
/* this is in firmware/target/mips/ingenic_x1000/boot-x1000.c for X1000 */
extern void rolo_restart(const unsigned char* source, unsigned char* dest,
                         int length);
#else

/* explicitly put this code in iram, ICODE_ATTR is defined to be null for some
   targets that are low on iram, like the gigabeat F/X */
void rolo_restart(const unsigned char* source, unsigned char* dest,
                  long length) __attribute__ ((section(".icode")));
void rolo_restart(const unsigned char* source, unsigned char* dest,
                  long length)
{
    long i;
    unsigned char* localdest = dest;

    /* This is the equivalent of a call to memcpy() but this must be done from
       iram to avoid overwriting itself and we don't want to depend on memcpy()
       always being in iram */
    for(i = 0;i < length;i++)
        *localdest++ = *source++;

#if   defined(CPU_MIPS)
    commit_discard_idcache();
    asm volatile(
        "jr     %0               \n"
        "nop\n"
        : : "r"(dest)
    );
#endif
}
#endif

/* This is assigned in the linker control file */
extern unsigned long loadaddress;

/***************************************************************************
 *
 * Name: rolo_load(const char *filename)
 * Filename must be a fully defined filename including the path and extension
 *
 * Must call audio_hard_stop() before calling us!
 ***************************************************************************/
int rolo_load(const char* filename)
{
    unsigned char* ramstart = (void*)&loadaddress;
    unsigned char* filebuf;
    size_t filebuf_size;
    int err, length;

    lcd_clear_display();
    lcd_puts(0, 0, "ROLO");
    lcd_puts(0, 1, "Loading...");
    lcd_update();

    /* get the system buffer. release only in case of error, otherwise
     * we don't return anyway */
    rolo_handle = core_alloc_maximum(&filebuf_size, &buflib_ops_locked);
    if (rolo_handle < 0)
    {
        rolo_error("OOM");
        return -1;
    }

    filebuf = core_get_data(rolo_handle);

    err = LOAD_FIRMWARE(filebuf, filename, filebuf_size);
#if !defined(SIMULATOR)
    // Write bootdata as long as the existing bootdata is valid
    if (boot_data_valid)
    {
        int volume = 0;

        if (boot_data.version == 0)
            volume = boot_data._boot_volume;
        else if (boot_data.version == 1)
        {
            for (int i = 0; i < NUM_VOLUMES; ++i)
            {
                if (volume_drive(i) == boot_data.boot_drive &&
                    volume_partition(i) == boot_data.boot_partition)
                {
                    volume = i;
                    break;
                }
            }
        }

        write_bootdata(filebuf, filebuf_size, volume);
    }
#endif

    write_devicedata(filebuf, filebuf_size);

    if (err <= 0)
    {
        rolo_error(loader_strerror(err));
        return -1;
    }
    else
        length = err;

#ifdef HAVE_STORAGE_FLUSH
    lcd_puts(0, 1, "Flushing storage buffers...");
    lcd_update();
    storage_flush();
    lcd_clear_display();
    lcd_puts(0, 0, "ROLO");
    lcd_update();
#endif

    lcd_puts(0, 1, "Executing...");
    lcd_update();

    adc_close();

    /* Some targets have a higher disable level than HIGEST_IRQ_LEVEL */
    set_irq_level(DISABLE_INTERRUPTS);

    rolo_restart(filebuf, ramstart, length);

    /* never reached */
    return 0;
}
