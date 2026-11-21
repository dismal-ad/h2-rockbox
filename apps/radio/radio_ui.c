/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
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

/* Radio and Wi-Fi screens */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "action.h"
#include "list.h"
#include "splash.h"
#include "yesno.h"
#include "keyboard.h"
#include "file.h"
#include "audio.h"
#include "root_menu.h"
#include "misc.h"
#include "playlist.h"
#include "settings.h"
#include "string-extra.h"
#include "net/net.h"
#include "radio.h"
#include "radio_dir.h"
#include "radio_ui.h"
#include <stdio.h>
#include <string.h>

/***************************************************************************
 * Waiting with a status line; Back cancels
 ***************************************************************************/

/* Shows text until done() or Back; true if done() */
static bool wait_status(bool (*done)(void), bool (*failed)(void),
                        const char *(*text)(char *, size_t), long timeout)
{
    char buf[128], last[128] = "";
    long end = current_tick + timeout;
    while (!done()) {
        if (failed && failed())
            return false;
        text(buf, sizeof(buf));
        if (strcmp(buf, last)) {
            splashf(0, "%s", buf);
            strmemccpy(last, buf, sizeof(last));
        }
        int action = get_action(CONTEXT_STD, HZ / 10);
        if (action == ACTION_STD_CANCEL)
            return false;
        if (TIME_AFTER(current_tick, end))
            return false;
    }
    return true;
}

static bool net_is_up(void)
{
    return net_get_state() == NET_UP;
}

static bool net_has_failed(void)
{
    return !net_pending() && (net_get_state() == NET_FAILED ||
                              net_get_state() == NET_OFF);
}

static const char *net_text(char *buf, size_t len)
{
    return net_status_text(buf, len);
}

/***************************************************************************
 * Wi-Fi screen
 ***************************************************************************/

#define MAX_NETS 32

static struct wifi_bss nets[MAX_NETS];
static int nr_nets;
static char status_line[128];

static const char *sec_name(int s)
{
    switch (s) {
    case WIFI_SEC_OPEN:       return "open";
    case WIFI_SEC_WEP:        return "WEP";
    case WIFI_SEC_WPA:        return "WPA";
    case WIFI_SEC_WPA2:       return "WPA2";
    case WIFI_SEC_WPA3:       return "WPA3";
    default:                  return "802.1X";
    }
}

static const char *signal_word(int rssi)
{
    return rssi >= -55 ? "strong" : rssi >= -67 ? "good" :
           rssi >= -78 ? "fair" : "weak";
}

/* rows: status, networks, "Search again", "Saved networks", "Log" */
static int wifi_rows(void)
{
    return 1 + nr_nets + 3;
}

static const char *wifi_get_name(int item, void *data, char *buf, size_t len)
{
    (void)data;
    if (item == 0) {
        char ip[20];
        net_status_text(status_line, sizeof(status_line));
        if (net_get_state() == NET_UP && net_ip(ip, sizeof(ip))[0])
            snprintf(buf, len, "%s\t%s", status_line, ip);
        else
            snprintf(buf, len, "%s", status_line);
        return buf;
    }
    item--;
    if (item < nr_nets) {
        const struct wifi_bss *b = &nets[item];
        const char *tag = net_get_state() == NET_UP &&
                          !strcmp(net_ssid(), b->ssid) ? "connected" :
                          net_is_saved(b->ssid) ? "saved" : sec_name(b->security);
        snprintf(buf, len, "%s\t%s, %s", b->ssid, tag, signal_word(b->rssi));
        return buf;
    }
    item -= nr_nets;
    return item == 0 ? "Search again" : item == 1 ? "Saved networks" : "Log";
}

static void do_scan(void)
{
    splash(0, "Searching for networks...");
    /* Wi-Fi may still be starting */
    long end = current_tick + 10 * HZ;
    while ((net_get_state() == NET_STARTING || net_get_state() == NET_OFF) &&
           !TIME_AFTER(current_tick, end))
        sleep(HZ / 10);
    int n = net_scan(nets, MAX_NETS);
    nr_nets = n > 0 ? n : 0;
    if (n < 0) {
        char st[96];
        splashf(2 * HZ, "%s", net_status_text(st, sizeof(st)));
    }
}

