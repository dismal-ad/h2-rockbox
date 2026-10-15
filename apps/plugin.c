/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 Björn Stenberg
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
#define DIRFUNCTIONS_DEFINED
#define FILEFUNCTIONS_DEFINED
#include "plugin.h"
#include "open_plugin.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "i2c.h"
#include "lang.h"
#include "led.h"
#include "keyboard.h"
#include "backlight.h"
#include "sound_menu.h"
#include "mp3data.h"
#include "powermgmt.h"
#include "splash.h"
#include "logf.h"
#include "option_select.h"
#include "talk.h"
#include "version.h"
#include "storage.h"
#include "pcmbuf.h"
#include "errno.h"
#include "diacritic.h"
#include "pathfuncs.h"
#include "load_code.h"
#include "file.h"
#include "core_keymap.h"
#include "language.h"
#include "statusbar-skinned.h"
#include "panic.h"

#include "power.h"

#include "scrollbar.h"
#include "peakmeter.h"
#include "bmp.h"
#include "bidi.h"

#ifdef USB_ENABLE_HID
#include "usbstack/usb_hid.h"
#endif

#ifdef USB_ENABLE_AUDIO
#include "usbstack/usb_audio.h"
#endif

#define WRAPPER(_x_) _x_ ## _wrapper

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)
static unsigned char pluginbuf[PLUGIN_BUFFER_SIZE];
void sim_lcd_ex_init(unsigned long (*getpixel)(int, int));
void sim_lcd_ex_update_rect(int x, int y, int width, int height);
#else
extern unsigned char pluginbuf[];
#include "bitswap.h"
#endif

/* for actual plugins only, not for codecs */
static int  plugin_size = 0;
static int (*pfn_tsr_exit)(bool reenter) = NULL; /* TSR exit callback */
static char current_plugin[MAX_PATH];
/* NULL if no plugin is loaded, otherwise the handle that lc_open() returned */
static void *current_plugin_handle;

char *plugin_get_current_filename(void);

static void* plugin_get_audio_buffer(size_t *buffer_size);
static void plugin_release_audio_buffer(void);
static void plugin_tsr(int (*exit_callback)(bool));

extern struct battery_tables_t device_battery_tables; /* powermgmt.c */

#ifdef HAVE_PLUGIN_CHECK_OPEN_CLOSE
/* File handle leak prophylaxis */
#include "bitarray.h"
#include "file_internal.h" /* for MAX_OPEN_FILES */

#define PCOC_WRAPPER(_x_)   WRAPPER(_x_)

BITARRAY_TYPE_DECLARE(plugin_check_open_close_bitmap_t, open_files_bitmap,
                      MAX_OPEN_FILES)

static plugin_check_open_close_bitmap_t open_files_bitmap;

static void plugin_check_open_close__enter(void)
{
    if (!current_plugin_handle)
        open_files_bitmap_clear(&open_files_bitmap);
}

static void plugin_check_open_close__open(int fildes)
{
    if (fildes >= 0)
        open_files_bitmap_set_bit(&open_files_bitmap, fildes);
}

static void plugin_check_open_close__close(int fildes)
{
    if (fildes < 0)
        return;

    if (!open_files_bitmap_test_bit(&open_files_bitmap, fildes))
    {
        logf("double close from plugin");
    }

    open_files_bitmap_clear_bit(&open_files_bitmap, fildes);
}

static int WRAPPER(open)(const char *path, int oflag, ...)
{
    int fildes = FS_PREFIX(open)(path, oflag __OPEN_MODE_ARG);
    plugin_check_open_close__open(fildes);
    return fildes;
}

static int WRAPPER(creat)(const char *path, mode_t mode)
{
    int fildes = FS_PREFIX(creat)(path __CREAT_MODE_ARG);
    plugin_check_open_close__open(fildes);
    return fildes;
    (void)mode;
}

static int WRAPPER(close)(int fildes)
{
    int rc = FS_PREFIX(close)(fildes);
    if (rc >= 0)
        plugin_check_open_close__close(fildes);

    return rc;
}

