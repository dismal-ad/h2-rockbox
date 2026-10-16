/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Stuart Martin
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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>
#include <stddef.h>
#include "inttypes.h"
#include "config.h"
#include "audiohw.h" /* for the AUDIOHW_* defines */
#include "statusbar.h" /* for the statusbar values */
#include "button.h"
#include "audio.h"
#include "dsp_proc_settings.h"
#include "gui/list.h"

struct opt_items {
    unsigned const char* string;
    int32_t voice_id;
};

/** Setting values defines **/
#define MAX_FILENAME 32
#define MAX_PATHNAME 80
#define MAX_PATHLIST (MAX_PATHNAME*2)

/* The values are assigned to the enums so that they correspond to */
/* setting values in settings_list.c                               */

/* Shared by all bookmark parameters */
enum {
    BOOKMARK_NO = 0,
    BOOKMARK_YES = 1,
};

/* Auto create bookmark */
enum {
    BOOKMARK_ASK = 2,
    BOOKMARK_RECENT_ONLY_YES = 3,
    BOOKMARK_RECENT_ONLY_ASK = 4,
};

/* Most recent bookmark */
enum {
    BOOKMARK_ONE_PER_PLAYLIST = 2,
    BOOKMARK_ONE_PER_TRACK = 3,
};

enum
{
    TRIG_MODE_OFF = 0,
    TRIG_MODE_NOREARM,
    TRIG_MODE_REARM
};

enum
{
    TRIG_TYPE_STOP = 0,
    TRIG_TYPE_PAUSE,
    TRIG_TYPE_NEW_FILE
};

enum {
    PLAYLIST_VIEWER_ENTRY_SHOW_FILE_NAME = 0,
    PLAYLIST_VIEWER_ENTRY_SHOW_FULL_PATH = 1,
    PLAYLIST_VIEWER_ENTRY_SHOW_ID3_TITLE_AND_ALBUM = 2,
    PLAYLIST_VIEWER_ENTRY_SHOW_ID3_TITLE = 3
};

enum {
    CROSSFADE_ENABLE_OFF = 0,
    CROSSFADE_ENABLE_AUTOSKIP,
    CROSSFADE_ENABLE_MANSKIP,
    CROSSFADE_ENABLE_SHUFFLE,
    CROSSFADE_ENABLE_SHUFFLE_OR_MANSKIP,
    CROSSFADE_ENABLE_ALWAYS,
};

enum {
    FOLDER_ADVANCE_OFF = 0,
    FOLDER_ADVANCE_NEXT,
    FOLDER_ADVANCE_RANDOM,
};

/* repeat mode options */
enum
{
    REPEAT_OFF = 0,
    REPEAT_ALL,
    REPEAT_ONE,
    REPEAT_SHUFFLE,
    NUM_REPEAT_MODES
};

/* single mode options */
enum {
    SINGLE_MODE_OFF = 0,
    SINGLE_MODE_TRACK,
    SINGLE_MODE_ALBUM,
    SINGLE_MODE_ALBUM_ARTIST,
    SINGLE_MODE_ARTIST,
    SINGLE_MODE_COMPOSER,
    SINGLE_MODE_GROUPING,
    SINGLE_MODE_GENRE,
    SINGLE_MODE_PLAYLIST,
};

enum
{
    QUEUE_HIDE = 0,
    QUEUE_SHOW_AT_TOPLEVEL,
    QUEUE_SHOW_IN_SUBMENU
};

#ifdef HAVE_ALBUMART
enum
{
    AA_OFF = 0,
    AA_PREFER_EMBEDDED,
    AA_PREFER_IMAGE_FILE
};
#endif

enum
{
    TAGCACHE_RAM_OFF = 0,
    TAGCACHE_RAM_ON = 1,
    TAGCACHE_RAM_QUICK = 2
};

/* dir filter options */
/* Note: Any new filter modes need to be added before NUM_FILTER_MODES.
 *       Any new rockbox browse filter modes (accessible through the menu)
 *       must be added after NUM_FILTER_MODES. */