static void connect_to(const struct wifi_bss *b)
{
    struct net_saved saved[NET_MAX_SAVED];
    int ns = net_saved_list(saved, NET_MAX_SAVED);

    if (b->security == WIFI_SEC_WEP || b->security == WIFI_SEC_WPA3 ||
        b->security == WIFI_SEC_ENTERPRISE) {
        splashf(3 * HZ, "%s networks are not supported", sec_name(b->security));
        return;
    }
    for (int i = 0; i < ns; ++i) {
        if (!strcmp(saved[i].ssid, b->ssid)) {
            net_connect(b->ssid, saved[i].psk, b->security);
            return;
        }
    }
    if (b->security == WIFI_SEC_OPEN) {
        net_connect(b->ssid, "", WIFI_SEC_OPEN);
        return;
    }
    char psk[WIFI_PSK_MAX + 1] = "";
    splash(HZ, "Enter the password");
    if (kbd_input(psk, sizeof(psk), NULL) < 0)
        return;
    size_t l = strlen(psk);
    if (l < 8 || l > 63) {
        splash(2 * HZ, "A Wi-Fi password has 8 to 63 characters");
        return;
    }
    net_connect(b->ssid, psk, b->security);
}

static const char *saved_get_name(int item, void *data, char *buf, size_t len)
{
    struct net_saved *s = data;
    snprintf(buf, len, "%s", s[item].ssid);
    return buf;
}

static void saved_screen(void)
{
    struct net_saved saved[NET_MAX_SAVED];
    while (1) {
        int n = net_saved_list(saved, NET_MAX_SAVED);
        if (n == 0) {
            splash(2 * HZ, "No saved networks");
            return;
        }
        struct simplelist_info info;
        simplelist_info_init(&info, "Saved networks", n, saved);
        info.get_name = saved_get_name;
        simplelist_show_list(&info);
        if (info.selection < 0)
            return;
        char q[96];
        snprintf(q, sizeof(q), "Forget %s?", saved[info.selection].ssid);
        if (yesno_pop(q))
            net_forget(saved[info.selection].ssid);
    }
}

static void log_screen(void)
{
#ifndef SIMULATOR
    static char log[4096 + 256];
    char st[128], ip[20];
    size_t n = snprintf(log, sizeof(log), "%s %s\nfirmware %s\n",
                        net_status_text(st, sizeof(st)), net_ip(ip, sizeof(ip)),
                        wifi_hw_fw_version());
    wifi_hw_log(log + n, sizeof(log) - n);
    int fd = open("/wifi_log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, log, strlen(log));
        close(fd);
    }
    struct simplelist_info info;
    simplelist_info_init(&info, "Wi-Fi log (/wifi_log.txt)", 0, NULL);
    simplelist_reset_lines();
    char *line = log;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        simplelist_addline("%s", line);
        if (!nl)
            break;
        line = nl + 1;
    }
    info.scroll_all = true;
    simplelist_show_list(&info);
#endif
}

static int wifi_action(int action, struct gui_synclist *lists)
{
    static char last[128];
    if (action == ACTION_NONE) {
        /* follow the status */
        char now[128];
        net_status_text(now, sizeof(now));
        if (strcmp(now, last)) {
            strmemccpy(last, now, sizeof(last));
            return ACTION_REDRAW;
        }
    }
    if (action == ACTION_REDRAW)
        gui_synclist_set_nb_items(lists, wifi_rows());
    return action;
}

int wifi_screen(void)
{
    int sel = 0;
    net_acquire();
    do_scan();
    while (1) {
        struct simplelist_info info;
        simplelist_info_init(&info, "Wi-Fi", wifi_rows(), NULL);
        info.get_name = wifi_get_name;
        info.action_callback = wifi_action;
        info.timeout = HZ / 2;
        info.selection = sel;
        info.scroll_all = false;
        if (simplelist_show_list(&info)) {
            net_release();
            return GO_TO_ROOT;  /* USB */
        }
        if (info.selection < 0)
            break;
        sel = info.selection;
        int item = info.selection - 1;
        if (item < 0) {
            /* the status line: search again */
            do_scan();
        } else if (item < nr_nets) {
            connect_to(&nets[item]);
        } else if (item == nr_nets) {
            do_scan();
        } else if (item == nr_nets + 1) {
            saved_screen();
        } else {
            log_screen();
        }
    }
    net_release();
    return 0;
}