static void plugin_check_open_close__exit(void)
{
    if (current_plugin_handle)
        return;

    if (open_files_bitmap_is_clear(&open_files_bitmap))
        return;

    logf("Plugin '%s' leaks file handles", current_plugin);

    static const char *lines[] =
        { ID2P(LANG_PLUGIN_ERROR), "#leak-file-handles" };
    static const struct text_message message = { lines, 2 };
    button_clear_queue(); /* Empty the keyboard buffer */
    gui_syncyesno_run(&message, NULL, NULL);

    FOR_EACH_BITARRAY_SET_BIT(&open_files_bitmap, fildes)
        WRAPPER(close)(fildes);
}

#else /* !HAVE_PLUGIN_CHECK_OPEN_CLOSE */

#define PCOC_WRAPPER(_x_) FS_PREFIX(_x_)
#define plugin_check_open_close__enter()
#define plugin_check_open_close__exit()

#endif /* HAVE_PLUGIN_CHECK_OPEN_CLOSE */

static const struct plugin_api rockbox_api = {
    rbversion,
    &global_settings,
    &global_status,
    language_strings,
    &core_bitmaps[0],
    /* lcd */
    splashf,
    splash_progress,
    splash_progress_set_delay,
    lcd_update,
    lcd_clear_display,
    lcd_getstringsize,
    lcd_putsxy,
    lcd_putsxyf,
    lcd_puts,
    lcd_putsf,
    lcd_puts_scroll,
    lcd_scroll_stop,
    lcd_set_viewport,
    lcd_bmp_part,
    lcd_update_rect,
    lcd_set_drawmode,
    lcd_get_drawmode,
    screen_helper_setfont,
    lcd_drawpixel,
    lcd_drawline,
    lcd_hline,
    lcd_vline,
    lcd_drawrect,
    lcd_fillrect,
    lcd_mono_bitmap_part,
    lcd_mono_bitmap,
    lcd_set_foreground,
    lcd_get_foreground,
    lcd_set_background,
    lcd_get_background,
    lcd_bitmap_part,
    lcd_bitmap,
    lcd_get_backdrop,
    lcd_set_backdrop,
    lcd_bitmap_transparent_part,
    lcd_bitmap_transparent,
#if MEMORYSIZE > 2
    lcd_blit_yuv,
#endif /* MEMORYSIZE > 2 */
    button_queue_post,
    bidi_l2v,
    is_diacritic,
    font_get_bits,
    font_load,
    font_unload,
    font_get,
    font_measurestring,
    font_getstringsize,
    font_get_width,
    screen_clear_area,
    gui_scrollbar_draw,
    get_codepage_name,

#if NB_SCREENS == 2
    {&screens[SCREEN_MAIN], &screens[SCREEN_REMOTE]},
#else
    {&screens[SCREEN_MAIN]},
#endif

    viewport_set_defaults,
    viewportmanager_theme_enable,
    viewportmanager_theme_undo,
    viewport_set_fullscreen,
    viewport_set_buffer,

    /* lcd backlight */
    /* For OLED targets like the Sansa Clip, the backlight_* functions control
     * the display enable, which has essentially the same effect. */
    is_backlight_on,
    backlight_on,
    backlight_off,
    backlight_set_on_button_hold,
    backlight_set_timeout,
    backlight_set_brightness,

    backlight_set_timeout_plugged,

    /* list */
    gui_synclist_init,
    gui_synclist_set_nb_items,
    gui_synclist_set_voice_callback,
    gui_synclist_set_icon_callback,
    gui_synclist_get_nb_items,
    gui_synclist_get_sel_pos,
    gui_synclist_draw,
    gui_synclist_speak_item,
    gui_synclist_select_item,
    gui_synclist_add_item,
    gui_synclist_del_item,
    gui_synclist_do_button,
    gui_synclist_set_title,
    gui_synclist_scroll_stop,
    gui_syncyesno_run,
    simplelist_info_init,
    simplelist_show_list,
    yesno_pop,
    yesno_pop_confirm,

    /* status bar */
    sb_set_title_text,
    sb_set_persistent_title,

    /* action handling */
    get_custom_action,
    get_action,
    action_userabort,
    core_set_keyremap,

    /* button */
    button_get,
    button_get_w_tmo,
    button_status,
    button_clear_queue,
    button_queue_count,
    button_set_sw_poweroff_state,
    button_get_sw_poweroff_state,

    /* file */
    open_utf8,
    PCOC_WRAPPER(open),
    PCOC_WRAPPER(creat),
    PCOC_WRAPPER(close),
    FS_PREFIX(read),
    FS_PREFIX(lseek),
    FS_PREFIX(write),
    FS_PREFIX(remove),
    FS_PREFIX(rename),
    FS_PREFIX(ftruncate),
    FS_PREFIX(ffilesize),
    fdprintf,
    read_line,
    settings_parseline,
    storage_sleep,
    STORAGE_FUNCTION(spin),
    STORAGE_FUNCTION(spindown),
#ifdef USING_STORAGE_CALLBACK
    register_storage_idle_func,
    unregister_storage_idle_func,
#endif /* USING_STORAGE_CALLBACK */
    reload_directory,
    create_numbered_filename,
    FS_PREFIX(file_exists),
    strip_extension,
    crc_32,
    crc_32r,
    filetype_get_attr,
    filetype_get_plugin,
#ifdef HAVE_DIRCACHE
    dircache_wait,
#endif

    /* dir */
    FS_PREFIX(opendir),
    FS_PREFIX(closedir),
    FS_PREFIX(readdir),
    FS_PREFIX(mkdir),
    FS_PREFIX(rmdir),
    FS_PREFIX(dir_exists),
    FS_PREFIX(dir_get_info),

    /* browsing */
    rockbox_browse,
    tree_get_context,
    tree_get_entries,
    tree_get_entry_at,
    set_current_file,
    set_dirfilter,
    onplay_show_playlist_menu,
    onplay_show_playlist_cat_menu,
    browse_id3,

    /* talking */
    talk_id,
    talk_idarray,
    talk_file,
    talk_file_or_spell,
    talk_fullpath,
    talk_dir_or_spell,
    talk_number,
    talk_value_decimal,
    talk_spell,
    talk_time,
    talk_date,
    talk_disable,
    talk_shutup,
    talk_force_shutup,
    talk_force_enqueue_next,

    /* kernel/ system */
    panicf,
    sleep,
    yield,
    &current_tick,
    default_event_handler,
    default_event_handler_ex,
    create_thread,
    thread_self,
    thread_exit,
    thread_wait,
    thread_thaw,
    thread_set_priority,
    mutex_init,
    mutex_lock,
    mutex_unlock,
    semaphore_init,
    semaphore_wait,
    semaphore_release,
    reset_poweroff_timer,
    set_sleeptimer_duration, /*stub*/
    get_sleep_timer, /*stub*/
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    &cpu_frequency,

#endif /* PLATFORM_NATIVE */
#ifdef HAVE_SCHEDULER_BOOSTCTRL
    trigger_cpu_boost,
    cancel_cpu_boost,
#endif

    commit_dcache,
    commit_discard_dcache,
    commit_discard_idcache,

    lc_open,
    lc_get_header,
    lc_close,

    timer_register,
    timer_unregister,
    timer_set_period,

    queue_init,
    queue_delete,
    queue_post,
    queue_wait_w_tmo,
    queue_enable_queue_send,
    queue_empty,
    queue_wait,
    queue_send,
    queue_reply,
    queue_remove_from_head,

#ifdef RB_PROFILE
    profile_thread,
    profstop,
    __cyg_profile_func_enter,
    __cyg_profile_func_exit,
#endif
    add_event,
    add_event_ex,
    remove_event,
    remove_event_ex,
    send_event,

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)
    /* special simulator hooks */