enum { SHOW_ALL, SHOW_SUPPORTED, SHOW_MUSIC, SHOW_PLAYLIST, SHOW_ID3DB,
       NUM_FILTER_MODES,
       SHOW_WPS, SHOW_RWPS, SHOW_FMS, SHOW_RFMS, SHOW_SBS, SHOW_RSBS, SHOW_FMR, SHOW_CFG,
       SHOW_LNG, SHOW_MOD, SHOW_FONT, SHOW_PLUGINS, SHOW_M3U};

/* file and dir sort options */
enum { SORT_ALPHA, SORT_DATE, SORT_DATE_REVERSED, SORT_TYPE, /* available as settings */
       SORT_ALPHA_REVERSED, SORT_TYPE_REVERSED, SORT_AS_FILE }; /* internal use only */
enum { SORT_INTERPRET_AS_DIGIT, SORT_INTERPRET_AS_NUMBER };

/* recursive dir insert options */
enum { RECURSE_OFF, RECURSE_ON, RECURSE_ASK };

/* show path types */
enum { SHOW_PATH_OFF = 0, SHOW_PATH_CURRENT, SHOW_PATH_FULL };

/* scrollbar visibility/position */
enum { SCROLLBAR_OFF = 0, SCROLLBAR_LEFT, SCROLLBAR_RIGHT };

/* autoresume settings */
enum { AUTORESUME_NEXTTRACK_NEVER = 0, AUTORESUME_NEXTTRACK_ALWAYS,
       AUTORESUME_NEXTTRACK_CUSTOM};

/* Alarm settings */

/* Keyclick stuff */

 /* Not really a setting but several files should stay synced */
#define KEYCLICK_DURATION 2

/** virtual pointer stuff.. move to another .h maybe? **/
/* These define "virtual pointers", which could either be a literal string,
   or a mean a string ID if the pointer is in a certain range.
   This helps to save space for menus and options.

   While using 0 as the base address is fastest/simplest,
   it could result in a nullptr when ID==0 which C compilers try to
   helpfully "optimize" away.

   NOTE:  VIRT_PTR must point at an *invalid* address in the target
          memory map!
*/

#define VIRT_SIZE 0xFFFF /* more than enough for our string ID range */
/* offset from 0x0 slightly */
#define VIRT_PTR ((unsigned char*)sizeof(char*))

/* form a "virtual pointer" out of a language ID */
#define ID2P(id) (VIRT_PTR + id)

/* resolve a pointer which could be a virtualized ID or a literal */
#define P2STR(p) (char *)((p>=VIRT_PTR && p<VIRT_PTR+VIRT_SIZE) ? str(p-VIRT_PTR) : p)

/* get the string ID from a virtual pointer, -1 if not virtual */
#define P2ID(p) ((p>=VIRT_PTR && p<VIRT_PTR+VIRT_SIZE) ? p-VIRT_PTR : -1)

/* !defined(HAVE_LCD_COLOR) implies HAVE_LCD_CONTRAST with default 40.
   Explicitly define HAVE_LCD_CONTRAST in config file for newer ports for
   simplicity. */



/** function prototypes **/
void reset_runtime(void);
void update_runtime(void);
void zero_runtime(void);
void settings_load(void) INIT_ATTR;
bool settings_load_config(const char* file, bool apply);

void status_save(bool force);
int settings_save(void);

/* defines for the options paramater */
enum {
    SETTINGS_SAVE_CHANGED = 0,
    SETTINGS_SAVE_ALL,
    SETTINGS_SAVE_THEME,
    SETTINGS_SAVE_SOUND,
    SETTINGS_SAVE_EQPRESET,
    SETTINGS_SAVE_RESUMEINFO,
};
bool settings_save_config(int options);

struct settings_list;
struct filename_setting;
void reset_setting(const struct settings_list *setting, void *var);
void settings_reset(void);
void sound_settings_apply(void);

/* call this after loading a .wps/.rwps or other skin files, so that the
 * skin buffer is reset properly
 */

void settings_apply(bool read_disk);
void settings_apply_pm_range(void);
void settings_display(void);

enum optiontype { RB_INT, RB_BOOL };

