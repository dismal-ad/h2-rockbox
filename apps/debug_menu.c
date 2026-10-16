/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 Heikki Hannikainen
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
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string-extra.h>
#include "lcd.h"
#include "lang.h"
#include "menu.h"
#include "debug_menu.h"
#include "kernel.h"
#include "action.h"
#include "debug.h"
#include "thread.h"
#include "powermgmt.h"
#include "system.h"
#include "font.h"
#include "audio.h"
#include "settings.h"
#include "list.h"
#include "dir.h"
#include "panic.h"
#include "screens.h"
#include "misc.h"
#include "splash.h"
#include "shortcuts.h"
#include "dircache.h"
#include "viewport.h"
#include "tagcache.h"
#include "crc32.h"
#include "logf.h"
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#include "disk.h"
#include "adc.h"
#include "usb.h"
#include "rtc.h"
#include "storage.h"
#include "fs_defines.h"
#include "eeprom_24cxx.h"
#if (CONFIG_STORAGE & STORAGE_MMC) || (CONFIG_STORAGE & STORAGE_SD)
#include "sdmmc.h"
#endif
#endif /* CONFIG_PLATFORM & PLATFORM_NATIVE */
#include "power.h"

#include "scrollbar.h"
#include "peakmeter.h"
#include "logfdisp.h"
#include "core_alloc.h"
#include "pcmbuf.h"
#include "buffering.h"
#include "playback.h"
#include "hwcompat.h"
#include "button.h"
#include "appevents.h"

#ifdef HAVE_USBSTACK
#include "usb_core.h"
#ifdef USB_ENABLE_AUDIO
#include "../usbstack/usb_audio.h"
#endif
#endif

#include "talk.h"

#include "devicedata.h"

#if !defined(SIMULATOR)
#include "bootdata.h"
#include "multiboot.h"
#include "rbpaths.h"
#include "pathfuncs.h"
#include "rb-loader.h"
#endif

#if defined(HAVE_BLUETOOTH)
#include "bluetooth/bluetooth.h"
#endif
#include "newui/ui.h"

#define SCREEN_MAX_CHARS (LCD_WIDTH / SYSFONT_WIDTH)

static const char* threads_getname(int selected_item, void *data,
                                   char *buffer, size_t buffer_len)
{
    int *x_offset = (int*) data;

#if NUM_CORES > 1
    if (selected_item < (int)NUM_CORES)
    {
        struct core_debug_info coreinfo;
        core_get_debug_info(selected_item, &coreinfo);
        snprintf(buffer, buffer_len, "Idle (%2d): %2d%%", selected_item,
                 coreinfo.idle_stack_usage);
        return buffer;
    }

    selected_item -= NUM_CORES;
#endif

    const char *fmtstr = "%2d: ---";

    struct thread_debug_info threadinfo;
    if (thread_get_debug_info(selected_item, &threadinfo) > 0)
    {
        fmtstr = "%2d:" IF_COP(" (%d)") " %s%n" IF_PRIO(" %2d %2d")
                 IFN_SDL(" %2d%% %2d%%") " %s";
    }
    int status_len;
    size_t len = snprintf(buffer, buffer_len, fmtstr,
             selected_item,
             IF_COP(threadinfo.core,)
             threadinfo.statusstr,
             &status_len,
             IF_PRIO(threadinfo.base_priority, threadinfo.current_priority,)
             IFN_SDL(threadinfo.stack_usage_cur, threadinfo.stack_usage,)
             threadinfo.name);

    int start = 0;
    (void) x_offset;
    (void) len;
    return &buffer[start];
}

static int dbg_threads_action_callback(int action, struct gui_synclist *lists)
{

    if (action == ACTION_NONE)
    {
        return ACTION_REDRAW;
    }
    (void) lists;
    return action;
}
/* Test code!!! */
static bool dbg_os(void)
{
    struct simplelist_info info;
    int xoffset = 0;

    simplelist_info_init(&info, IF_COP("Core and ") "Stack usage:",
                         MAXTHREADS IF_COP( + NUM_CORES ), &xoffset);
    info.scroll_all = false;
    info.action_callback = dbg_threads_action_callback;
    info.get_name = threads_getname;
    return simplelist_show_list(&info);
}

#ifdef __linux__
#include "cpuinfo-linux.h"

#define MAX_STATES 16
static struct time_state states[MAX_STATES];

static const char* get_cpuinfo(int selected_item, void *data,
                                   char *buffer, size_t buffer_len)
{
    (void)data;(void)buffer_len;
    const char* text;
    long time, diff;
    struct cpuusage us;
    static struct cpuusage last_us;
    int state_count = *(int*)data;

    if (cpuusage_linux(&us) != 0)
        return NULL;

    switch(selected_item)
    {
        case 0:
            diff = abs(last_us.usage - us.usage);
            sprintf(buffer, "Usage: %ld.%02ld%% (%c %ld.%02ld)",
                                    us.usage/100, us.usage%100,
                                    (us.usage >= last_us.usage) ? '+':'-',
                                    diff/100, diff%100);
            last_us.usage = us.usage;
            return buffer;
        case 1:
            text = "User";
            time = us.utime;
            diff = us.utime - last_us.utime;
            last_us.utime = us.utime;
            break;
        case 2:
            text = "Sys";
            time = us.stime;
            diff = us.stime - last_us.stime;
            last_us.stime = us.stime;
            break;
        case 3:
            text = "Real";
            time = us.rtime;
            diff = us.rtime - last_us.rtime;
            last_us.rtime = us.rtime;
            break;
        case 4:
            return "*** Per CPU freq stats ***";
        default:
        {
            int cpu = (selected_item - 5) / (state_count + 1);
            int cpu_line = (selected_item - 5) % (state_count + 1);

            /* scaling info */
            int min_freq = min_scaling_frequency(cpu);
            int cur_freq = current_scaling_frequency(cpu);
            /* fallback if scaling frequency is not available */
            if(cur_freq <= 0)
                cur_freq = frequency_linux(cpu);
            int max_freq = max_scaling_frequency(cpu);
            char governor[20];
            bool have_governor = current_scaling_governor(cpu, governor, sizeof(governor));
            if(cpu_line == 0)
            {
                sprintf(buffer,
                        " CPU%d: %s: %d/%d/%d MHz",
                        cpu,
                        have_governor ? governor : "Min/Cur/Max freq",
                        min_freq > 0 ? min_freq/1000 : -1,
                        cur_freq > 0 ? cur_freq/1000 : -1,
                        max_freq > 0 ? max_freq/1000 : -1);
            }
            else
            {
                cpustatetimes_linux(cpu, states, ARRAYLEN(states));
                snprintf(buffer, buffer_len, "   %ld %ld",
                            states[cpu_line-1].frequency,
                            states[cpu_line-1].time);
            }
            return buffer;
        }
    }
    sprintf(buffer, "%s: %ld.%02lds (+ %ld.%02ld)", text,
                    time / us.hz, time % us.hz,
                    diff / us.hz, diff % us.hz);
    return buffer;
}