#endif

    /* strings and memory */
    snprintf,
    vsnprintf,
    vuprintf,
    strcpy,
    strncpy,
    strlcpy,
    strlen,
    strrchr,
    strcmp,
    strncmp,
    strcasecmp,
    strncasecmp,
    strstr,
    memset,
    memcpy,
    memmove,
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    _ctype_,
#endif
    atoi,
    strtol,
    strtoul,
    strchr,
    strcat,
    strlcat,
    memchr,
    memcmp,
    strcasestr,
    strtok_r,
    output_dyn_value,
    /* unicode stuff */
    utf8decode,
    iso_decode,
    utf16LEdecode,
    utf16BEdecode,
    utf8encode,
    utf8length,
    utf8seek,

    /* language */
    lang_is_rtl,

    /* the buflib memory management library */
    buflib_init,
    buflib_available,
    buflib_alloc,
    buflib_alloc_ex,
    buflib_alloc_maximum,
    buflib_buffer_in,
    buflib_buffer_out,
    buflib_free,
    buflib_shrink,
    buflib_get_data,

    /* sound */
    adjust_volume,
    sound_set,
    sound_current, /*stub*/
    sound_default,
    sound_min,
    sound_max,
    sound_unit,
    sound_val2phys,
#ifdef AUDIOHW_HAVE_EQ
    sound_enum_hw_eq_band_setting,