const struct settings_list* find_setting(const void* variable);
const struct settings_list* find_setting_by_cfgname(const char* name);
bool cfg_int_to_string(const struct settings_list *setting, int val, char* buf, int buf_len);
bool cfg_string_to_int(const struct settings_list *setting, int* out, const char* str);
void cfg_to_string(const struct settings_list *setting, char* buf, int buf_len);
bool string_to_cfg(const char *name, char* value, bool *theme_changed);

bool copy_filename_setting(char *buf, size_t buflen, const char *input,
                           const struct filename_setting *fs);
bool set_bool_options(const char* string, const bool* variable,
                      const char* yes_str, int yes_voice,
                      const char* no_str, int no_voice,
                      void (*function)(bool));

bool set_bool(const char* string, const bool* variable);
bool set_int(const unsigned char* string, const char* unit, int voice_unit,
             const int* variable,
             void (*function)(int), int step, int min, int max,
             const char* (*formatter)(char*, size_t, int, const char*) );

/* use this one if you need to create a lang from the value (i.e with TALK_ID()) */
bool set_int_ex(const unsigned char* string, const char* unit, int voice_unit,
             const int* variable,
             void (*function)(int), int step, int min, int max,
             const char* (*formatter)(char*, size_t, int, const char*),
             int32_t (*get_talk_id)(int, int));

void set_file(const char* filename, char* setting);

bool set_option(const char* string, const void* variable, enum optiontype type,
                const struct opt_items* options, int numoptions, void (*function)(int));

const char* setting_get_cfgvals(const struct settings_list *setting);

/** global_settings and global_status struct definitions **/

struct system_status
{
    int volume;     /* audio output volume in decibels range depends on the dac */
    int resume_index;  /* index in playlist (-1 for no active resume) */
    uint32_t resume_crc32; /* crc32 of the name of the file */
    uint32_t resume_elapsed; /* elapsed time in last file */
    uint32_t resume_offset; /* byte offset in mp3 file */
#ifdef HAVE_PITCHCONTROL
    int32_t resume_pitch;
    int32_t resume_speed;
#endif
    int runtime;       /* current runtime since last charge */
    int topruntime;    /* top known runtime */
#ifdef HAVE_DIRCACHE
    int dircache_size;      /* directory cache structure last size, 22 bits */
#endif
    int last_screen;
    int last_browser;
    int  viewer_icon_count;
    int last_volume_change; /* tick the last volume change happened. skins use this */
    int font_id[NB_SCREENS]; /* font id of the settings font for each screen */

    int resume_modified; /* playlist is modified (=> warn before erase) */
    char browse_last_folder[MAX_PATH];/* only saved if keep_directory = true */
};

struct user_settings
{
    /* audio settings */
    int balance;    /* stereo balance: -100 - +100 -100=left  0=bal +100=right  */
    int bass;       /* bass boost/cut in decibels                               */
    int treble;     /* treble boost/cut in decibels                             */
    int channel_config; /* Stereo, Mono, Custom, Mono left, Mono right, Karaoke */
    int stereo_width; /* 0-255% */

#ifdef AUDIOHW_HAVE_BASS_CUTOFF
    int bass_cutoff;
#endif
#ifdef AUDIOHW_HAVE_TREBLE_CUTOFF
    int treble_cutoff;
#endif

    /* Crossfade */
    int crossfade;     /* Enable crossfade (0=off, 1=shuffle, 2=trackskip,
                                            3=shuff&trackskip, 4=always) */
    int crossfade_fade_in_delay;      /* Fade in delay (0-15s)             */
    int crossfade_fade_out_delay;     /* Fade out delay (0-15s)            */
    int crossfade_fade_in_duration;   /* Fade in duration (0-15s)          */
    int crossfade_fade_out_duration;  /* Fade out duration (0-15s)         */
    int crossfade_fade_out_mixmode;   /* Fade out mode (0=crossfade,1=mix) */

    /* Replaygain */
    struct replaygain_settings replaygain_settings;

    /* Crossfeed */
    int crossfeed;                              /* crossfeed type */
    unsigned int crossfeed_direct_gain;         /* dB x 10 */
    unsigned int crossfeed_cross_gain;          /* dB x 10 */
    unsigned int crossfeed_hf_attenuation;      /* dB x 10 */
    unsigned int crossfeed_hf_cutoff;           /* Frequency in Hz */