/***************************************************************************
 * Stations
 ***************************************************************************/

static struct radio_station *stations;     /* RADIO_MAX_STATIONS */
static int nr_stations;

static const char *station_get_name(int item, void *data, char *buf, size_t len)
{
    (void)data;
    const struct radio_station *s = &stations[item];
    char info[24] = "";
    if (s->codec[0] && s->bitrate)
        snprintf(info, sizeof(info), "%s %d", s->codec, s->bitrate);
    else if (s->codec[0] && strcasecmp(s->codec, "UNKNOWN"))
        snprintf(info, sizeof(info), "%s", s->codec);
    snprintf(buf, len, info[0] ? "%s\t%s" : "%s", s->name, info);
    return buf;
}

static int station_action(int action, struct gui_synclist *lists)
{
    /* Menu: favourite or not */
    if (action == ACTION_STD_MENU || action == ACTION_STD_CONTEXT) {
        int i = gui_synclist_get_sel_pos(lists);
        if (i >= 0 && i < nr_stations) {
            const struct radio_station *s = &stations[i];
            char q[RADIO_NAME_LEN + 32];
            if (radio_fav_has(s)) {
                snprintf(q, sizeof(q), "Remove %s from favourites?", s->name);
                if (yesno_pop(q))
                    radio_fav_remove(s);
            } else {
                snprintf(q, sizeof(q), "Add %s to favourites?", s->name);
                if (yesno_pop(q))
                    radio_fav_add(s);
            }
        }
        return ACTION_REDRAW;
    }
    return action;
}

static bool radio_started(void)
{
    return radio_poll_start();
}

static bool radio_failed(void)
{
    enum radio_state st = radio_get_state();
    return st == RADIO_FAILED || st == RADIO_IDLE;
}

static const char *radio_text(char *buf, size_t len)
{
    return radio_status_text(buf, len);
}

/* Plays a station; GO_TO_WPS when it started */
static int play_station(const struct radio_station *s)
{
    radio_start(s);
    if (wait_status(radio_started, radio_failed, radio_text, 60 * HZ)) {
        radio_last_save(s);
        return GO_TO_WPS;
    }
    if (radio_get_state() == RADIO_FAILED) {
        char buf[128];
        splashf(3 * HZ, "%s", radio_status_text(buf, sizeof(buf)));
    }
    radio_stop();
    return 0;
}

bool radio_resume_is_station(void)
{
    struct playlist_track_info ti;
    return global_status.resume_index >= 0 &&
           playlist_get_track_info(NULL, global_status.resume_index, &ti) >= 0 &&
           radio_is_path(ti.filename);
}

/* Resume Playback when the playlist holds a station: plays the last one
 * again, since the stream itself can't be resumed */
bool radio_resume_last(void)
{
    struct radio_station *s = net_malloc(sizeof(*s));
    if (!s)
        return false;
    bool ok = radio_last_load(s);
    if (ok)
        ok = play_station(s) == GO_TO_WPS;
    else
        splash(2 * HZ, "Nothing to resume");
    net_free(s);
    return ok;
}

/* Lists stations; returns GO_TO_WPS once one plays */
static int stations_screen(const char *title)
{
    int sel = 0;
    while (1) {
        if (nr_stations == 0) {
            splash(2 * HZ, "No stations");
            return 0;
        }
        struct simplelist_info info;
        simplelist_info_init(&info, (char *)title, nr_stations, NULL);
        info.get_name = station_get_name;
        info.action_callback = station_action;
        info.selection = sel;
        if (simplelist_show_list(&info))
            return GO_TO_ROOT;
        if (info.selection == -2)
            return GO_TO_WPS;
        if (info.selection < 0)
            return 0;
        sel = info.selection;
        int r = play_station(&stations[sel]);
        if (r)
            return r;
    }
}