#endif
#if defined (HAVE_PITCHCONTROL)
    sound_get_pitch,
    sound_set_pitch,
#endif
    pcm_play_lock,
    pcm_play_unlock,
    pcm_current_sink_caps,
    beep_play,
    dsp_set_crossfeed_type ,
    dsp_eq_enable,
    dsp_dither_enable,
#ifdef HAVE_PITCHCONTROL
    dsp_get_timestretch,
    dsp_set_timestretch,
    dsp_timestretch_enable,
    dsp_timestretch_available,
#endif
    dsp_configure,
    dsp_get_config,
    dsp_process,

    mixer_channel_status,
    mixer_channel_get_buffer,
    mixer_channel_calculate_peaks,
    mixer_channel_play_data,
    mixer_channel_play_pause,
    mixer_channel_stop,
    mixer_channel_set_amplitude,
    mixer_channel_get_bytes_waiting,
    mixer_channel_set_buffer_hook,
    mixer_set_frequency,
    mixer_get_frequency,

    pcmbuf_fade,
    pcmbuf_set_low_latency,
    system_sound_play,
    keyclick_click,

    /* metadata */
    get_metadata,
    get_codec_string,
    count_mp3_frames,
    create_xing_header,
    tagtree_entries_iterate,
    tagcache_search,
    tagcache_search_set_uniqbuf,
    tagcache_search_add_filter,
    tagcache_get_next,
    tagcache_retrieve,
    tagcache_search_finish,
    tagcache_get_numeric,
    tagcache_get_stat,
    tagcache_commit_finalize,
#if defined(HAVE_TC_RAMCACHE)
    tagcache_is_in_ram,
#if defined(HAVE_DIRCACHE)
    tagcache_fill_tags,
#endif /* HAVE_DIRCACHE */
#endif /* HAVE_TC_RAMCACHE */

#ifdef HAVE_ALBUMART
    search_albumart_files,