    /* EQ */
    bool eq_enabled;            /* Enable equalizer */
    unsigned int eq_precut;     /* dB */
    struct eq_band_setting eq_band_settings[EQ_NUM_BANDS]; /* for each band */

    /* Misc. swcodec */
    int  beep;              /* system beep volume when changing tracks etc. */
    int  keyclick;          /* keyclick volume */
    int  keyclick_repeats;  /* keyclick on repeats */
    bool dithering_enabled;
#ifdef HAVE_PITCHCONTROL
    bool timestretch_enabled;
#endif

    /* misc options */
    int list_accel_start_delay; /* ms before we start increaseing step size */
    int list_accel_wait; /* ms between increases */

    int  pause_rewind; /* time in s to rewind when pausing */
    int  unplug_mode; /* pause on headphone unplug */
#ifdef HAVE_BLUETOOTH
    bool bt_enabled;
    bool bt_high_quality; /* SBC dual channel at the highest bitpool */
    bool bt_volume_sync;  /* AVRCP absolute volume */
#endif
    bool unplug_autoresume; /* disable auto-resume if no phones */


    int timeformat;    /* time format: 0=24 hour clock, 1=12 hour clock */

    int dirfilter;     /* 0=display all, 1=only supported, 2=only music,
                          3=dirs+playlists, 4=ID3 database */
    int show_filename_ext; /* show filename extensions in file browser?
                              0 = no, 1 = yes, 2 = only unknown 0 */
    int default_codepage;   /* set default codepage for tag conversion */
    bool hold_lr_for_scroll_in_list; /* hold L/R scrolls the list left/right */
    bool play_selected; /* Plays selected file even in shuffle mode */
    int single_mode;    /* single mode - stop after every track, album, album artist,
                           artist, composer, work, or genre */
    bool party_mode;    /* party mode - unstoppable music */
    bool cuesheet;
    bool car_adapter_mode; /* 0=off 1=on */
    int car_adapter_mode_delay; /* delay before resume,  in seconds*/
    int start_in_screen;
    int ff_rewind_min_step; /* FF/Rewind minimum step size */
    int ff_rewind_accel; /* FF/Rewind acceleration (in seconds per doubling) */

    int peak_meter_release;   /* units per read out */
    int peak_meter_hold;      /* hold time for peak meter in 1/100 s */
    int peak_meter_clip_hold; /* hold time for clips */
    bool peak_meter_dbfs;     /* show linear or dbfs values */
    int peak_meter_min; /* range minimum */
    int peak_meter_max; /* range maximum */

    unsigned char wps_file[MAX_FILENAME+1];  /* last wps */
    unsigned char sbs_file[MAX_FILENAME+1];  /* last statusbar skin */
    unsigned char lang_file[MAX_FILENAME+1]; /* last language */
    unsigned char playlist_catalog_dir[MAX_PATHNAME+1];
    int skip_length; /* skip length */
    int max_files_in_dir; /* Max entries in directory (file browser) */
    int max_files_in_playlist; /* Max entries in playlist */
    int volume_type;   /* how volume is displayed: 0=graphic, 1=percent */
    int battery_display; /* how battery is displayed: 0=graphic, 1=percent */
    bool show_icons;   /* 0=hide 1=show */
    int statusbar;    /* STATUSBAR_* enum values */

    int scrollbar;    /* SCROLLBAR_* enum values */
    int scrollbar_width;

    int list_separator_height; /* -1=auto (== 1 currently), 0=disabled, X=height in pixels */
    int list_separator_color;
    /* goto current song when exiting WPS */
    bool browse_current; /* 1=goto current song,
                            0=goto previous location */
    bool scroll_paginated; /* 0=dont 1=do */
    bool list_wraparound;  /* wrap around to opposite end of list when scrolling */
    int  list_order;       /* order for numeric lists (ascending or descending) */
    int  scroll_speed;     /* long texts scrolling speed: 1-30 */
    int  bidir_limit;      /* bidir scroll length limit */
    int  scroll_delay;     /* delay (in 1/10s) before starting scroll */
    int  scroll_step;      /* pixels to advance per update */