static int cpuinfo_cb(int action, struct gui_synclist *lists)
{
    (void)lists;
    if (action == ACTION_NONE)
        action = ACTION_REDRAW;
    return action;
}

static bool dbg_cpuinfo(void)
{
    struct simplelist_info info;
    int cpu_count = MAX(cpucount_linux(), 1);
    int state_count = cpustatetimes_linux(0, states, ARRAYLEN(states));
    printf("%s(): %d %d\n", __func__, cpu_count, state_count);
    simplelist_info_init(&info, "CPU info:", 5 + cpu_count*(state_count+1), &state_count);
    info.get_name = get_cpuinfo;
    info.action_callback = cpuinfo_cb;
    info.timeout = HZ;
    info.scroll_all = true;
    return simplelist_show_list(&info);
}

#endif

static unsigned int ticks, freq_sum;
#ifndef CPU_MULTI_FREQUENCY
static unsigned int boost_ticks;
#endif

static void dbg_audio_task(void)
{
#ifdef CPUFREQ_NORMAL
#ifndef CPU_MULTI_FREQUENCY
    if(FREQ > CPUFREQ_NORMAL)
        boost_ticks++;
#endif
    freq_sum += FREQ/1000000; /* in MHz */
#endif
    ticks++;
}

static bool dbg_buffering_thread(void)
{
    int button;
    int line;
    bool done = false;
    size_t bufused;
    size_t bufsize = pcmbuf_get_bufsize();
    int pcmbufdescs = pcmbuf_descs();
    struct buffering_debug d;
    size_t filebuflen = audio_get_filebuflen();
    /* This is a size_t, but call it a long so it puts a - when it's bad. */
    #define STR_DATAREM "data_rem"
    const char * const fmt_used = "%s: %6ld/%ld";

#ifndef CPU_MULTI_FREQUENCY
    boost_ticks = 0;
#endif
    ticks = freq_sum = 0;

    tick_add_task(dbg_audio_task);

    FOR_NB_SCREENS(i)
        screens[i].setfont(FONT_SYSFIXED);

    while(!done)
    {
        button = get_action(CONTEXT_STD,HZ/5);
        switch(button)
        {
            case ACTION_STD_NEXT:
                audio_next();
                break;
            case ACTION_STD_PREV:
                audio_prev();
                break;
            case ACTION_STD_CANCEL:
                done = true;
                break;
        }

        buffering_get_debugdata(&d);
        bufused = bufsize - pcmbuf_free();

        FOR_NB_SCREENS(i)
        {
            line = 0;
            screens[i].clear_display();


            screens[i].putsf(0, line++, fmt_used, "pcm", (long) bufused, (long) bufsize);

            gui_scrollbar_draw(&screens[i],0, line*SYSFONT_HEIGHT, screens[i].lcdwidth, 6,
                               bufsize, 0, bufused, HORIZONTAL);
            line++;

            screens[i].putsf(0, line++, fmt_used, "alloc", audio_filebufused(),
                            (long) filebuflen);

            if (screens[i].lcdheight > 80)
            {
                gui_scrollbar_draw(&screens[i],0, line*SYSFONT_HEIGHT, screens[i].lcdwidth, 6,
                                   filebuflen, 0, audio_filebufused(), HORIZONTAL);
                line++;

                screens[i].putsf(0, line++, fmt_used, "real", (long)d.buffered_data,
                                (long)filebuflen);

                gui_scrollbar_draw(&screens[i],0, line*SYSFONT_HEIGHT, screens[i].lcdwidth, 6,
                                   filebuflen, 0, (long)d.buffered_data, HORIZONTAL);
                line++;
            }

            screens[i].putsf(0, line++, fmt_used, "usefl", (long)(d.useful_data),
                                                       (long)filebuflen);

            if (screens[i].lcdheight > 80)
            {
                gui_scrollbar_draw(&screens[i],0, line*SYSFONT_HEIGHT, screens[i].lcdwidth, 6,
                                   filebuflen, 0, d.useful_data, HORIZONTAL);
                line++;
            }

            screens[i].putsf(0, line++, "%s: %ld", STR_DATAREM, (long)d.data_rem);

            screens[i].putsf(0, line++, "track count: %2u", audio_track_count());

            screens[i].putsf(0, line++, "handle count: %d", (int)d.num_handles);

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
            screens[i].putsf(0, line++, "cpu freq: %3dMHz",
                             (int)((FREQ + 500000) / 1000000));
#endif

            if (ticks > 0)
            {
                int avgclock   = freq_sum * 10 / ticks;      /* in 100 kHz */
#ifdef CPU_MULTI_FREQUENCY
                int boostquota = (avgclock * 100 - CPUFREQ_NORMAL/1000) /
                    ((CPUFREQ_MAX - CPUFREQ_NORMAL) / 1000000); /* in 0.1 % */
#else
                int boostquota = boost_ticks * 1000 / ticks; /* in 0.1 % */
#endif
                screens[i].putsf(0, line++, "boost:%3d.%d%% (%d.%dMHz)",
                                 boostquota/10, boostquota%10, avgclock/10, avgclock%10);
            }

            screens[i].putsf(0, line++, "pcmbufdesc: %2d/%2d",
                             pcmbuf_used_descs(), pcmbufdescs);
            screens[i].putsf(0, line++, "watermark: %6d",
                             (int)(d.watermark));

            screens[i].update();
        }
    }

    tick_remove_task(dbg_audio_task);

    FOR_NB_SCREENS(i)
        screens[i].setfont(FONT_UI);

    return false;
#undef STR_DATAREM
}