#endif

    /* playback control */
    playlist_get_current,
    playlist_get_resume_info,
    playlist_get_track_info,
    playlist_get_first_index,
    playlist_get_display_index,
    playlist_entries_iterate,
    playlist_amount,
    playlist_resume,
    playlist_resume_track,
    playlist_set_modified,
    playlist_start,
    playlist_sync,
    playlist_remove_all_tracks,
    playlist_create,
    playlist_insert_track,
    playlist_insert_directory,
    playlist_insert_playlist,
    playlist_insert_context_create,
    playlist_insert_context_release,
    playlist_shuffle,
    warn_on_pl_erase,
    audio_play,
    audio_stop,
    audio_pause,
    audio_resume,
    audio_next,
    audio_prev,
    audio_pre_ff_rewind,
    audio_ff_rewind,
    audio_next_track,
    audio_status,
    audio_current_track,
    audio_flush_and_reload_tracks,
    audio_get_file_pos,
    add_playbacklog,

    /* menu */
    root_menu_get_options,
    do_menu,
    root_menu_set_default,
    root_menu_write_to_cfg,
    root_menu_load_from_cfg,

    /* options */
    get_settings_list,
    find_setting,
    settings_save,
    option_screen,
    set_option,
    set_bool_options,
    set_int,
    set_int_ex,
    set_bool,
    set_color,

    /* power */
    &device_battery_tables,
    battery_level,
    battery_level_safe,
    battery_time,
    battery_voltage,
    battery_current,
    charger_inserted,
    charging_state,

    /* usb */
    usb_inserted,
    usb_acknowledge,
#ifdef USB_ENABLE_HID
    usb_hid_send,
#endif
#ifdef USB_ENABLE_AUDIO
    usb_audio_get_playing,
#endif

    /* misc */
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    __errno,
#endif
    led,
    srand,
    rand,
    (void *)qsort,
    kbd_input,
    get_time,
    gmtime_r,
    mktime,
    format_time_auto,
#if defined(DEBUG) || defined(SIMULATOR)
    debugf,
#endif
#ifdef ROCKBOX_HAS_LOGF
    _logf,
#endif
    codec_thread_do_callback,
    codec_load_file,
    codec_run_proc,
    codec_close,
    get_codec_filename,
    find_array_ptr,
    remove_array_ptr,
    round_value_to_list32,

    read_bmp_file,
    read_bmp_fd,
    read_jpeg_file,
    read_jpeg_fd,
    screen_dump_set_hook,

    /*plugin*/
    plugin_open,
    plugin_get_buffer,
    plugin_get_audio_buffer,     /* defined in plugin.c */
    plugin_release_audio_buffer, /* defined in plugin.c */
    plugin_tsr,                  /* defined in plugin.c */
    plugin_get_current_filename,
    plugin_reserve_buffer,

    /* reboot and poweroff */
    sys_poweroff,
    sys_reboot,

    /* pathfuncs */
    fix_path_part,
#ifdef HAVE_MULTIVOLUME
    path_strip_volume,
#endif

    /* new stuff at the end, sort into place next time
       the API gets incompatible */
};

static int plugin_buffer_handle;
static size_t plugin_buffer_size;

