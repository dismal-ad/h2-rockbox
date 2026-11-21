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

/* Network manager: the state machine (net thread), saved networks and the
 * network heap. The backend (lwIP on Wi-Fi, or host sockets in the
 * simulator) does the actual work. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "file.h"
#include "buflib.h"
#include "core_alloc.h"
#include "tlsf.h"
#include "string-extra.h"
#include "logf.h"
#include "misc.h"
#include "dir.h"
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVED_FILE      WIFI_FW_DIR "networks.txt"
#define IDLE_OFF_TICKS  (60 * HZ)   /* Wi-Fi off a minute after last use */
#define JOIN_TIMEOUT    (20 * HZ)
#define DHCP_TIMEOUT    (20 * HZ)

/***************************************************************************
 * Heap
 ***************************************************************************/

static void *net_heap;

void *net_malloc(unsigned long size)
{
    return net_heap ? malloc_ex(size, net_heap) : NULL;
}

void *net_calloc(unsigned long n, unsigned long size)
{
    return net_heap ? calloc_ex(n, size, net_heap) : NULL;
}

void *net_realloc(void *p, unsigned long size)
{
    return net_heap ? realloc_ex(p, size, net_heap) : NULL;
}

void net_free(void *p)
{
    if (p && net_heap)
        free_ex(p, net_heap);
}

char *net_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = net_malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

/***************************************************************************
 * Time
 ***************************************************************************/

static bool time_known;
static time_t time_base;    /* UTC seconds at tick 0 */

void net_set_time(time_t t)
{
    time_base = t - current_tick / HZ;
    time_known = true;
}

bool net_time_known(void)
{
    return time_known;
}

time_t net_time(void)
{
    return time_base + current_tick / HZ;
}

/***************************************************************************
 * Saved networks: "<ssid>\t<security>\t<password>" lines
 ***************************************************************************/

static struct net_saved saved[NET_MAX_SAVED];
static int nr_saved;
static bool saved_loaded;
static struct mutex saved_mtx;

static void saved_load(void)
{
    if (saved_loaded)
        return;
    saved_loaded = true;
    nr_saved = 0;
    int fd = open(SAVED_FILE, O_RDONLY);
    if (fd < 0)
        return;
    char line[160];
    while (nr_saved < NET_MAX_SAVED && read_line(fd, line, sizeof(line)) > 0) {
        char *t1 = strchr(line, '\t');
        if (!t1)
            continue;
        *t1++ = '\0';
        char *t2 = strchr(t1, '\t');
        if (!t2)
            continue;
        *t2++ = '\0';
        if (!line[0] || strlen(line) > WIFI_SSID_LEN || strlen(t2) > WIFI_PSK_MAX)
            continue;
        struct net_saved *s = &saved[nr_saved++];
        strmemccpy(s->ssid, line, sizeof(s->ssid));
        strmemccpy(s->psk, t2, sizeof(s->psk));
        s->security = atoi(t1);
    }
    close(fd);
}