#ifdef BUFLIB_DEBUG_PRINT
static const char* bf_getname(int selected_item, void *data,
                                   char *buffer, size_t buffer_len)
{
    (void)data;
    core_print_block_at(selected_item, buffer, buffer_len);
    return buffer;
}

static int bf_action_cb(int action, struct gui_synclist* list)
{
    if (action == ACTION_STD_OK)
    {
        if (gui_synclist_get_sel_pos(list) == 0 && core_test_free())
        {
            splash(HZ, "Freed test handle. New alloc should trigger compact");
        }
        else
        {
            splash(HZ/1, "Attempting a 64k allocation");
            int handle = core_alloc(64<<10);
            splash(HZ/2, (handle > 0) ? "Success":"Fail");
            /* for some reason simplelist doesn't allow adding items here if
             * info.get_name is given, so use normal list api */
            gui_synclist_set_nb_items(list, core_get_num_blocks());
            core_free(handle);
        }
        action = ACTION_REDRAW;
    }
    return action;
}

static bool dbg_buflib_allocs(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, "mem allocs", core_get_num_blocks(), NULL);
    info.get_name = bf_getname;
    info.action_callback = bf_action_cb;
    info.timeout = HZ;
    return simplelist_show_list(&info);
}
#endif /* BUFLIB_DEBUG_PRINT */

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
static const char* dbg_partitions_getname(int selected_item, void *data,
                                          char *buffer, size_t buffer_len)
{
    (void)data;
    int partition = selected_item/2;

    struct partinfo p;
    if (!disk_partinfo(partition, &p))
        return buffer;

    // XXX fix this up to use logical sector size
    // XXX and if mounted, show free info...
    if (selected_item%2)
    {
        snprintf(buffer, buffer_len, "   T:%x %lu MB", p.type,
                 (unsigned long)(p.size / ( 2048 / ( SECTOR_SIZE / 512 ))));
    }
    else
    {
        snprintf(buffer, buffer_len, "P%d: S:%llx", partition, (unsigned long long)p.start);
    }
    return buffer;
}

static bool dbg_partitions(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, "Partition Info", NUM_DRIVES * MAX_PARTITIONS_PER_DRIVE, NULL);
    info.selection_size = 2;
    info.scroll_all = true;
    info.get_name = dbg_partitions_getname;
    return simplelist_show_list(&info);
}
#endif /* PLATFORM_NATIVE */

#if !defined(SIMULATOR)
/*
 * view_battery() shows a automatically scaled graph of the battery voltage
 * over time. Usable for estimating battery life / charging rate.
 * The power_history array is updated in power_thread of powermgmt.c.
 */

#define BAT_LAST_VAL  MIN(LCD_WIDTH, POWER_HISTORY_LEN)
#define BAT_TSPACE    20
#define BAT_YSPACE    (LCD_HEIGHT - BAT_TSPACE)