    /* auto bookmark settings */
    int autoloadbookmark;   /* auto load option: 0=off, 1=ask, 2=on */
    int autocreatebookmark; /* auto create option: 0=off, 1=ask, 2=on */
    bool autoupdatebookmark;/* auto update option */
    int usemrb;             /* use MRB list: 0=No, 1=Yes, 2=One per playlist,
                                             3=One per playlist and track */

#ifdef HAVE_DIRCACHE
    bool dircache;          /* enable directory cache */
#endif
#ifdef HAVE_TC_RAMCACHE
    int tagcache_ram;        /* load tagcache to ram: 1=on, 2=quick (ignore dircache) */
#endif
    bool tagcache_autoupdate; /* automatically keep tagcache in sync? */
    bool autoresume_enable;   /* enable auto-resume feature? */
    int autoresume_automatic; /* resume next track? 0=never, 1=always,
                                 2=custom */
    unsigned char autoresume_paths[MAX_PATHLIST+1]; /* colon-separated list */
    bool runtimedb;           /* runtime database active? */
    unsigned char tagcache_scan_paths[MAX_PATHLIST+1];
    unsigned char tagcache_db_path[MAX_PATHNAME+1];

    unsigned char backdrop_file[MAX_PATHNAME+1];  /* backdrop bitmap file */

    int bg_color; /* background color native format */
    int fg_color; /* foreground color native format */
    int lss_color; /* background color for the selector or start color for the gradient */
    int lse_color; /* end color for the selector gradient */
    int lst_color; /* color of the text for the selector */
    unsigned char colors_file[MAX_FILENAME+1];

    /* playlist/playback settings */
    int  repeat_mode; /* 0=off 1=repeat all 2=repeat one 3=shuffle 4=ab */
    int  next_folder; /* move to next folder */
    bool constrain_next_folder; /* whether next_folder is constrained to
                                   directories within start_directory */
    int  recursive_dir_insert; /* should directories be inserted recursively */
    bool fade_on_stop; /* fade on pause/unpause/stop */
    bool playlist_shuffle;
    bool warnon_erase_dynplaylist; /* warn when erasing dynamic playlist */
    bool keep_current_track_on_replace_playlist;
    bool show_shuffled_adding_options; /* whether to display options for adding shuffled tracks to dynamic playlist */
    int show_queue_options; /* how and whether to display options to queue tracks */
#ifdef HAVE_ALBUMART
    int album_art; /* switch off album art display or choose preferred source */
#endif
    bool rewind_across_tracks;

    /* playlist viewer settings */
    bool playlist_viewer_icons; /* display icons on viewer */
    bool playlist_viewer_indices; /* display playlist indices on viewer */
    int playlist_viewer_track_display; /* how to display tracks in viewer */

    /* voice UI settings */
    bool talk_menu; /* enable voice UI */
    int talk_dir; /* voiced directories mode: 0=off 1=number 2=spell */
    bool talk_dir_clip; /* use directory .talk clips */
    int talk_file; /* voice file mode: 0=off, 1=number, 2=spell */
    bool talk_file_clip; /* use file .talk clips */
    bool talk_filetype; /* say file type */
    bool talk_battery_level;
    int  talk_mixer_amp; /* Relative volume of voices, MIX_AMP_MPUTE->MIX_AMP_UNITY */

    /* file browser sorting */
    bool sort_case; /* dir sort order: 0=case insensitive, 1=sensitive */
    int sort_dir;   /* 0=alpha, 1=date (old first), 2=date (new first) */
    int sort_file;  /* 0=alpha, 1=date, 2=date (new first), 3=type */
    int sort_playlists; /* in playlist catalog 0=alpha, 1=date, 2=date (new first) */
    int interpret_numbers; /* true=strnatcmp, false=strcmp */

    /* power settings */
    int poweroff;   /* idle power off timer */
#ifdef HAVE_USB_CHARGING_ENABLE
    int usb_charging;
#endif
    /* device settings */