/* The network for the directory; asks for a network if none is saved */
static bool ensure_network(void)
{
    if (net_get_state() == NET_UP)
        return true;
    struct net_saved saved[1];
    if (net_saved_list(saved, 1) == 0) {
        splash(2 * HZ, "Choose a Wi-Fi network first");
        wifi_screen();
    }
    if (net_get_state() == NET_UP)
        return true;
    /* after a failed join, look for the saved networks again */
    net_acquire();
    bool up = wait_status(net_is_up, net_has_failed, net_text, 45 * HZ);
    net_release();
    if (up)
        return true;
    char st[96];
    splashf(3 * HZ, "%s", net_status_text(st, sizeof(st)));
    return false;
}

enum dir_query { Q_POPULAR, Q_SEARCH, Q_COUNTRY, Q_TAG };

static int directory_stations(enum dir_query q, const char *arg,
                              const char *title)
{
    char err[96] = "";
    if (!ensure_network())
        return 0;
    splash(0, "Loading stations...");
    int n;
    switch (q) {
    case Q_POPULAR:
        n = radio_dir_popular(stations, RADIO_MAX_STATIONS, err, sizeof(err));
        break;
    case Q_SEARCH:
        n = radio_dir_search(arg, stations, RADIO_MAX_STATIONS, err, sizeof(err));
        break;
    case Q_COUNTRY:
        n = radio_dir_by_country(arg, stations, RADIO_MAX_STATIONS, err, sizeof(err));
        break;
    default:
        n = radio_dir_by_tag(arg, stations, RADIO_MAX_STATIONS, err, sizeof(err));
        break;
    }
    if (n < 0) {
        splashf(3 * HZ, "%s", err);
        return 0;
    }
    nr_stations = n;
    return stations_screen(title);
}

static struct radio_category *cats;
static int nr_cats;

static const char *cat_get_name(int item, void *data, char *buf, size_t len)
{
    (void)data;
    snprintf(buf, len, "%s\t%d", cats[item].name, cats[item].count);
    return buf;
}

static int categories_screen(bool country)
{
    char err[96] = "";
    if (!ensure_network())
        return 0;
    splash(0, "Loading...");
    cats = net_malloc(250 * sizeof(*cats));
    if (!cats) {
        splash(2 * HZ, "Out of memory");
        return 0;
    }
    nr_cats = country ? radio_dir_countries(cats, 250, err, sizeof(err))
                      : radio_dir_tags(cats, 120, err, sizeof(err));
    int ret = 0, sel = 0;
    if (nr_cats < 0) {
        splashf(3 * HZ, "%s", err);
        nr_cats = 0;
    }
    while (nr_cats > 0) {
        struct simplelist_info info;
        simplelist_info_init(&info, country ? "By country" : "By genre",
                             nr_cats, NULL);
        info.get_name = cat_get_name;
        info.selection = sel;
        if (simplelist_show_list(&info)) {
            ret = GO_TO_ROOT;
            break;
        }
        if (info.selection == -2) {
            ret = GO_TO_WPS;
            break;
        }
        if (info.selection < 0)
            break;
        sel = info.selection;
        struct radio_category c = cats[sel];
        ret = directory_stations(country ? Q_COUNTRY : Q_TAG, c.code, c.name);
        if (ret)
            break;
    }
    net_free(cats);
    cats = NULL;
    return ret;
}

static int search_screen(void)
{
    static char query[64];
    if (kbd_input(query, sizeof(query), NULL) < 0 || !query[0])
        return 0;
    char title[80];
    snprintf(title, sizeof(title), "\"%s\"", query);
    return directory_stations(Q_SEARCH, query, title);
}

static int favourites_screen(void)
{
    nr_stations = radio_fav_load(stations, RADIO_MAX_STATIONS);
    if (nr_stations == 0) {
        splash(3 * HZ, "No favourites yet: press Menu on a station");
        return 0;
    }
    int r = stations_screen("Favourites");
    return r;
}

static char (*file_names)[64];
static int nr_files;

static const char *file_get_name(int item, void *data, char *buf, size_t len)
{
    (void)data;
    (void)buf;
    (void)len;
    return file_names[item];
}