static bool view_battery(void)
{
    extern struct battery_tables_t device_battery_tables; /* powermgmt.c */
    unsigned short *power_history = device_battery_tables.history;
    int view = 0;
    int i, x, y, z, y1, y2, grid, graph;
    unsigned short maxv, minv;
    lcd_setfont(FONT_SYSFIXED);

    while(1)
    {
        lcd_clear_display();
        switch (view) {
            case 0: /* voltage history graph */
                /* Find maximum and minimum voltage for scaling */
                minv = power_history[0];
                maxv = minv + 1;
                for (i = 1; i < BAT_LAST_VAL && power_history[i]; i++) {
                    if (power_history[i] > maxv)
                        maxv = power_history[i];
                    if (power_history[i] < minv)
                        minv = power_history[i];
                }
                /* print header */
                /* adjust grid scale */
                if ((maxv - minv) > 50)
                    grid = 50;
                else
                    grid = 5;

                lcd_putsf(0, 0, "%s %d.%03dV", "Battery", power_history[0] / 1000,
                         power_history[0] % 1000);
                lcd_putsf(0, 1, "%d.%03d-%d.%03dV (%2dmV)",
                          minv / 1000, minv % 1000, maxv / 1000, maxv % 1000,
                          grid);

                i = 1;
                while ((y = (minv - (minv % grid)+i*grid)) < maxv)
                {
                    graph = ((y-minv)*BAT_YSPACE)/(maxv-minv);
                    graph = LCD_HEIGHT-1 - graph;

                    /* draw dotted horizontal grid line */
                    for (x=0; x<LCD_WIDTH;x=x+2)
                        lcd_drawpixel(x,graph);

                    i++;
                }

                x = 0;
                /* draw plot of power history
                 * skip empty entries
                 */
                for (i = BAT_LAST_VAL - 1; i > 0; i--)
                {
                    if (power_history[i] && power_history[i-1])
                    {
                        y1 = (power_history[i] - minv) * BAT_YSPACE /
                            (maxv - minv);
                        y1 = MIN(MAX(LCD_HEIGHT-1 - y1, BAT_TSPACE),
                                 LCD_HEIGHT-1);
                        y2 = (power_history[i-1] - minv) * BAT_YSPACE /
                            (maxv - minv);
                        y2 = MIN(MAX(LCD_HEIGHT-1 - y2, BAT_TSPACE),
                                 LCD_HEIGHT-1);

                        lcd_set_drawmode(DRMODE_SOLID);

                        /* make line thicker */
                        lcd_drawline(((x*LCD_WIDTH)/(BAT_LAST_VAL)),
                                     y1,
                                     (((x+1)*LCD_WIDTH)/(BAT_LAST_VAL)),
                                     y2);
                        lcd_drawline(((x*LCD_WIDTH)/(BAT_LAST_VAL))+1,
                                     y1+1,
                                     (((x+1)*LCD_WIDTH)/(BAT_LAST_VAL))+1,
                                     y2+1);
                        x++;
                    }
                }
                break;

            case 1: /* status: */
                lcd_putsf(0, 0, "Pwr status: %s",
                         charging_state() ? "charging" : "discharging");
                battery_read_info(&y, &z);
                if (y > 0)
                    lcd_putsf(0, 1, "%s: %d.%03d V (%d %%)", "Battery", y / 1000, y % 1000, z);
                else if (z > 0)
                    lcd_putsf(0, 1, "%s: %d %%", "Battery", z);
#ifdef ADC_EXT_POWER
                y = (adc_read(ADC_EXT_POWER) * EXT_SCALE_FACTOR) / 1000;
                lcd_putsf(0, 2, "%s: %d.%03d V", "External", y / 1000, y % 1000);
#endif
                lcd_putsf(0, 3, "Charger: %s",
                         charger_inserted() ? "present" : "absent");
#if defined(HAVE_USBSTACK) && defined(HAVE_USB_CHARGING_ENABLE)
                lcd_putsf(0, 4, "USB current limit: %d mA",
                          usb_charging_maxcurrent());
#endif /* HAVE_USB_CHARGING_ENABLE */
                break;
            case 2: /* voltage deltas: */
                lcd_puts(0, 0, "Voltage deltas:");
                for (i = 0; i < POWER_HISTORY_LEN-1; i++) {
                    y = power_history[i] - power_history[i+1];
                    lcd_putsf(0, i+1, "-%d min: %c%d.%03d V", i,
                             (y < 0) ? '-' : ' ', ((y < 0) ? y * -1 : y) / 1000,
                             ((y < 0) ? y * -1 : y ) % 1000);
                }
                break;

            case 3: /* remaining time estimation: */

                lcd_putsf(0, 5, "Last PwrHist: %d.%03dV",
                    power_history[0] / 1000,
                    power_history[0] % 1000);

                lcd_putsf(0, 6, "%s level: %d%%", "Battery", battery_level());

                int time_left = battery_time();
                if (time_left >= 0)
                    lcd_putsf(0, 7, "Est. remain: %d m", time_left);
                else
                    lcd_puts(0, 7, "Estimation n/a");

                break;
        }

        lcd_update();

        switch(get_action(CONTEXT_STD,HZ/2))
        {
            case ACTION_STD_PREV:
                if (view)
                    view--;
                break;

            case ACTION_STD_NEXT:
                if (view < 3)
                    view++;
                break;

            case ACTION_STD_CANCEL:
                lcd_setfont(FONT_UI);
                return false;
        }
    }
    lcd_setfont(FONT_UI);
    return false;
}

#endif /* (CONFIG_BATTERY_MEASURE != 0)  */

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#if (CONFIG_STORAGE & STORAGE_MMC) || (CONFIG_STORAGE & STORAGE_SD)

#if   (CONFIG_STORAGE & STORAGE_SD)
#define CARDTYPE "microSD"
#endif