int plugin_load(const char* plugin, const void* parameter)
{
    struct plugin_header *p_hdr;
    struct lc_header     *hdr;
    const char * resume_plugin = NULL;

    if (!plugin)
        return PLUGIN_ERROR;

    /* for some plugins, the SBS can be left enabled */
    const char *sepch = strrchr(plugin, PATH_SEPCH);
    bool theme_enabled = sepch && (!strcmp("properties.rock", sepch + 1) ||
                                   !strcmp("playing_time.rock", sepch + 1) ||
                                   !strcmp("main_menu_config.rock", sepch + 1) ||
                                   !strcmp("text_viewer.rock", sepch + 1) ||
                                   !strcmp("view_text.rock", sepch + 1) ||
                                   !strcmp("disktidy.rock", sepch + 1) ||
                                   !strcmp("open_plugins.rock", sepch + 1));

    if (current_plugin_handle)
    {
        if (!pfn_tsr_exit)
        {
            /* not allowing another plugin to load */
            logf("Attempt to load plugin `%s` while another non-TSR plugin `%s` is running.", plugin, current_plugin);
            return PLUGIN_OK;
        }

        /* if we have a resident old plugin and a callback */
        bool reenter = (strcmp(current_plugin, plugin) == 0);
        int exit_status = pfn_tsr_exit(reenter);
        if (exit_status == PLUGIN_TSR_CONTINUE)
        {
            /* not allowing another plugin to load */
            return PLUGIN_OK;
        }
        else
        {
            lc_close(current_plugin_handle);
            current_plugin_handle = pfn_tsr_exit = NULL;
            plugin_buffer_handle = core_free(plugin_buffer_handle);

            if (!reenter)
                resume_plugin = strdupa(current_plugin);
            else if (exit_status == PLUGIN_TSR_TERMINATE)
                return PLUGIN_OK; /* don't even load the new plugin either */
        }
    }

    strcpy(current_plugin, plugin);
    current_plugin_handle = lc_open(plugin, pluginbuf, PLUGIN_BUFFER_SIZE);
    if (current_plugin_handle == NULL) {
        if (global_settings.talk_menu)
        {
            talk_id(LANG_PLUGIN_CANT_OPEN, false);
            talk_spell(plugin, true);
            talk_force_enqueue_next();
        }
        /* (voiced above) */
        splashf(HZ*2, str(LANG_PLUGIN_CANT_OPEN), plugin);
        return -1;
    }

    p_hdr = lc_get_header(current_plugin_handle);
    hdr = p_hdr ? &p_hdr->lc_hdr : NULL;

    if (hdr == NULL
        || hdr->magic != PLUGIN_MAGIC
        || hdr->target_id != TARGET_ID
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
        || hdr->load_addr != pluginbuf
        || hdr->end_addr > pluginbuf + PLUGIN_BUFFER_SIZE
#endif
        )
    {
        hdr = NULL;
    }

    if (hdr == NULL || hdr->api_version != PLUGIN_API_VERSION ||
        p_hdr->api_size > sizeof(struct plugin_api))
    {
        lc_close(current_plugin_handle);
        current_plugin_handle = NULL;
        splash(HZ*2, hdr ? ID2P(LANG_PLUGIN_WRONG_VERSION)
                         : ID2P(LANG_PLUGIN_WRONG_MODEL));
        return -1;
    }

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    /* tlsf crashes observed on arm with 0x4 aligned addresses */
    plugin_size = ALIGN_UP(hdr->end_addr - pluginbuf, 0x8);
#else
    plugin_size = 0;
#endif

    *(p_hdr->api) = &rockbox_api;
    lcd_set_viewport(NULL);
    if (!theme_enabled)
        lcd_clear_display();

    if (get_current_activity() == ACTIVITY_WPS)
        push_activity_without_refresh(ACTIVITY_PLUGIN);
    else
        push_current_activity(ACTIVITY_PLUGIN);
    /* some plugins assume the entry cache doesn't move and save pointers to it
     * they should be fixed properly instead of this lock */
    struct tree_context *tc = tree_get_context();
    tree_lock_cache(tc);
    int *last_dirfilter_p = tc->dirfilter; /* store incoming dirfilter pointer */

    if (!theme_enabled)
        FOR_NB_SCREENS(i)
            viewportmanager_theme_enable(i, false, NULL);

    /* allow voice to back off if the plugin needs lots of memory */
    if (!global_settings.talk_menu)
        talk_buffer_set_policy(TALK_BUFFER_LOOSE);

    screen_helper_setfont(FONT_UI);

    plugin_check_open_close__enter();

    int rc = p_hdr->entry_point(parameter); /* run the loaded plugin */

    /* unlock the tree and restore the dirfilter pointer */
    tree_unlock_cache(tc);
    tc->dirfilter = last_dirfilter_p;

    pop_current_activity_without_refresh();
    if (get_current_activity() != ACTIVITY_WPS)
    {
        FOR_NB_SCREENS(i)
            skin_update(CUSTOM_STATUSBAR, i, SKIN_REFRESH_ALL);
        sb_skin_force_next_update();
    }

    if (!pfn_tsr_exit)
    {   /* close handle if plugin is no tsr one */
        lc_close(current_plugin_handle);
        current_plugin_handle = NULL;
        plugin_buffer_handle = core_free(plugin_buffer_handle);
    }

    talk_buffer_set_policy(TALK_BUFFER_DEFAULT);

    /* Go back to the global setting in case the plugin changed it */
    /* restore default vp */
    lcd_set_viewport(NULL);
    screen_helper_setfont(FONT_UI);
    lcd_set_drawinfo(DRMODE_SOLID, global_settings.fg_color,
                                   global_settings.bg_color);

    if (!theme_enabled)
    {
        lcd_clear_display();
        FOR_NB_SCREENS(i)
            viewportmanager_theme_undo(i, true);
    }
    else
        /* fix dangling sbs_title pointer */
        FOR_NB_SCREENS(i)
            sb_set_title_text(NULL, Icon_NOICON, i);

    plugin_check_open_close__exit();

    status_save(false);

    if (rc == PLUGIN_ERROR)
        splash(HZ*2, str(LANG_PLUGIN_ERROR));

    if (resume_plugin && rc != PLUGIN_GOTO_PLUGIN && !pfn_tsr_exit)
    {
            /*plugin = resume_plugin;*/
            /*parameter = rockbox_api.plugin_tsr;*/
            return plugin_load(resume_plugin, rockbox_api.plugin_tsr);
    }
    return rc;
}

