/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 Jonathan Gordon
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

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include "config.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "rbpaths.h"
#include "menu.h"
#include "open_plugin.h"
#include "keyboard.h"
#include "sound_menu.h"
#include "exported_menus.h"
#include "tree.h"
#include "tagtree.h"
#include "usb.h"
#include "splash.h"
#include "yesno.h"
#include "talk.h"
#include "powermgmt.h"
#include "playback.h"
#include "screens.h"
#include "quickscreen.h"
#ifdef HAVE_DIRCACHE
#include "dircache.h"
#endif
#include "mask_select.h"
#include "plugin.h"
#include "onplay.h"
#include "misc.h"

#if !defined(SIMULATOR) && defined(HAVE_USB_POWER)
#define HAVE_USB_MODE
#endif

static int selectivesoftlock_callback(int action,
                                      const struct menu_item_ex *this_item,
                                      struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;

    switch (action)
    {
        case ACTION_STD_MENU:
        case ACTION_STD_CANCEL:
        case ACTION_EXIT_MENUITEM:
            set_selective_softlock_actions(
                            global_settings.bt_selective_softlock_actions,
                            global_settings.bt_selective_softlock_actions_mask);
            action_autosoftlock_init();
            break;
    }

    return action;
}

static int selectivesoftlock_set_mask(void* param)
{
    (void)param;
int mask = global_settings.bt_selective_softlock_actions_mask;
            struct s_mask_items maskitems[]={
                                       {ID2P(LANG_ACTION_VOLUME), SEL_ACTION_VOL},
                                       {ID2P(LANG_ACTION_PLAY),   SEL_ACTION_PLAY},
                                       {ID2P(LANG_ACTION_SEEK),   SEL_ACTION_SEEK},
                                       {ID2P(LANG_ACTION_SKIP),   SEL_ACTION_SKIP},
                                       {ID2P(LANG_ACTION_AUTOLOCK_ON),    SEL_ACTION_AUTOLOCK},
                                       {ID2P(LANG_ACTION_ALWAYSAUTOLOCK), SEL_ACTION_ALWAYSAUTOLOCK},
                                       {ID2P(LANG_ACTION_DISABLE_NOTIFY), SEL_ACTION_NONOTIFY},
                                       {ID2P(LANG_SOFTLOCK_DISABLE_ALL_NOTIFY), SEL_ACTION_ALLNONOTIFY}
                                            };

            mask = mask_select(mask, ID2P(LANG_SOFTLOCK_SELECTIVE)
                               , maskitems,ARRAYLEN(maskitems));

            if (mask == SEL_ACTION_NONE)
                global_settings.bt_selective_softlock_actions = false;
            else if (global_settings.bt_selective_softlock_actions_mask != mask)
                global_settings.bt_selective_softlock_actions = true;

            global_settings.bt_selective_softlock_actions_mask = mask;

    return true;
}

/***********************************/
/*    TAGCACHE MENU                */

static void tagcache_rebuild_with_splash(void)
{
    tagcache_rebuild();
    splash(HZ*2, ID2P(LANG_TAGCACHE_FORCE_UPDATE_SPLASH));
}

static void tagcache_update_with_splash(void)
{
    tagcache_update();
    splash(HZ*2, ID2P(LANG_TAGCACHE_FORCE_UPDATE_SPLASH));
}

static int dirs_to_scan(void)
{
    if(plugin_load(VIEWERS_DIR"/db_folder_select.rock", NULL) > PLUGIN_OK)
    {
        static const char *lines[] = {ID2P(LANG_TAGCACHE_BUSY),
                                      ID2P(LANG_TAGCACHE_FORCE_UPDATE)};
        static const struct text_message message = {lines, 2};

        if (gui_syncyesno_run(&message, NULL, NULL) == YESNO_YES)
            tagcache_rebuild_with_splash();
    }
    return 0;
}