static int disk_callback(int btn, struct gui_synclist *lists)
{
    tCardInfo *card;
    int *cardnum = (int*)lists->data;
    unsigned char card_name[6];
    unsigned char pbuf[32];
    /* Casting away const is safe; the buffer is defined as non-const. */
    char *title = (char *)lists->title;
    static const unsigned char i_vmin[] = { 0, 1, 5, 10, 25, 35, 60, 100 };
    static const unsigned char i_vmax[] = { 1, 5, 10, 25, 35, 45, 80, 200 };
    static const unsigned char * const kbit_units[] = { "kBit/s", "MBit/s", "GBit/s" };
    static const unsigned char * const nsec_units[] = { "ns", "µs", "ms" };

    if ((btn == ACTION_STD_OK) || (btn == SYS_FS_CHANGED) || (btn == ACTION_REDRAW))
    {

        simplelist_reset_lines();

        card = card_get_info(*cardnum);

        if (card->initialized > 0)
        {
            unsigned i;
            for (i=0; i<sizeof(card_name); i++)
            {
                card_name[i] = card_extract_bits(card->cid, (103-8*i), 8);
            }
            strmemccpy(card_name, card_name, sizeof(card_name));
            simplelist_addline(
                    "%s Rev %d.%d", card_name,
                    (int) card_extract_bits(card->cid, 63, 4),
                    (int) card_extract_bits(card->cid, 59, 4));
            simplelist_addline(
                    "Prod: %d/%d",
#if (CONFIG_STORAGE & STORAGE_SD)
                    (int) card_extract_bits(card->cid, 11, 4),
                    (int) card_extract_bits(card->cid, 19, 8) + 2000
#endif
                    );
            simplelist_addline(
#if (CONFIG_STORAGE & STORAGE_SD)
                    "Ser#: 0x%08lx",
                    card_extract_bits(card->cid, 55, 32)
#endif
                    );

            simplelist_addline("M=%02x, "
#if (CONFIG_STORAGE & STORAGE_SD)
                    "O=%c%c",
                    (int) card_extract_bits(card->cid, 127, 8),
                    (int) card_extract_bits(card->cid, 119, 8),
                    (int) card_extract_bits(card->cid, 111, 8)
#endif
                    );

            simplelist_addline(
                    "Blocks: 0x%08lx", card->numblocks);
            output_dyn_value(pbuf, sizeof pbuf, card->speed / 1000,
                                            kbit_units, 3, false);
            simplelist_addline(
                    "Speed: %s", pbuf);
            output_dyn_value(pbuf, sizeof pbuf, card->taac,
                            nsec_units, 3, false);
            simplelist_addline(
                    "Taac: %s", pbuf);
            simplelist_addline(
                    "Nsac: %d clk", card->nsac);
            simplelist_addline(
                    "R2W: *%d", 1 << card->r2w_factor);
#if (CONFIG_STORAGE & STORAGE_SD)
            int csd_structure = card_extract_bits(card->csd, 127, 2);
            const char *ver;
            switch(csd_structure) {
            case 0:
                    ver = "1 (SD)";
                    break;
            case 1:
                    ver = "2 (SDHC/SDXC)";
                    break;
            case 2:
                    ver = "3 (SDUC)";
                    break;
            default:
                    ver = "Unknown";
                    break;
            }
            simplelist_addline("SDVer: %s", ver);
            if (csd_structure == 0) /* CSD version 1.0 */
#endif
            {
            simplelist_addline(
                    "IRmax: %d..%d mA",
                    i_vmin[card_extract_bits(card->csd, 61, 3)],
                    i_vmax[card_extract_bits(card->csd, 58, 3)]);
            simplelist_addline(
                    "IWmax: %d..%d mA",
                    i_vmin[card_extract_bits(card->csd, 55, 3)],
                    i_vmax[card_extract_bits(card->csd, 52, 3)]);
            }
#if (CONFIG_STORAGE & STORAGE_SD)
            /*
             * Most drivers don't read SCR so an all zero value means
             * the driver didn't populate it. Valid SCRs will have at
             * least the SD_BUS_WIDTHS field containing a nonzero value.
             */
            if (card->scr[0] || card->scr[1])
            {
                int scr_vers = (card->scr[1] >> 28) & 0xF;
                simplelist_addline("SCR version: %d", scr_vers);

                if (scr_vers == 0)
                {
                    bool supports_cmd23 = card->scr[1] & 0x2;
                    simplelist_addline("CMD23 supported: %s", supports_cmd23 ? "yes" : "no");
                }
            }
#endif
        }
        else if (card->initialized == 0)
        {
            simplelist_setline("Not Found!");
        }
#if (CONFIG_STORAGE & STORAGE_SD)
        else /* card->initialized < 0 */
        {
            simplelist_addline("Init Error! (%d)", card->initialized);
        }
#endif
        snprintf(title, 16, "[" CARDTYPE " %d]", *cardnum);
        gui_synclist_set_title(lists, title, Icon_NOICON);
        gui_synclist_set_nb_items(lists, simplelist_get_line_count());
        gui_synclist_select_item(lists, 0);
        btn = ACTION_REDRAW;
    }
    return btn;
}
#else /* No SD, MMC or ATA */
static int disk_callback(int btn, struct gui_synclist *lists)
{
    (void)lists;
    struct storage_info info;
    storage_get_info(0,&info);
    simplelist_addline("Vendor: %s", info.vendor);
    simplelist_addline("Model: %s", info.product);
    simplelist_addline("Firmware: %s", info.revision);
    simplelist_addline(
            "Size: %lu MB", (unsigned long)(info.num_sectors*(info.sector_size/512)/2048));
    sector_t free;
    volume_size( IF_MV(0,) NULL, &free );
    simplelist_addline(
             "Free: %ld MB", free / 1024);
    simplelist_addline(
             "Cluster size: %d bytes", volume_get_cluster_size(IF_MV(0)));
    return btn;
}
#endif

static bool dbg_disk_info(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, "Disk Info", 1, NULL);
#if (CONFIG_STORAGE & STORAGE_MMC) || (CONFIG_STORAGE & STORAGE_SD)
    char title[16];
    int card = 0;
    info.callback_data = (void*)&card;
    info.title = title;
#endif
    info.action_callback = disk_callback;
    info.scroll_all = true;
    return simplelist_show_list(&info);
}
#endif /* PLATFORM_NATIVE */

#ifdef HAVE_DIRCACHE
static int dircache_callback(int btn, struct gui_synclist *lists)
{
    (void)lists;
    struct dircache_info info;
    dircache_get_info(&info);

    if (global_settings.dircache)
    {
        switch (btn)
        {
        case ACTION_STD_CONTEXT:
            splash(HZ/2, "Rebuilding cache");
            dircache_suspend();
            *(int *)lists->data = dircache_resume();
            /* Fallthrough */
        case ACTION_UNKNOWN:
            btn = ACTION_NONE;
            break;
    #ifdef DIRCACHE_DUMPSTER
        case ACTION_STD_OK:
            splash(0, "Dumping cache");
            dircache_dump();
            btn = ACTION_NONE;
            break;
    #endif /* DIRCACHE_DUMPSTER */
        case ACTION_STD_CANCEL:
            if (*(int *)lists->data > 0 && info.status == DIRCACHE_SCANNING)
            {
                splash(HZ, ID2P(LANG_SCANNING_DISK));
                btn = ACTION_NONE;
            }
            break;
        }
    }

    simplelist_reset_lines();

    simplelist_addline("Cache status: %s", info.statusdesc);
    simplelist_addline("Last size: %zu B", info.last_size);
    simplelist_addline("Size: %zu B", info.size);
    unsigned int utilized = info.size ? 1000ull*info.sizeused / info.size : 0;
    simplelist_addline("Used: %zu B (%u.%u%%)", info.sizeused,
                       utilized / 10, utilized % 10);
    simplelist_addline("Limit: %zu B", info.size_limit);
    simplelist_addline("Reserve: %zu/%zu B", info.reserve_used, info.reserve);
    long ticks = ALIGN_UP(info.build_ticks, HZ / 10);
    simplelist_addline("Scanning took: %ld.%ld s",
                       ticks / HZ, (ticks*10 / HZ) % 10);
    simplelist_addline("Entry count: %u", info.entry_count);

    return btn;
}