    int  cursor_style; /* style of the selection cursor */
    int  screen_scroll_step;
    int  show_path_in_browser; /* 0=off, 1=current directory, 2=full path */
    bool offset_out_of_view;
    bool disable_mainmenu_scrolling;
    unsigned char icon_file[MAX_FILENAME+1];
    unsigned char viewers_icon_file[MAX_FILENAME+1];
    unsigned char font_file[MAX_FILENAME+1]; /* last font */
    int glyphs_to_cache; /* default font allocation size in glyphs */
    unsigned char kbd_file[MAX_FILENAME+1];  /* last keyboard */
    int  backlight_timeout;  /* backlight off timeout:  -1=never,
                                0=always, or time in seconds */
    bool caption_backlight; /* turn on backlight at end and start of track */
    bool bl_filter_first_keypress;   /* filter first keypress when dark? */
    int backlight_timeout_plugged;
    bool bt_selective_softlock_actions;
    int bt_selective_softlock_actions_mask;
    bool bl_selective_actions; /* backlight disable on some actions */
    int  bl_selective_actions_mask;/* mask of actions that will not enable backlight */
    int backlight_on_button_hold; /* what to do with backlight when hold
                                     switch is on */

    bool backlight_fade_in;
    bool backlight_fade_out;
    int brightness;

    bool prevent_skip;

#ifdef HAVE_PITCHCONTROL
    /* pitch screen settings */
    bool pitch_mode_semitone;
    bool pitch_mode_timestretch;
#endif
    /* If values are just added to the end, no need to bump plugin API
       version. */
    /* new stuff to be added at the end */

#ifdef USB_ENABLE_HID
    bool usb_hid;
    int usb_keypad_mode;
#endif

#if defined(USB_ENABLE_AUDIO)
    int usb_audio;
#endif

    unsigned char ui_vp_config[64]; /* viewport string for the lists */

    struct compressor_settings compressor_settings;

    int sleeptimer_duration; /* In minutes; 0=off */
    bool sleeptimer_on_startup;
    bool keypress_restarts_sleeptimer;

    bool show_shutdown_message; /* toggle whether display lights up and displays message
                                when shutting down */

    /* hotkey assignments - acceptable values are in
       hotkey_action enum in onplay.h */
    int hotkey_tree;
    /* WPS context assignments - acceptable values are in
       hotkey_action enum in onplay.h */
    int context_wps;
    /* When resuming playback (after a stop), rewind this number of seconds */
    int resume_rewind;

#ifdef AUDIOHW_HAVE_DEPTH_3D
    int depth_3d;
#endif

#ifdef AUDIOHW_HAVE_FILTER_ROLL_OFF
    int roll_off;
#endif

#ifdef AUDIOHW_HAVE_POWER_MODE
    int power_mode;
#endif

#ifdef AUDIOHW_HAVE_EQ
    /** Hardware EQ tone controls **/
    struct hw_eq_band
    {
        /* Maintain the order of members or sound_menu has to be changed */
        int gain;
#ifdef AUDIOHW_HAVE_EQ_FREQUENCY
        int frequency;
#endif
#ifdef AUDIOHW_HAVE_EQ_WIDTH
        int width;
#endif
    } hw_eq_bands[AUDIOHW_EQ_BAND_NUM];
#endif /* AUDIOHW_HAVE_EQ */

    char start_directory[MAX_PATHNAME+1];
    /* Has the root been customized from the .cfg file? false = no, true = loaded from cfg */
    bool keep_directory;  /* this saves the last path visited in the file browser */
    bool root_menu_customized;

    int play_frequency; /* core audio output frequency selection */
    int volume_limit; /* maximum volume limit */

    int volume_adjust_mode;
    int volume_adjust_norm_steps;

    int surround_enabled;
    int surround_balance;
    int surround_fx1;
    int surround_fx2;
    bool surround_method2;
    int surround_mix;

    int pbe;
    int pbe_precut;

    int afr_enabled;

#if !defined(SIMULATOR) && defined(HAVE_USB_POWER)
    int usb_mode;
#endif
#if defined(BUTTON_REC)
    bool clear_settings_on_hold;
#endif
    int hp_lo_select; /* indicates automatic, headphone-only, or lineout-only operation */
    bool playback_log; /* ROCKBOX_DIR/playback.log for tracks played */
};

/* global settings */
extern struct user_settings global_settings;
/* global status */
extern struct system_status global_status;

#endif /* __SETTINGS_H__ */