/* For Terminate Stay Resident plugins
 * Locks buffer_size bytes of the plugin buffer
 * freed on plugin exit; call plugin_get_buffer first then reserve all
 * or a portion with plugin_reserve_buffer()
 * Returns size of buffer remaining */
size_t plugin_reserve_buffer(size_t buffer_size)
{
    size_t locked_size = 0;

    if (current_plugin_handle)
    {
        locked_size = ALIGN_UP(plugin_size + buffer_size, 0x8);
        if (locked_size > PLUGIN_BUFFER_SIZE)
            locked_size = PLUGIN_BUFFER_SIZE;

        plugin_size = locked_size;
    }

    return (PLUGIN_BUFFER_SIZE - locked_size);
}

/* Returns a pointer to the portion of the plugin buffer that is not already
   being used.  If no plugin is loaded, returns the entire plugin buffer */
void* plugin_get_buffer(size_t *buffer_size)
{
    int buffer_pos;

    if (current_plugin_handle)
    {
        if (plugin_size >= PLUGIN_BUFFER_SIZE)
            return NULL;

        *buffer_size = PLUGIN_BUFFER_SIZE-plugin_size;
        buffer_pos = plugin_size;
    }
    else
    {
        *buffer_size = PLUGIN_BUFFER_SIZE;
        buffer_pos = 0;
    }

    return &pluginbuf[buffer_pos];
}

/* Returns a pointer to the mp3 buffer.
   Playback gets stopped, to avoid conflicts.
   Talk buffer is stolen as well.
 */
static void* plugin_get_audio_buffer(size_t *buffer_size)
{
    if (plugin_buffer_handle <= 0)
    {
        plugin_buffer_handle = core_alloc_maximum(&plugin_buffer_size,
                                                  &buflib_ops_locked);
    }

    if (buffer_size)
        *buffer_size = plugin_buffer_size;

    return core_get_data(plugin_buffer_handle);
}

static void plugin_release_audio_buffer(void)
{
    if (plugin_buffer_handle > 0)
    {
        plugin_buffer_handle = core_free(plugin_buffer_handle);
        plugin_buffer_size = 0;
    }
}

/* The plugin wants to stay resident after leaving its main function, e.g.
   runs from timer or own thread. The callback is registered to later
   instruct it to free its resources before a new plugin gets loaded. */
static void plugin_tsr(int (*exit_callback)(bool))
{
    pfn_tsr_exit = exit_callback; /* remember the callback for later */
}

int plugin_open(const char *plugin, const char *parameter)
{
    open_plugin_add_path(ID2P(LANG_OPEN_PLUGIN), plugin, parameter);
    return PLUGIN_GOTO_PLUGIN;
}

char *plugin_get_current_filename(void)
{
    return current_plugin;
}