static bool dbg_dircache_info(void)
{
    struct simplelist_info info;
    int syncbuild = 0;
    simplelist_info_init(&info, "Dircache Info", 0, &syncbuild);
    info.action_callback = dircache_callback;
    info.scroll_all = true;
    return simplelist_show_list(&info);
}

#endif /* HAVE_DIRCACHE */

static int database_callback(int btn, struct gui_synclist *lists)
{
    (void)lists;
    struct tagcache_stat *stat = tagcache_get_stat();
    static bool synced = false;
    static int update_entries = 0;

    simplelist_reset_lines();

    simplelist_addline("Initialized: %s",
             stat->initialized ? "Yes" : "No");
    simplelist_addline("DB %s: %s", "Ready",
             stat->ready ? "Yes" : "No");
    simplelist_addline("DB Path: %s", stat->db_path);
    simplelist_addline("RAM Cache: %s",
             stat->ramcache ? "Yes" : "No");
    simplelist_addline("RAM: %d/%d B",
             stat->ramcache_used, stat->ramcache_allocated);
    simplelist_addline("Total entries: %d",
                       stat->total_entries);
    simplelist_setline("Progress:");
    simplelist_addline(" %d%% (%d entries)",
                       stat->progress, stat->processed_entries);
    simplelist_setline("Curfile:");
    simplelist_addline(" %s", stat->curentry ? stat->curentry : "---");
    simplelist_addline("Commit step: %d",
             stat->commit_step);
    simplelist_addline("Commit delayed: %s",
             stat->commit_delayed ? "Yes" : "No");

    simplelist_addline("Queue length: %d",
             stat->queue_length);

    if (synced)
    {
        synced = false;
        tagcache_screensync_event();
    }

    if (!btn && stat->curentry)
    {
        synced = true;
        if (update_entries <= stat->processed_entries)
        {
            update_entries = stat->processed_entries + 100;
            return ACTION_REDRAW;
        }
        return ACTION_NONE;
    }

    if (btn == ACTION_STD_CANCEL)
    {
        update_entries = 0;
        tagcache_screensync_enable(false);
    }
    return btn;
}
static bool dbg_tagcache_info(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, "Database Info", 0, NULL);
    info.action_callback = database_callback;
    info.scroll_all = true;

    /* Don't do nonblock here, must give enough processing time
       for tagcache thread. */
    /* info.timeout = TIMEOUT_NOBLOCK; */
    info.timeout = 1;
    tagcache_screensync_enable(true);
    return simplelist_show_list(&info);
}

#ifndef SIMULATOR
#endif /* !SIMULATOR */

extern bool do_screendump_instead_of_usb;

static bool dbg_screendump(void)
{
    do_screendump_instead_of_usb = !do_screendump_instead_of_usb;
    splashf(HZ, "Screendump %sabled", do_screendump_instead_of_usb?"en":"dis");
    return false;
}

extern bool write_metadata_log;

static bool dbg_metadatalog(void)
{
    write_metadata_log = !write_metadata_log;
    splashf(HZ, "Metadata log %sabled", write_metadata_log ? "en" : "dis");
    return false;
}

#ifdef CPU_BOOST_LOGGING
static bool cpu_boost_log(void)
{
    int count = cpu_boost_log_getcount();
    char *str = cpu_boost_log_getlog_first();
    bool done;
    lcd_setfont(FONT_SYSFIXED);
    for (int i = 0; i < count ;)
    {
        lcd_clear_display();
        for(int j=0; j<LCD_HEIGHT/SYSFONT_HEIGHT; j++,i++)
        {
            if (!str)
                str = cpu_boost_log_getlog_next();
            if (str)
            {
                if(strlen(str) > LCD_WIDTH/SYSFONT_WIDTH)
                    lcd_puts_scroll(0, j, str);
                else
                    lcd_puts(0, j,str);
            }
            str = NULL;
        }
        lcd_update();
        done = false;
        while (!done)
        {
            switch(get_action(CONTEXT_STD,TIMEOUT_BLOCK))
            {
                case ACTION_STD_OK:
                case ACTION_STD_PREV:
                case ACTION_STD_NEXT:
                    done = true;
                break;
                case ACTION_STD_CANCEL:
                    i = count;
                    done = true;
                break;
            }
        }
    }
    lcd_scroll_stop();
    get_action(CONTEXT_STD,TIMEOUT_BLOCK);
    lcd_setfont(FONT_UI);
    return false;
}

static bool cpu_boost_log_dump(void)
{
    int fd;
    int count = cpu_boost_log_getcount();
    char *str = cpu_boost_log_getlog_first();

    splashf(HZ, "Boost Log File Dumped");

    /* nothing to print ? */
    if(count == 0)
        return false;

    char fname[MAX_PATH];
    struct tm *nowtm = get_time();
    fd = open_pathfmt(fname, sizeof(fname), O_CREAT|O_WRONLY|O_TRUNC,
                      "%s/boostlog_%04d%02d%02d%02d%02d%02d.txt", ROCKBOX_DIR,
                      nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday,
                      nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec);
    if(-1 != fd) {
        for (int i = 0; i < count; i++)
        {
            if (!str)
                str = cpu_boost_log_getlog_next();
            if (str)
            {
               fdprintf(fd, "%s\n", str);
               str = NULL;
            }
        }

        close(fd);
        return true;
    }

    return false;
}
#endif