#ifdef HAVE_TC_RAMCACHE
MENUITEM_SETTING(tagcache_ram, &global_settings.tagcache_ram, NULL);
#endif
MENUITEM_SETTING(tagcache_autoupdate, &global_settings.tagcache_autoupdate, NULL);
MENUITEM_FUNCTION(tc_init, 0, ID2P(LANG_TAGCACHE_FORCE_UPDATE),
                  (int(*)(void))tagcache_rebuild_with_splash, NULL, Icon_NOICON);
MENUITEM_FUNCTION(tc_update, 0, ID2P(LANG_TAGCACHE_UPDATE),
                  (int(*)(void))tagcache_update_with_splash, NULL, Icon_NOICON);
MENUITEM_SETTING(runtimedb, &global_settings.runtimedb, NULL);

MENUITEM_FUNCTION(tc_export, 0, ID2P(LANG_TAGCACHE_EXPORT),
                  tagtree_export,
                  NULL, Icon_NOICON);

MENUITEM_FUNCTION(tc_import, 0, ID2P(LANG_TAGCACHE_IMPORT),
                  tagtree_import,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(tc_paths, 0, ID2P(LANG_SELECT_DATABASE_DIRS),
                  dirs_to_scan, NULL, Icon_NOICON);

MAKE_MENU(tagcache_menu, ID2P(LANG_TAGCACHE), 0, Icon_NOICON,
#ifdef HAVE_TC_RAMCACHE
                &tagcache_ram,
#endif
                &tagcache_autoupdate, &tc_init, &tc_update, &runtimedb,
                &tc_export, &tc_import, &tc_paths
                );
/*    TAGCACHE MENU                */
/***********************************/

/***********************************/
/*    FILE VIEW MENU               */
MENUITEM_SETTING(sort_case, &global_settings.sort_case, NULL);
MENUITEM_SETTING(sort_dir, &global_settings.sort_dir, NULL);
MENUITEM_SETTING(sort_file, &global_settings.sort_file, NULL);
MENUITEM_SETTING(interpret_numbers, &global_settings.interpret_numbers, NULL);
MENUITEM_SETTING(dirfilter, &global_settings.dirfilter, NULL);
MENUITEM_SETTING(show_filename_ext, &global_settings.show_filename_ext, NULL);
MENUITEM_SETTING(browse_current, &global_settings.browse_current, NULL);
MENUITEM_SETTING(show_path_in_browser, &global_settings.show_path_in_browser, NULL);
MENUITEM_FUNCTION_W_PARAM(hotkey_tree_item, 0, ID2P(LANG_HOTKEY_FILE_BROWSER),
                                  tree_context_menu_do_setting, (void*)0,
                                  NULL, Icon_Menu_setting);
static int clear_start_directory(void)
{
    path_append(global_settings.start_directory, PATH_ROOTSTR,
                PA_SEP_HARD, sizeof(global_settings.start_directory));
    settings_save();
    splash(HZ, ID2P(LANG_RESET_DONE_CLEAR));
    return false;
}
MENUITEM_SETTING(keep_directory, &global_settings.keep_directory, NULL);
MENUITEM_FUNCTION(clear_start_directory_item, 0, ID2P(LANG_RESET_START_DIR),
                  clear_start_directory, NULL, Icon_file_view_menu);

static int filemenu_callback(int action,
                             const struct menu_item_ex *this_item,
                             struct gui_synclist *this_list);
MAKE_MENU(file_menu, ID2P(LANG_FILE), filemenu_callback, Icon_file_view_menu,
                &sort_case, &sort_dir, &sort_file, &interpret_numbers,
                &dirfilter, &show_filename_ext, &browse_current,
                &show_path_in_browser,
                &keep_directory,
                &clear_start_directory_item
                ,&hotkey_tree_item
                );
static int filemenu_callback(int action,
                             const struct menu_item_ex *this_item,
                             struct gui_synclist *this_list)
{
    (void)this_list;

    /* Show File View menu in Settings or File Browser,
       but not in Database or Playlist Catalog */
    if (action == ACTION_REQUEST_MENUITEM &&
        this_item == &file_menu &&
        get_current_activity() != ACTIVITY_SETTINGS &&
        (get_onplay_context() != CONTEXT_TREE
         || *tree_get_context()->dirfilter == SHOW_M3U))
        return ACTION_EXIT_MENUITEM;

    return action;
}

/*    FILE VIEW MENU               */
/***********************************/


/***********************************/
/*    SYSTEM MENU                  */

/* Battery */

#ifdef HAVE_USB_CHARGING_ENABLE
static int usbcharging_callback(int action,
                                const struct menu_item_ex *this_item,
                                struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    switch (action)
    {
        case ACTION_EXIT_MENUITEM: /* on exit */
            usb_charging_enable(global_settings.usb_charging);
            break;
    }
    return action;
}
MENUITEM_SETTING(usb_charging, &global_settings.usb_charging, usbcharging_callback);
#endif /* HAVE_USB_CHARGING_ENABLE */
MAKE_MENU(battery_menu, ID2P(LANG_BATTERY_MENU), 0, Icon_NOICON,
#ifdef HAVE_USB_CHARGING_ENABLE
            &usb_charging,
#endif
         );
#ifdef HAVE_USB_MODE
MENUITEM_SETTING(usb_mode, &global_settings.usb_mode, NULL);
#endif
/* Disk */
#ifdef HAVE_DIRCACHE
static int dircache_callback(int action,
                             const struct menu_item_ex *this_item,
                             struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    switch (action)
    {
        case ACTION_EXIT_MENUITEM: /* on exit */
            if (global_settings.dircache)
            {
                if (dircache_enable() < 0)
                    splash(HZ*2, ID2P(LANG_PLEASE_REBOOT));
            }
            else
            {
                dircache_disable();
            }
            break;
    }
    return action;
}
MENUITEM_SETTING(dircache, &global_settings.dircache, dircache_callback);
#endif
#if defined(HAVE_DIRCACHE)
MAKE_MENU(disk_menu, ID2P(LANG_DISK_MENU), 0, Icon_NOICON,
#ifdef HAVE_DIRCACHE
            &dircache,
#endif
         );
#endif

/* Limits menu */
MENUITEM_SETTING(max_files_in_dir, &global_settings.max_files_in_dir, NULL);
MENUITEM_SETTING(max_files_in_playlist, &global_settings.max_files_in_playlist, NULL);
MENUITEM_SETTING(default_glyphs, &global_settings.glyphs_to_cache, NULL);
MAKE_MENU(limits_menu, ID2P(LANG_LIMITS_MENU), 0, Icon_NOICON,
           &max_files_in_dir, &max_files_in_playlist
           ,&default_glyphs
           );

/* Volume adjustment */
MENUITEM_SETTING(volume_adjust_mode, &global_settings.volume_adjust_mode, NULL);
MENUITEM_SETTING(volume_adjust_norm_steps, &global_settings.volume_adjust_norm_steps, NULL);

/* Keyclick menu */
MENUITEM_SETTING(keyclick, &global_settings.keyclick, NULL);
MENUITEM_SETTING(keyclick_repeats, &global_settings.keyclick_repeats, NULL);
MAKE_MENU(keyclick_menu, ID2P(LANG_KEYCLICK), 0, Icon_NOICON,
           &keyclick, &keyclick_repeats);

#if CONFIG_CHARGING
MENUITEM_SETTING(car_adapter_mode, &global_settings.car_adapter_mode, NULL);
MENUITEM_SETTING(car_adapter_mode_delay, &global_settings.car_adapter_mode_delay, NULL);
MAKE_MENU(car_adapter_mode_menu, ID2P(LANG_CAR_ADAPTER_MODE), 0, Icon_NOICON,
           &car_adapter_mode, &car_adapter_mode_delay);
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
MENUITEM_SETTING(serial_bitrate, &global_settings.serial_bitrate, NULL);
#endif
#ifdef USB_ENABLE_HID
MENUITEM_SETTING(usb_hid, &global_settings.usb_hid, NULL);
MENUITEM_SETTING(usb_keypad_mode, &global_settings.usb_keypad_mode, NULL);
#endif
#if defined(USB_ENABLE_AUDIO)
MENUITEM_SETTING(usb_audio, &global_settings.usb_audio, NULL);
#endif
#if defined(USB_ENABLE_STORAGE) && defined(HAVE_MULTIDRIVE)
MENUITEM_SETTING(usb_skip_first_drive, &global_settings.usb_skip_first_drive, NULL);
#endif

MENUITEM_SETTING(shortcuts_replaces_quickscreen, &global_settings.shortcuts_replaces_qs, NULL);

MENUITEM_SETTING(bt_selective_actions,
                 &global_settings.bt_selective_softlock_actions,
                                                    selectivesoftlock_callback);
MENUITEM_FUNCTION(sel_softlock_mask, 0, ID2P(LANG_SETTINGS),
                  selectivesoftlock_set_mask, selectivesoftlock_callback,
                  Icon_Menu_setting);

MAKE_MENU(sel_softlock, ID2P(LANG_SOFTLOCK_SELECTIVE),
          NULL, Icon_Menu_setting, &bt_selective_actions, &sel_softlock_mask);

MAKE_MENU(usb_menu, ID2P(LANG_USB_MENU), 0, Icon_NOICON,
#ifdef USB_ENABLE_HID
            &usb_hid,
            &usb_keypad_mode,
#endif
#if defined(USB_ENABLE_AUDIO)
            &usb_audio,
#endif
#if defined(USB_ENABLE_STORAGE) && defined(HAVE_MULTIDRIVE)
            &usb_skip_first_drive,
#endif
#ifdef HAVE_USB_MODE
            &usb_mode,
#endif
         );

MAKE_MENU(system_menu, ID2P(LANG_SYSTEM),
          0, Icon_System_menu,
#if defined(HAVE_USB_CHARGING_ENABLE)
            &battery_menu,
#endif
#if defined(HAVE_DIRCACHE)
            &disk_menu,
#endif
            &limits_menu,
            &volume_adjust_mode,
            &volume_adjust_norm_steps,
            &shortcuts_replaces_quickscreen,
#if CONFIG_CHARGING
            &car_adapter_mode_menu,
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
            &serial_bitrate,
#endif
            &keyclick_menu,
            &sel_softlock,
#if (defined(USB_ENABLE_HID) || defined(USB_ENABLE_AUDIO)) || (defined(USB_ENABLE_STORAGE) && defined(HAVE_MULTIDRIVE)) || defined(HAVE_USB_MODE)
            &usb_menu,
#endif
         );

/*    SYSTEM MENU                  */
/***********************************/

/***********************************/
/*    STARTUP/SHUTDOWN MENU      */


char* sleeptimer_getname(int selected_item, void * data,
                         char *buffer, size_t buffer_len)
{
    (void)selected_item;
    (void)data;
    return string_sleeptimer(buffer, buffer_len);
}

int sleeptimer_voice(int selected_item, void*data)
{
    (void)selected_item;
    (void)data;
    talk_sleeptimer(-1);
    return 0;
}

/* Handle restarting a current sleep timer to the newly set default
   duration */
static int sleeptimer_duration_cb(int action,
                                  const struct menu_item_ex *this_item,
                                  struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    static int initial_duration;
    switch (action)
    {
        case ACTION_ENTER_MENUITEM:
            initial_duration = global_settings.sleeptimer_duration;
            break;
        case ACTION_EXIT_MENUITEM:
            if (initial_duration != global_settings.sleeptimer_duration
                    && get_sleep_timer())
                set_sleeptimer_duration(global_settings.sleeptimer_duration);
    }
    return action;
}

MENUITEM_SETTING(start_screen, &global_settings.start_in_screen, NULL);
MENUITEM_SETTING(poweroff, &global_settings.poweroff, NULL);
MENUITEM_FUNCTION_DYNTEXT(sleeptimer_toggle, 0, toggle_sleeptimer,
                          sleeptimer_getname, sleeptimer_voice, NULL,
                          NULL, Icon_NOICON);
MENUITEM_SETTING(sleeptimer_duration,
                 &global_settings.sleeptimer_duration,
                 sleeptimer_duration_cb);
MENUITEM_SETTING(sleeptimer_on_startup,
                 &global_settings.sleeptimer_on_startup, NULL);
MENUITEM_SETTING(keypress_restarts_sleeptimer,
                 &global_settings.keypress_restarts_sleeptimer, NULL);
MENUITEM_SETTING(show_shutdown_message, &global_settings.show_shutdown_message, NULL);

#if defined(BUTTON_REC)
#define SETTINGS_CLEAR_ON_HOLD
MENUITEM_SETTING(clear_settings_on_hold,
                 &global_settings.clear_settings_on_hold, NULL);
#endif

MAKE_MENU(startup_shutdown_menu, ID2P(LANG_STARTUP_SHUTDOWN),
          0, Icon_System_menu,
            &show_shutdown_message,
            &start_screen,
            &poweroff,
            &sleeptimer_toggle,
            &sleeptimer_duration,
            &sleeptimer_on_startup,
            &keypress_restarts_sleeptimer,
#if defined(SETTINGS_CLEAR_ON_HOLD)
            &clear_settings_on_hold,
#undef SETTINGS_CLEAR_ON_HOLD
#endif
         );

/*    STARTUP/SHUTDOWN MENU      */
/***********************************/

/***********************************/
/*    BOOKMARK MENU                */
static int bmark_callback(int action,
                          const struct menu_item_ex *this_item,
                          struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    switch (action)
    {
        case ACTION_EXIT_MENUITEM: /* on exit */
            if(global_settings.autocreatebookmark ==  BOOKMARK_RECENT_ONLY_YES ||
               global_settings.autocreatebookmark ==  BOOKMARK_RECENT_ONLY_ASK)
            {
                if(global_settings.usemrb == BOOKMARK_NO)
                    global_settings.usemrb = BOOKMARK_YES;

            }
            break;
    }
    return action;
}
MENUITEM_SETTING(autocreatebookmark,
                 &global_settings.autocreatebookmark, bmark_callback);
MENUITEM_SETTING(autoupdatebookmark, &global_settings.autoupdatebookmark, NULL);
MENUITEM_SETTING(autoloadbookmark, &global_settings.autoloadbookmark, NULL);
MENUITEM_SETTING(usemrb, &global_settings.usemrb, NULL);
MAKE_MENU(bookmark_settings_menu, ID2P(LANG_BOOKMARK_SETTINGS), 0,
          Icon_Bookmark,
          &autocreatebookmark, &autoupdatebookmark, &autoloadbookmark, &usemrb);
/*    BOOKMARK MENU                */
/***********************************/

/***********************************/
/*    AUTORESUME MENU              */

static int autoresume_callback(int action,
                               const struct menu_item_ex *this_item,
                               struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;

    if (action == ACTION_EXIT_MENUITEM  /* on exit */
        && global_settings.autoresume_enable
        && !tagcache_is_usable())
    {
        static const char *lines[] = {ID2P(LANG_TAGCACHE_BUSY),
                                      ID2P(LANG_TAGCACHE_FORCE_UPDATE)};
        static const struct text_message message = {lines, 2};

        if (gui_syncyesno_run(&message, NULL, NULL) == YESNO_YES)
            tagcache_rebuild_with_splash();
    }
    return action;
}

static int autoresume_nexttrack_callback(int action,
                                         const struct menu_item_ex *this_item,
                                         struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    static int oldval = 0;
    switch (action)
    {
        case ACTION_ENTER_MENUITEM:
            oldval = global_settings.autoresume_automatic;
            break;
        case ACTION_EXIT_MENUITEM:
            if (global_settings.autoresume_automatic == AUTORESUME_NEXTTRACK_CUSTOM
                && plugin_load(VIEWERS_DIR"/db_folder_select.rock",
                               str(LANG_AUTORESUME)) == PLUGIN_OK)
            {
                global_settings.autoresume_automatic = oldval;
            }
    }
    return action;
}

MENUITEM_SETTING(autoresume_enable, &global_settings.autoresume_enable,
                 autoresume_callback);
MENUITEM_SETTING(autoresume_automatic, &global_settings.autoresume_automatic,
                 autoresume_nexttrack_callback);

MAKE_MENU(autoresume_menu, ID2P(LANG_AUTORESUME),
          0, Icon_NOICON,
          &autoresume_enable, &autoresume_automatic);

/*    AUTORESUME MENU              */
/***********************************/

/***********************************/
/*    VOICE MENU                   */
static int talk_callback(int action,
                         const struct menu_item_ex *this_item,
                         struct gui_synclist *this_list);

MENUITEM_SETTING(talk_menu_item, &global_settings.talk_menu, NULL);
MENUITEM_SETTING(talk_dir_item, &global_settings.talk_dir, NULL);
MENUITEM_SETTING(talk_dir_clip_item, &global_settings.talk_dir_clip, talk_callback);
MENUITEM_SETTING(talk_file_item, &global_settings.talk_file, NULL);
MENUITEM_SETTING(talk_file_clip_item, &global_settings.talk_file_clip, talk_callback);
static int talk_callback(int action,
                         const struct menu_item_ex *this_item,
                         struct gui_synclist *this_list)
{
    (void)this_list;
    static int oldval = 0;
    switch (action)
    {
        case ACTION_ENTER_MENUITEM:
            oldval = global_settings.talk_file_clip;
            break;
        case ACTION_EXIT_MENUITEM:
            audio_set_crossfade(global_settings.crossfade);
            if (this_item == &talk_dir_clip_item)
                break;
            if (!oldval && global_settings.talk_file_clip)
            {
                /* force reload if newly talking thumbnails,
                because the clip presence is cached only if enabled */
                reload_directory();
            }
            break;
    }
    return action;
}
MENUITEM_SETTING(talk_filetype_item, &global_settings.talk_filetype, NULL);
MENUITEM_SETTING(talk_battery_level_item,
                 &global_settings.talk_battery_level, NULL);
MENUITEM_SETTING(talk_mixer_amp_item, &global_settings.talk_mixer_amp, NULL);
MAKE_MENU(voice_settings_menu, ID2P(LANG_VOICE), 0, Icon_Voice,
          &talk_menu_item, &talk_dir_item, &talk_dir_clip_item,
          &talk_file_item, &talk_file_clip_item, &talk_filetype_item,
          &talk_battery_level_item, &talk_mixer_amp_item);
/*    VOICE MENU                   */
/***********************************/

/*    WPS_CONTEXT_MENU          */
/***********************************/
static char *wps_context_menu_get_name(int selected_item, void * data,
                                   char *buffer, size_t buffer_len)
{
    int item = (intptr_t)data;
    int act = HK_CTX_GET(item, global_settings.context_wps);
    const struct hotkey_assignment *cur = get_hotkey(act);
    if (item == 0)
    {
        snprintf(buffer, buffer_len,"%s [%s]",
                 str(LANG_HOTKEY_WPS), str(cur->lang_id));
    }
    else
    {
        snprintf(buffer, buffer_len,"%s %d [%s]",
                 str(LANG_SET_CONTEXT_ITEM), item, str(cur->lang_id));
    }
    return buffer;
    (void)selected_item;
}

static int wps_context_menu_speak_item(int selected_item, void * data)
{
    int item = (intptr_t)data;
    int act = HK_CTX_GET(item, global_settings.context_wps);

    const struct hotkey_assignment *cur = get_hotkey(act);
    if (item == 0)
    {
        talk_id(LANG_HOTKEY_WPS, false);
    }
    else
    {
        talk_id(LANG_SET_CONTEXT_ITEM, false);
        talk_number(item, true);
    }
    talk_id(cur->lang_id, true);
    return 0;
    (void)selected_item;
}

MENUITEM_FUNCTION_DYNTEXT_W_PARAM(hotkey_wps_item, 0,
                                  wps_context_menu_do_setting, (void*)0,
                                  wps_context_menu_get_name,
                                  wps_context_menu_speak_item,
                                  (void*)0, NULL, Icon_Menu_setting);
MENUITEM_FUNCTION_DYNTEXT_W_PARAM(wps_set_context_item_1, 0,
                                  wps_context_menu_do_setting, (void*)1,
                                  wps_context_menu_get_name,
                                  wps_context_menu_speak_item,
                                  (void*)1, NULL, Icon_Menu_setting);

MENUITEM_FUNCTION_DYNTEXT_W_PARAM(wps_set_context_item_2, 0,
                                  wps_context_menu_do_setting, (void*)2,
                                  wps_context_menu_get_name,
                                  wps_context_menu_speak_item,
                                  (void*)2, NULL, Icon_Menu_setting);

MENUITEM_FUNCTION_DYNTEXT_W_PARAM(wps_set_context_item_3, 0,
                                  wps_context_menu_do_setting, (void*)3,
                                  wps_context_menu_get_name,
                                  wps_context_menu_speak_item,
                                  (void*)3, NULL, Icon_Menu_setting);

MENUITEM_FUNCTION_DYNTEXT_W_PARAM(wps_set_context_item_4, 0,
                                  wps_context_menu_do_setting, (void*)4,
                                  wps_context_menu_get_name,
                                  wps_context_menu_speak_item,
                                  (void*)4, NULL, Icon_Menu_setting);

static void reset_wps_items(void)
{
    reset_setting(find_setting(&global_settings.context_wps), NULL);
}
MENUITEM_FUNCTION(reset_wps_item, 0, ID2P(LANG_RESET), reset_wps_items, NULL, Icon_Queued)
/***********************************/
/*    WPS Settings MENU            */

MAKE_MENU(wps_settings, ID2P(LANG_WPS), 0, Icon_Playback_menu
            ,&hotkey_wps_item /* this is item 0 */
            ,&wps_set_context_item_1
            ,&wps_set_context_item_2
            ,&wps_set_context_item_3
            ,&wps_set_context_item_4
            ,&reset_wps_item
            );
/*    WPS Settings MENU            */
/***********************************/


/***********************************/
/*    SETTINGS MENU                */

static struct browse_folder_info langs = { LANG_DIR, SHOW_LNG };

MENUITEM_FUNCTION_W_PARAM(browse_langs, 0, ID2P(LANG_LANGUAGE),
                          browse_folder, (void*)&langs, NULL, Icon_Language);

MAKE_MENU(settings_menu_item, ID2P(LANG_GENERAL_SETTINGS), 0,
          Icon_General_settings_menu,
          &wps_settings,
          &playlist_settings, &file_menu,
          &tagcache_menu,
          &display_menu, &system_menu,
          &startup_shutdown_menu,
          &bookmark_settings_menu,
          &autoresume_menu,
          &browse_langs, &voice_settings_menu,
          );
/*    SETTINGS MENU                */
/***********************************/
