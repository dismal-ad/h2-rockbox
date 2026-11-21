/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2021 by Tomasz Moń
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

/* Wi-Fi station driver (Broadcom/Cypress FullMAC chips on SDIO).
 *
 * The chip's firmware does 802.11 and the WPA2 handshake; the host sends
 * and receives Ethernet frames. The driver has its own thread that polls
 * the chip; the receive and event callbacks run in it. */

#ifndef WIFI_H
#define WIFI_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define WIFI_SSID_LEN       32
#define WIFI_PSK_MAX        64
#define WIFI_FW_DIR         "/.rockbox/wifi/"

enum wifi_security {
    WIFI_SEC_OPEN = 0,
    WIFI_SEC_WEP,           /* not supported */
    WIFI_SEC_WPA,           /* WPA-PSK (TKIP) */
    WIFI_SEC_WPA2,          /* WPA2-PSK, also WPA2/WPA3 mixed */
    WIFI_SEC_WPA3,          /* SAE only: not supported */
    WIFI_SEC_ENTERPRISE,    /* 802.1X: not supported */
};

struct wifi_bss {
    uint8_t bssid[6];
    char ssid[WIFI_SSID_LEN + 1];
    int16_t rssi;           /* dBm */
    uint8_t channel;
    uint8_t security;       /* enum wifi_security */
};

enum wifi_event {
    WIFI_EV_LINK_UP,        /* associated, keys installed */
    WIFI_EV_LINK_DOWN,      /* lost the network, or left it */
    WIFI_EV_NO_NETWORK,     /* the network was not found */
    WIFI_EV_JOIN_FAILED,    /* association failed */
    WIFI_EV_AUTH_FAILED,    /* key exchange failed: wrong password? */
};

struct wifi_callbacks {
    /* An Ethernet frame from the network */
    void (*rx)(const uint8_t *frame, size_t len);
    /* enum wifi_event */
    void (*event)(int event);
};

/* Once, before anything else */
void wifi_hw_init(const struct wifi_callbacks *cb);

/* Powers the chip and starts its firmware (files in WIFI_FW_DIR). Takes
 * about a second. On failure returns < 0 and a reason in err. */
int wifi_hw_start(char *err, size_t errlen);
/* Leaves any network and powers the chip off */
void wifi_hw_stop(void);
bool wifi_hw_running(void);

/* Active scan of all channels; blocks for 2-3 s. Returns the number of
 * networks (strongest first) or < 0. */
int wifi_hw_scan(struct wifi_bss *out, int max);

/* Starts joining; the result comes as an event */
int wifi_hw_join(const char *ssid, const char *psk, int security);
void wifi_hw_leave(void);

/* Sends an Ethernet frame. Returns 0 or < 0 if it was dropped. */
int wifi_hw_send(const void *frame, size_t len);

void wifi_hw_mac(uint8_t mac[6]);
/* Signal of the joined network in dBm, 0 if unknown */
int wifi_hw_rssi(void);
const char *wifi_hw_fw_version(void);

/* Fast power save on or off while joined */
void wifi_hw_power_save(bool on);
/* The driver's log (newest last), for the Wi-Fi screen */
size_t wifi_hw_log(char *out, size_t size);

/* Entropy for TLS: timings and received data, hashed by the caller */
size_t wifi_hw_entropy(uint8_t *out, size_t len);

/* Debug menu tests */
int wifi_debug_probe(char *log, size_t size);

/* --- target interface (the SDIO bus) --- */

/* Powers the Wi-Fi core and its host controller, and brings the card to
 * the transfer state (SDIO CMD5/3/7), then raises the bus clock */
int wifi_target_power_on(void);
void wifi_target_power_off(void);
/* CMD52 */
int wifi_sdio_rd8(int fn, uint32_t addr, uint8_t *val);
int wifi_sdio_wr8(int fn, uint32_t addr, uint8_t val);
/* CMD53: blocks of blksz first, then the rest in byte mode. len must be a
 * multiple of 4. */
int wifi_sdio_rw(bool write, int fn, uint32_t addr, bool incr,
                 uint8_t *buf, size_t len, unsigned blksz);
/* Microseconds, for timing and entropy */
uint32_t wifi_target_usec(void);

#endif /* WIFI_H */