static bool dbg_talk(void)
{
    struct simplelist_info list;
    struct talk_debug_data data;
    talk_get_debug_data(&data);

    simplelist_info_init(&list, "Voice Information:", 0, NULL);

    list.scroll_all = true;
    list.timeout = HZ;

    simplelist_reset_lines();

    simplelist_setline("Current voice file:");
    if (data.status != TALK_STATUS_ERR_NOFILE)
        simplelist_addline(" %s", data.voicefile);
    else
        simplelist_setline(" No voice information available");

    if (data.status != TALK_STATUS_OK)
    {
        simplelist_addline("Talk Status: ERR (%i)",
                    data.status);
        return simplelist_show_list(&list);
    }
    else
        simplelist_setline("Talk Status: OK");
    simplelist_setline("Number of (empty) clips in voice file:");
    simplelist_addline(" (%d) %d", data.num_empty_clips, data.num_clips);
    simplelist_setline("Min/Avg/Max size of clips:");
    simplelist_addline(" %d / %d / %d",
                    data.min_clipsize, data.avg_clipsize, data.max_clipsize);
    simplelist_setline("Memory allocated:");
    simplelist_addline(" %ld.%02ld KB",
                    data.memory_allocated / 1024, data.memory_allocated % 1024);
    simplelist_addline("Memory used:");
    simplelist_addline(" %ld.%02ld KB",
                       data.memory_used / 1024, data.memory_used % 1024);
    simplelist_setline("Number of clips in cache:");
    simplelist_addline(" %d", data.cached_clips);
    simplelist_setline("Cache hits / misses:");
    simplelist_addline("%d / %d", data.cache_hits, data.cache_misses);

    return simplelist_show_list(&list);
}

#ifdef HAVE_USBSTACK
#if (defined(ROCKBOX_HAS_LOGF) && defined(USB_ENABLE_SERIAL))
#ifdef USB_ENABLE_SERIAL
static bool toggle_usb_serial(void)
{
    bool enabled = !usb_get_serial();

    usb_set_serial(enabled);
    splashf(HZ, "USB Serial %s", enabled ? "enabled" : "disabled");

    return false;
}
#endif /* USB_ENABLE_SERIAL */
#endif

#ifdef USB_ENABLE_AUDIO
static int dbg_usb_audio_cb(int action, struct gui_synclist *lists)
{
    (void)lists;
    simplelist_reset_lines();
    simplelist_addline("%sabled", usb_core_driver_enabled(USB_DRIVER_AUDIO)?"En":"Dis");
    simplelist_addline("%sPlaying", usb_audio_get_playing()?"":"Not ");
    simplelist_addline("iface: %d alt: %d", usb_audio_get_main_intf(), usb_audio_get_alt_intf());
    simplelist_addline("out ep: 0x%X in ep: 0x%X", usb_audio_get_out_ep(), usb_audio_get_in_ep());
    simplelist_addline("Volume: %d", usb_audio_get_cur_volume());
    simplelist_addline("Playback Frequency: %lu", usb_audio_get_playback_sampling_frequency());
    simplelist_addline("Frames dropped: %d", usb_audio_get_frames_dropped());
    simplelist_addline("Buffers filled: %f", (double)usb_audio_get_prebuffering_avg()/(1<<16)); // convert from 16.16 fixed to float
    simplelist_addline("Min: %d / Max: %d", usb_audio_get_prebuffering_maxmin(false), usb_audio_get_prebuffering_maxmin(true));
    simplelist_addline("Samples used per Frame: %f", (double)usb_audio_get_samplesperframe()/(1<<16)); // convert from 16.16 fixed to float
    simplelist_addline("Samples received per frame: %f", (double)usb_audio_get_samples_rx_perframe()/(1<<16)); // convert from 16.16 fixed to float
    simplelist_addline("Samples diff: %f", (double)(usb_audio_get_samplesperframe()-usb_audio_get_samples_rx_perframe())/(1<<16)); // convert from 16.16 fixed to float
    simplelist_addline("%s", usb_audio_get_underflow()?"UNDERFLOW!":" ");
    simplelist_addline("%s", usb_audio_get_overflow()?"OVERFLOW!":" ");
    simplelist_addline("%s", usb_audio_get_alloc_failed()?"ALLOC FAILED!":" ");
    if (action == ACTION_NONE)
    {
        action = ACTION_REDRAW;
    }
    return action;
}
static bool dbg_usb_audio(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, "USB Audio", 0, NULL);
    info.scroll_all = true;
    info.action_callback = dbg_usb_audio_cb;
    return simplelist_show_list(&info);
}
#endif /* USB_ENABLE_AUDIO */
#endif /* HAVE_USBSTACK */

#if !defined(SIMULATOR)
static bool dbg_boot_data(void)
{
    struct simplelist_info info;
    info.scroll_all = true;
    simplelist_info_init(&info, "Boot data", 0, NULL);
    simplelist_reset_lines();

    if (!boot_data_valid)
    {
        simplelist_setline("Boot data invalid");
        simplelist_addline("Magic[0]: %08lx", boot_data.magic[0]);
        simplelist_addline("Magic[1]: %08lx", boot_data.magic[1]);
        simplelist_addline("Length: %lu", boot_data.length);
    }
    else
    {
        simplelist_setline("Boot data valid");
        simplelist_addline("Version: %d", (int)boot_data.version);

        if (boot_data.version == 0)
        {
            simplelist_addline("Boot volume: %d", (int)boot_data._boot_volume);
        }
        else if (boot_data.version == 1)
        {
            simplelist_addline("Boot drive: %d", (int)boot_data.boot_drive);
            simplelist_addline("Boot partition: %d", (int)boot_data.boot_partition);
        }
        simplelist_setline("Boot path:");
        simplelist_addline(" %s%s/%s", root_realpath(), BOOTDIR, BOOTFILE);
    }

    simplelist_setline("Bootdata RAW:");
    for (size_t i = 0; i < boot_data.length; i += 4)
    {
        simplelist_addline("%02x: %02x %02x %02x %02x", i,
                           boot_data.payload[i + 0], boot_data.payload[i + 1],
                           boot_data.payload[i + 2], boot_data.payload[i + 3]);
    }

    return simplelist_show_list(&info);
}
#endif /* defined(HAVE_BOOTDATA) && !defined(SIMULATOR) */