static int files_screen(void)
{
    file_names = net_malloc(64 * 64);
    if (!file_names)
        return 0;
    nr_files = radio_files_list(file_names, 64);
    int ret = 0, sel = 0;
    if (nr_files == 0)
        splash(3 * HZ, "Put .m3u or .pls files in /Radio");
    while (nr_files > 0) {
        struct simplelist_info info;
        simplelist_info_init(&info, "Station files", nr_files, NULL);
        info.get_name = file_get_name;
        info.selection = sel;
        if (simplelist_show_list(&info)) {
            ret = GO_TO_ROOT;
            break;
        }
        if (info.selection < 0)
            break;
        sel = info.selection;
        nr_stations = radio_file_load(file_names[sel], stations,
                                      RADIO_MAX_STATIONS);
        ret = stations_screen(file_names[sel]);
        if (ret)
            break;
    }
    net_free(file_names);
    file_names = NULL;
    return ret;
}

/***************************************************************************
 * Radio screen
 ***************************************************************************/

enum radio_item {
    RI_NOW_PLAYING,
    RI_FAVOURITES,
    RI_POPULAR,
    RI_SEARCH,
    RI_COUNTRY,
    RI_GENRE,
    RI_FILES,
    RI_WIFI,
    RI_COUNT
};

static const char *const radio_items[RI_COUNT] = {
    "Now playing", "Favourites", "Popular", "Search", "By country",
    "By genre", "Station files", "Wi-Fi",
};

static int visible[RI_COUNT];
static int nr_visible;

static bool radio_playing(void)
{
    struct mp3entry *id3 = audio_status() ? audio_current_track() : NULL;
    return id3 && radio_is_path(id3->path);
}

static const char *radio_get_name(int item, void *data, char *buf, size_t len)
{
    (void)data;
    int ri = visible[item];
    if (ri == RI_NOW_PLAYING) {
        const struct radio_station *s = radio_current();
        snprintf(buf, len, "%s\t%s", radio_items[ri], s ? s->name : "");
        return buf;
    }
    if (ri == RI_WIFI) {
        char st[96];
        snprintf(buf, len, "%s\t%s", radio_items[ri],
                 net_get_state() == NET_UP ? net_ssid() :
                 net_get_state() == NET_OFF ? "off" :
                 net_status_text(st, sizeof(st)));
        return buf;
    }
    return radio_items[ri];
}

static int radio_action(int action, struct gui_synclist *lists)
{
    (void)lists;
    return action == ACTION_NONE ? ACTION_REDRAW : action;
}

int radio_screen(void)
{
    int ret = 0, sel = 0;
    stations = net_malloc(RADIO_MAX_STATIONS * sizeof(*stations));
    if (!stations) {
        splash(2 * HZ, "Out of memory");
        return 0;
    }
    /* the directory needs the network: start it now */
    net_acquire();

    while (1) {
        nr_visible = 0;
        for (int i = 0; i < RI_COUNT; ++i) {
            if (i == RI_NOW_PLAYING && !radio_playing())
                continue;
            visible[nr_visible++] = i;
        }
        struct simplelist_info info;
        simplelist_info_init(&info, "Radio", nr_visible, NULL);
        info.get_name = radio_get_name;
        info.action_callback = radio_action;
        info.timeout = HZ;
        info.selection = MIN(sel, nr_visible - 1);
        if (simplelist_show_list(&info)) {
            ret = GO_TO_ROOT;
            break;
        }
        if (info.selection == -2) {
            ret = GO_TO_WPS;
            break;
        }
        if (info.selection < 0)
            break;
        sel = info.selection;
        int r = 0;
        switch (visible[sel]) {
        case RI_NOW_PLAYING: r = GO_TO_WPS; break;
        case RI_FAVOURITES:  r = favourites_screen(); break;
        case RI_POPULAR:     r = directory_stations(Q_POPULAR, NULL, "Popular"); break;
        case RI_SEARCH:      r = search_screen(); break;
        case RI_COUNTRY:     r = categories_screen(true); break;
        case RI_GENRE:       r = categories_screen(false); break;
        case RI_FILES:       r = files_screen(); break;
        case RI_WIFI:        r = wifi_screen(); break;
        }
        if (r) {
            ret = r;
            break;
        }
    }
    net_release();
    net_free(stations);
    stations = NULL;
    return ret;
}
