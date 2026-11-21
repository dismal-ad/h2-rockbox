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

/* Network backend on the player: lwIP on the Wi-Fi driver */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "logf.h"
#include "net.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>

#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/apps/sntp.h"
#include "netif/ethernet.h"

static struct netif wl_netif;
static bool netif_added;
static bool sntp_running;

/* --- Wi-Fi driver callbacks (driver thread) --- */

static void wl_rx(const uint8_t *frame, size_t len)
{
    if (!netif_added || len > 1600)
        return;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p)
        return;
    pbuf_take(p, frame, len);
    if (wl_netif.input(p, &wl_netif) != ERR_OK)
        pbuf_free(p);
}

static void wl_event(int ev)
{
    switch (ev) {
    case WIFI_EV_LINK_UP:     net_backend_event(NETB_LINK_UP); break;
    case WIFI_EV_LINK_DOWN:   net_backend_event(NETB_LINK_DOWN); break;
    case WIFI_EV_NO_NETWORK:  net_backend_event(NETB_NO_NETWORK); break;
    case WIFI_EV_JOIN_FAILED: net_backend_event(NETB_JOIN_FAILED); break;
    case WIFI_EV_AUTH_FAILED: net_backend_event(NETB_AUTH_FAILED); break;
    }
}

static const struct wifi_callbacks wl_cbs = {
    .rx = wl_rx,
    .event = wl_event,
};

/* --- lwIP netif --- */

static err_t wl_linkoutput(struct netif *n, struct pbuf *p)
{
    static uint8_t frame[1600];
    (void)n;
    if (p->tot_len > sizeof(frame))
        return ERR_IF;
    pbuf_copy_partial(p, frame, p->tot_len, 0);
    return wifi_hw_send(frame, p->tot_len) ? ERR_IF : ERR_OK;
}

static err_t wl_netif_init(struct netif *n)
{
    n->name[0] = 'w';
    n->name[1] = 'l';
    n->output = etharp_output;
    n->linkoutput = wl_linkoutput;
    n->mtu = 1500;
    n->hwaddr_len = 6;
    wifi_hw_mac(n->hwaddr);
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
#if LWIP_NETIF_HOSTNAME
    n->hostname = "rockbox-h2";
#endif
    return ERR_OK;
}

/* tcpip thread: an address from DHCP. Then the time, for certificate
 * checks. */
static void wl_status_cb(struct netif *n)
{
    if (netif_is_up(n) && !ip4_addr_isany_val(*netif_ip4_addr(n))) {
        if (!sntp_running) {
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, "pool.ntp.org");
            sntp_setservername(1, "time.google.com");
            sntp_init();
            sntp_running = true;
        }
        net_backend_event(NETB_GOT_IP);
    }
}

void net_sntp_set_time(unsigned long sec, unsigned long us)
{
    (void)us;
    net_set_time((time_t)sec);
}

unsigned long net_random(void)
{
    uint8_t b[4];
    net_entropy(b, 4);
    return b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned long)b[3] << 24);
}

size_t net_entropy(uint8_t *out, size_t len)
{
    size_t n = wifi_hw_entropy(out, len);
    uint32_t t = wifi_target_usec() ^ (uint32_t)current_tick;
    for (size_t i = n; i < len; ++i) {
        t = t * 1664525u + 1013904223u + wifi_target_usec();
        out[i] = t >> 24;
    }
    return len;
}

/* --- backend --- */

void netb_init(void)
{
    wifi_hw_init(&wl_cbs);
    tcpip_init(NULL, NULL);
}

int netb_start(char *err, size_t errlen)
{
    int rc = wifi_hw_start(err, errlen);
    if (rc)
        return rc;
    LOCK_TCPIP_CORE();
    if (!netif_added) {
        netif_add(&wl_netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4,
                  NULL, wl_netif_init, tcpip_input);
        netif_set_default(&wl_netif);
        netif_set_status_callback(&wl_netif, wl_status_cb);
        netif_added = true;
    } else {
        wifi_hw_mac(wl_netif.hwaddr);
    }
    netif_set_up(&wl_netif);
    UNLOCK_TCPIP_CORE();
    return 0;
}

void netb_stop(void)
{
    netb_link_down();
    LOCK_TCPIP_CORE();
    if (netif_added)
        netif_set_down(&wl_netif);
    UNLOCK_TCPIP_CORE();
    wifi_hw_stop();
}

int netb_scan(struct wifi_bss *out, int max)
{
    return wifi_hw_scan(out, max);
}

int netb_join(const char *ssid, const char *psk, int security)
{
    return wifi_hw_join(ssid, psk, security);
}

void netb_leave(void)
{
    wifi_hw_leave();
}

void netb_link_up(void)
{
    LOCK_TCPIP_CORE();
    netif_set_link_up(&wl_netif);
    dhcp_start(&wl_netif);
    UNLOCK_TCPIP_CORE();
    wifi_hw_power_save(true);
}

void netb_link_down(void)
{
    LOCK_TCPIP_CORE();
    if (netif_added) {
        if (sntp_running) {
            sntp_stop();
            sntp_running = false;
        }
        dhcp_release_and_stop(&wl_netif);
        netif_set_link_down(&wl_netif);
    }
    UNLOCK_TCPIP_CORE();
}

const char *netb_ip(char *buf, size_t len)
{
    ip4_addr_t a = *netif_ip4_addr(&wl_netif);
    ip4addr_ntoa_r(&a, buf, len);
    return buf;
}

int netb_rssi(void)
{
    return wifi_hw_rssi();
}

/* --- sockets --- */

int net_tcp_connect(const char *host, int port, int timeout_ms)
{
    ip_addr_t addr;
    if (netconn_gethostbyname(host, &addr) != ERR_OK)
        return -1;

    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    lwip_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_len = sizeof(sa);
    sa.sin_family = AF_INET;
    sa.sin_port = lwip_htons(port);
    inet_addr_from_ip4addr(&sa.sin_addr, ip_2_ip4(&addr));
    if (lwip_connect(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        lwip_close(s);
        return -1;
    }
    return s;
}

int net_send(int s, const void *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        int n = lwip_send(s, (const char *)buf + done, len - done, 0);
        if (n <= 0)
            return -1;
        done += n;
    }
    return (int)done;
}

int net_recv(int s, void *buf, size_t len, int timeout_ms)
{
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int n = lwip_recv(s, buf, len, 0);
    if (n > 0)
        return n;
    if (n == 0)
        return 0;
    int err = 0;
    socklen_t el = sizeof(err);
    lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el);
    if (err == EWOULDBLOCK || err == EAGAIN || err == 0)
        return NET_ERR_TIMEOUT;
    return NET_ERR_CLOSED;
}

void net_close(int s)
{
    if (s >= 0)
        lwip_close(s);
}