static bool dbg_device_data(void)
{
    struct simplelist_info info;
    info.scroll_all = true;
    simplelist_info_init(&info, "Device data", 0, NULL);
    simplelist_reset_lines();

    simplelist_setline("Device data");

    simplelist_addline("Hardware Revision: %d", (int)device_data.hw_rev);
    simplelist_addline("Struct Ver: %d", (int)device_data.version);

    simplelist_setline("Device data RAW:");
    for (size_t i = 0; i < device_data.length; i += 4)
    {
        simplelist_addline("%02zx: %02x %02x %02x %02x", i,
                           device_data.payload[i + 0], device_data.payload[i + 1],
                           device_data.payload[i + 2], device_data.payload[i + 3]);
    }

    return simplelist_show_list(&info);
}


#ifdef FTL_SYS_ON_USB
/* The NAND's hidden SYS volume, shown to a USB host in place of USER for
 * one connection */
static bool dbg_nand_sys_usb(void)
{
    ftl_set_sys_on_usb(!ftl_get_sys_on_usb());
    splashf(HZ, "NAND SYS on next USB %sabled",
            ftl_get_sys_on_usb() ? "en" : "dis");
    return false;
}
#endif

/****** The menu *********/
static const struct {
    unsigned char *desc; /* string or ID */
    bool (*function) (void); /* return true if USB was connected */
} menuitems[] = {
        { "New UI gallery", ui_gallery },
        { "View OS stacks", dbg_os },
#ifdef __linux__
        { "View CPU stats", dbg_cpuinfo },
#endif
#if !defined(SIMULATOR)
        { "View battery", view_battery },
#endif
        { "Screendump", dbg_screendump },
#if ((CONFIG_PLATFORM & PLATFORM_NATIVE) || defined(SONY_NWZ_LINUX)) && !defined(SIMULATOR)
        { "View HW info", dbg_hw_info },
#endif
#ifdef FTL_SYS_ON_USB
        { "NAND SYS on next USB", dbg_nand_sys_usb },
#endif
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
        { "View partitions", dbg_partitions },
#endif
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
        { "View disk info", dbg_disk_info },
#endif
        { "Metadata log", dbg_metadatalog },
#ifdef HAVE_DIRCACHE
        { "View dircache info", dbg_dircache_info },
#endif
        { "View database info", dbg_tagcache_info },
        { "View buffering thread", dbg_buffering_thread },
#ifdef PM_DEBUG
        { "pm histogram", peak_meter_histogram},
#endif /* PM_DEBUG */
#ifdef BUFLIB_DEBUG_PRINT
        { "View buflib allocs", dbg_buflib_allocs },
#endif
#ifndef SIMULATOR
#endif
#ifdef ROCKBOX_HAS_LOGF
        {"Show Log File", logfdisplay },
        {"Dump Log File", logfdump },
#endif
#if defined(HAVE_USBSTACK)
#if defined(ROCKBOX_HAS_LOGF) && defined(USB_ENABLE_SERIAL)
        {"USB Serial driver (logf)", toggle_usb_serial },
#endif
#if defined(USB_ENABLE_AUDIO)
        {"USB-DAC", dbg_usb_audio},
#endif
#endif /* HAVE_USBSTACK */
#ifdef CPU_BOOST_LOGGING
        {"Show cpu_boost log",cpu_boost_log},
        {"Dump cpu_boost log",cpu_boost_log_dump},
#endif
        {"Talk engine stats", dbg_talk },
#if !defined(SIMULATOR)
        {"Boot data", dbg_boot_data },
#endif

        {"Device data", dbg_device_data },
#ifdef HAVE_BLUETOOTH
        { "Bluetooth heap info", bt_heap_info },
#endif

};

static int menu_action_callback(int btn, struct gui_synclist *lists)
{
    int selection = gui_synclist_get_sel_pos(lists);
    if (btn == ACTION_STD_OK)
    {
        FOR_NB_SCREENS(i)
           viewportmanager_theme_enable(i, false, NULL);
        menuitems[selection].function();
        btn = ACTION_REDRAW;
        FOR_NB_SCREENS(i)
            viewportmanager_theme_undo(i, false);
    }
    else if (btn == ACTION_STD_CONTEXT)
    {
        MENUITEM_STRINGLIST(menu_items, "Debug", NULL, ID2P(LANG_ADD_TO_FAVES));
        if (do_menu(&menu_items, NULL, NULL, false) == 0)
            shortcuts_add(SHORTCUT_DEBUGITEM, menuitems[selection].desc);
        return ACTION_STD_CANCEL;
    }
    return btn;
}

static const char* menu_get_name(int item, void * data,
                                    char *buffer, size_t buffer_len)
{
    (void)data; (void)buffer; (void)buffer_len;
    return menuitems[item].desc;
}

static int menu_get_talk(int item, void *data)
{
    (void)data;
    if (global_settings.talk_menu && menuitems[item].desc)
    {
        talk_number(item + 1, true);
        talk_id(VOICE_PAUSE, true);
        talk_spell(menuitems[item].desc, true);
     }
    return 0;
}

int debug_menu(void)
{
    struct simplelist_info info;

    simplelist_info_init(&info, "Debug", ARRAYLEN(menuitems), NULL);
    info.action_callback = menu_action_callback;
    info.get_name        = menu_get_name;
    info.get_talk        = menu_get_talk;
    return (simplelist_show_list(&info)) ? 1 : 0;
}

bool run_debug_screen(char* screen)
{
    for (unsigned i=0; i<ARRAYLEN(menuitems); i++)
        if (!strcasecmp(screen, menuitems[i].desc))
        {
            FOR_NB_SCREENS(j)
               viewportmanager_theme_enable(j, false, NULL);
            menuitems[i].function();
            FOR_NB_SCREENS(j)
                viewportmanager_theme_undo(j, false);
            return true;
        }

    return false;
}
