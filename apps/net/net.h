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

/* Network manager: Wi-Fi on demand, saved networks, DHCP, and a small
 * sockets API for HTTP(S).
 *
 * Users take a reference while they need the network (net_acquire); the
 * manager powers Wi-Fi up, joins the strongest saved network in range and
 * gets an address. A minute after the last reference is dropped, Wi-Fi is
 * powered off again. */

#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "wifi.h"

enum net_state {
    NET_OFF,
    NET_STARTING,       /* loading the chip's firmware */
    NET_SEARCHING,      /* scanning for saved networks */
    NET_CONNECTING,     /* joining a network */
    NET_ADDRESSING,     /* DHCP */
    NET_UP,
    NET_FAILED,         /* see net_status_text() */
};

#define NET_MAX_SAVED 16

struct net_saved {
    char ssid[WIFI_SSID_LEN + 1];
    char psk[WIFI_PSK_MAX + 1];
    uint8_t security;
};

void net_heap_init(void);
void net_init(void);

void net_acquire(void);
void net_release(void);
/* Waits for NET_UP; false on failure, timeout or cancel() returning true */
/* true while an acquire or connect request is still being handled */
bool net_pending(void);
bool net_wait_up(long timeout_ticks, bool (*cancel)(void));

enum net_state net_get_state(void);
/* What the network is doing, for the user */
const char *net_status_text(char *buf, size_t len);
/* Status bar: 0 off, 1 busy, 2 connected */
int net_status_icon(void);
const char *net_ssid(void);
const char *net_ip(char *buf, size_t len);
int net_rssi(void);

/* Scans (Wi-Fi must be running: net_acquire first). Blocks a few seconds.
 * Returns the number of networks found or < 0. */
int net_scan(struct wifi_bss *out, int max);
/* Joins a network chosen by the user and remembers it if it works */
void net_connect(const char *ssid, const char *psk, int security);
int net_saved_list(struct net_saved *out, int max);
bool net_is_saved(const char *ssid);
void net_forget(const char *ssid);

/* --- sockets --- */
#define NET_ERR_TIMEOUT  (-2)
#define NET_ERR_CLOSED   (-3)

/* Resolves host and opens a TCP connection. Returns a socket or < 0. */
int net_tcp_connect(const char *host, int port, int timeout_ms);
int net_send(int s, const void *buf, size_t len);
/* > 0 bytes read, 0 closed by the peer, NET_ERR_TIMEOUT, or < 0 error */
int net_recv(int s, void *buf, size_t len, int timeout_ms);
void net_close(int s);

/* --- time (from SNTP), for certificate checks --- */
bool net_time_known(void);
time_t net_time(void);

/* --- memory: the network heap --- */
void *net_malloc(unsigned long size);
void *net_calloc(unsigned long n, unsigned long size);
void *net_realloc(void *p, unsigned long size);
void net_free(void *p);
char *net_strdup(const char *s);

/* --- entropy for TLS --- */
size_t net_entropy(uint8_t *out, size_t len);

/* --- backend (net_lwip.c on the player, net_sim.c in the simulator) --- */
void netb_init(void);
int netb_start(char *err, size_t errlen);
void netb_stop(void);
int netb_scan(struct wifi_bss *out, int max);
int netb_join(const char *ssid, const char *psk, int security);
void netb_leave(void);
void netb_link_up(void);    /* starts DHCP */
void netb_link_down(void);
const char *netb_ip(char *buf, size_t len);
int netb_rssi(void);
/* events from the backend */
enum { NETB_LINK_UP, NETB_LINK_DOWN, NETB_NO_NETWORK, NETB_JOIN_FAILED,
       NETB_AUTH_FAILED, NETB_GOT_IP };
void net_backend_event(int ev);
void net_set_time(time_t t);

#endif /* NET_H */