static void saved_write(void)
{
    mkdir(WIFI_FW_DIR);
    int fd = open(SAVED_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    for (int i = 0; i < nr_saved; ++i)
        fdprintf(fd, "%s\t%d\t%s\n", saved[i].ssid, saved[i].security,
                 saved[i].psk);
    close(fd);
}

static void saved_put(const struct net_saved *n)
{
    mutex_lock(&saved_mtx);
    saved_load();
    int i;
    for (i = 0; i < nr_saved; ++i)
        if (!strcmp(saved[i].ssid, n->ssid))
            break;
    if (i == nr_saved) {
        if (nr_saved == NET_MAX_SAVED)
            i = NET_MAX_SAVED - 1;  /* replace the oldest added last */
        else
            nr_saved++;
    }
    /* the newest first */
    memmove(&saved[1], &saved[0], i * sizeof(saved[0]));
    saved[0] = *n;
    saved_write();
    mutex_unlock(&saved_mtx);
}

int net_saved_list(struct net_saved *out, int max)
{
    mutex_lock(&saved_mtx);
    saved_load();
    int n = MIN(max, nr_saved);
    memcpy(out, saved, n * sizeof(saved[0]));
    mutex_unlock(&saved_mtx);
    return n;
}

bool net_is_saved(const char *ssid)
{
    bool found = false;
    mutex_lock(&saved_mtx);
    saved_load();
    for (int i = 0; i < nr_saved; ++i)
        if (!strcmp(saved[i].ssid, ssid))
            found = true;
    mutex_unlock(&saved_mtx);
    return found;
}

void net_forget(const char *ssid)
{
    mutex_lock(&saved_mtx);
    saved_load();
    for (int i = 0; i < nr_saved; ++i) {
        if (!strcmp(saved[i].ssid, ssid)) {
            memmove(&saved[i], &saved[i + 1],
                    (nr_saved - i - 1) * sizeof(saved[0]));
            nr_saved--;
            saved_write();
            break;
        }
    }
    mutex_unlock(&saved_mtx);
}

/***************************************************************************
 * State machine
 ***************************************************************************/

enum {
    Q_ACQUIRE = 1,
    Q_RELEASE,
    Q_CONNECT,
    Q_BACKEND,
};

static struct event_queue net_q;
static long net_stack[(DEFAULT_STACK_SIZE + 8192) / sizeof(long)];

static volatile enum net_state state = NET_OFF;
static volatile bool request_pending;
static bool wifi_on;
static int refs;
static long idle_since;
static long deadline;

static char cur_ssid[WIFI_SSID_LEN + 1];
static char fail_text[96];

/* networks to try, best first */
static struct net_saved cands[NET_MAX_SAVED];
static int nr_cands, cand_idx;
static bool user_join;
static int rejoins;

static struct net_saved user_net;
static struct mutex scan_mtx;

static void set_failed(const char *fmt, const char *arg)
{
    snprintf(fail_text, sizeof(fail_text), fmt, arg ? arg : "");
    state = NET_FAILED;
    logf("net: %s", fail_text);
}

static bool start_wifi(void)
{
    if (wifi_on)
        return true;
    char err[96];
    state = NET_STARTING;
    if (netb_start(err, sizeof(err))) {
        set_failed("%s", err);
        return false;
    }
    wifi_on = true;
    return true;
}

static void stop_wifi(void)
{
    if (!wifi_on)
        return;
    netb_stop();
    wifi_on = false;
    cur_ssid[0] = '\0';
    state = NET_OFF;
}

static void join(const struct net_saved *n)
{
    strmemccpy(cur_ssid, n->ssid, sizeof(cur_ssid));
    state = NET_CONNECTING;
    deadline = current_tick + JOIN_TIMEOUT;
    if (netb_join(n->ssid, n->psk, n->security))
        net_backend_event(NETB_JOIN_FAILED);
}

static int scan_locked(struct wifi_bss *out, int max)
{
    mutex_lock(&scan_mtx);
    int n = netb_scan(out, max);
    mutex_unlock(&scan_mtx);
    return n;
}

/* Scans and joins the strongest saved network in range */
static void auto_connect(void)
{
    static struct wifi_bss found[32];
    struct net_saved list[NET_MAX_SAVED];
    int ns = net_saved_list(list, NET_MAX_SAVED);

    user_join = false;
    if (ns == 0) {
        set_failed("No saved Wi-Fi networks%s", "");
        return;
    }
    state = NET_SEARCHING;
    int nf = scan_locked(found, ARRAYLEN(found));
    nr_cands = 0;
    for (int f = 0; f < nf; ++f)        /* found is strongest first */
        for (int s = 0; s < ns; ++s)
            if (!strcmp(found[f].ssid, list[s].ssid))
                cands[nr_cands++] = list[s];
    cand_idx = 0;
    if (nr_cands == 0) {
        set_failed("No saved network in range%s", "");
        return;
    }
    join(&cands[0]);
}

/* A join didn't work: the next candidate, or give up */
static void join_failed(const char *why)
{
    netb_leave();
    if (!user_join && ++cand_idx < nr_cands) {
        join(&cands[cand_idx]);
        return;
    }
    set_failed(why, cur_ssid);
    if (!user_join)
        cur_ssid[0] = '\0';
}

static void handle_backend(int ev)
{
    switch (ev) {
    case NETB_LINK_UP:
        if (state == NET_CONNECTING) {
            state = NET_ADDRESSING;
            deadline = current_tick + DHCP_TIMEOUT;
            netb_link_up();
        }
        break;
    case NETB_GOT_IP:
        if (state == NET_ADDRESSING) {
            state = NET_UP;
            rejoins = 0;
            if (user_join) {
                saved_put(&user_net);
                user_join = false;
            }
        }
        break;
    case NETB_LINK_DOWN:
        if (state == NET_UP || state == NET_ADDRESSING) {
            netb_link_down();
            /* try the same network again, then the others */
            if (rejoins++ < 3) {
                struct net_saved n;
                bool found = false;
                for (int i = 0; i < nr_cands; ++i)
                    if (!strcmp(cands[i].ssid, cur_ssid)) {
                        n = cands[i];
                        found = true;
                    }
                if (!found && !strcmp(user_net.ssid, cur_ssid)) {
                    n = user_net;
                    found = true;
                }
                if (found) {
                    join(&n);
                    break;
                }
            }
            auto_connect();
        }
        break;
    case NETB_NO_NETWORK:
        if (state == NET_CONNECTING)
            join_failed("%s is not in range");
        break;
    case NETB_JOIN_FAILED:
        if (state == NET_CONNECTING)
            join_failed("Could not connect to %s");
        break;
    case NETB_AUTH_FAILED:
        if (state == NET_CONNECTING)
            join_failed("Wrong password for %s?");
        break;
    }
}

static void net_thread(void)
{
    struct queue_event ev;
    while (1) {
        queue_wait_w_tmo(&net_q, &ev, HZ / 2);
        switch (ev.id) {
        case Q_ACQUIRE:
            refs++;
            if (state == NET_OFF || state == NET_FAILED) {
                if (start_wifi())
                    auto_connect();
            }
            request_pending = false;
            break;
        case Q_RELEASE:
            if (refs > 0 && --refs == 0)
                idle_since = current_tick;
            break;
        case Q_CONNECT:
            if (start_wifi()) {
                if (state == NET_UP || state == NET_ADDRESSING ||
                    state == NET_CONNECTING) {
                    netb_link_down();
                    netb_leave();
                }
                user_join = true;
                rejoins = 0;
                nr_cands = 0;
                join(&user_net);
            }
            request_pending = false;
            break;
        case Q_BACKEND:
            handle_backend((int)ev.data);
            break;
        }

        /* timeouts */
        if ((state == NET_CONNECTING || state == NET_ADDRESSING) &&
            TIME_AFTER(current_tick, deadline)) {
            if (state == NET_ADDRESSING) {
                netb_link_down();
                join_failed("No address from %s");
            } else {
                join_failed("Could not connect to %s");
            }
        }
        if (refs == 0 && wifi_on && !request_pending &&
            TIME_AFTER(current_tick, idle_since + IDLE_OFF_TICKS)) {
            netb_link_down();
            stop_wifi();
        }
    }
}

void net_backend_event(int ev)
{
    queue_post(&net_q, Q_BACKEND, ev);
}

/* Early, next to the Bluetooth heap, before playback takes the memory */
void net_heap_init(void)
{
    int h = core_alloc_ex(NET_HEAP_SIZE, &buflib_ops_locked);
    if (h > 0) {
        net_heap = core_get_data(h);
        memset(net_heap, 0, 4); /* make sure TLSF doesn't see ghosts */
        init_memory_pool(NET_HEAP_SIZE, net_heap);
    }
}

void net_init(void)
{
    mutex_init(&saved_mtx);
    mutex_init(&scan_mtx);
    queue_init(&net_q, true);
    netb_init();
    create_thread(net_thread, net_stack, sizeof(net_stack), 0, "net"
                  IF_PRIO(, PRIORITY_SYSTEM) IF_COP(, CPU));
}

void net_acquire(void)
{
    request_pending = true;
    queue_post(&net_q, Q_ACQUIRE, 0);
}

void net_release(void)
{
    queue_post(&net_q, Q_RELEASE, 0);
}

void net_connect(const char *ssid, const char *psk, int security)
{
    memset(&user_net, 0, sizeof(user_net));
    strmemccpy(user_net.ssid, ssid, sizeof(user_net.ssid));
    strmemccpy(user_net.psk, psk ? psk : "", sizeof(user_net.psk));
    user_net.security = security;
    request_pending = true;
    queue_post(&net_q, Q_CONNECT, 0);
}

bool net_pending(void)
{
    return request_pending;
}

bool net_wait_up(long timeout_ticks, bool (*cancel)(void))
{
    long end = current_tick + timeout_ticks;
    while (1) {
        if (!request_pending) {
            if (state == NET_UP)
                return true;
            if (state == NET_FAILED || state == NET_OFF)
                return false;
        }
        if (TIME_AFTER(current_tick, end))
            return false;
        if (cancel && cancel())
            return false;
        sleep(HZ / 10);
    }
}

enum net_state net_get_state(void)
{
    return state;
}

int net_scan(struct wifi_bss *out, int max)
{
    if (!wifi_on)
        return -1;
    return scan_locked(out, max);
}

const char *net_ssid(void)
{
    return cur_ssid;
}

const char *net_ip(char *buf, size_t len)
{
    if (state != NET_UP) {
        buf[0] = '\0';
        return buf;
    }
    return netb_ip(buf, len);
}

int net_rssi(void)
{
    return state == NET_UP ? netb_rssi() : 0;
}

int net_status_icon(void)
{
    switch (state) {
    case NET_OFF:
    case NET_FAILED:
        return 0;
    case NET_UP:
        return 2;
    default:
        return 1;
    }
}

const char *net_status_text(char *buf, size_t len)
{
    switch (state) {
    case NET_OFF:
        snprintf(buf, len, "Wi-Fi is off");
        break;
    case NET_STARTING:
        snprintf(buf, len, "Starting Wi-Fi...");
        break;
    case NET_SEARCHING:
        snprintf(buf, len, "Looking for networks...");
        break;
    case NET_CONNECTING:
        snprintf(buf, len, "Connecting to %s...", cur_ssid);
        break;
    case NET_ADDRESSING:
        snprintf(buf, len, "Getting an address from %s...", cur_ssid);
        break;
    case NET_UP:
        snprintf(buf, len, "Connected to %s", cur_ssid);
        break;
    case NET_FAILED:
        snprintf(buf, len, "%s", fail_text);
        break;
    }
    return buf;
}
