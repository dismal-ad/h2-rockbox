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

/* Network backend for the simulator: a pretend Wi-Fi with two networks,
 * and the host's sockets. Simulator threads must not block in the host,
 * so the sockets are non-blocking and waits sleep in Rockbox. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* host networking, not Rockbox's file functions */
#undef read
#undef write
#undef close
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

static bool sim_running;
static char sim_ssid[WIFI_SSID_LEN + 1];

static const struct wifi_bss sim_networks[] = {
    { { 0x02, 0, 0, 0, 0, 1 }, "Simulator Wi-Fi", -48, 6, WIFI_SEC_WPA2 },
    { { 0x02, 0, 0, 0, 0, 2 }, "Cafe Guest", -67, 1, WIFI_SEC_OPEN },
    { { 0x02, 0, 0, 0, 0, 3 }, "Neighbour", -82, 11, WIFI_SEC_WPA2 },
};

void netb_init(void)
{
}

int netb_start(char *err, size_t errlen)
{
    (void)err;
    (void)errlen;
    sleep(HZ / 2);
    sim_running = true;
    return 0;
}

void netb_stop(void)
{
    sim_running = false;
}

int netb_scan(struct wifi_bss *out, int max)
{
    if (!sim_running)
        return -1;
    sleep(HZ);
    int n = MIN(max, (int)ARRAYLEN(sim_networks));
    memcpy(out, sim_networks, n * sizeof(out[0]));
    return n;
}

/* "Neighbour" refuses any password, the others accept any */
int netb_join(const char *ssid, const char *psk, int security)
{
    (void)psk;
    (void)security;
    if (!sim_running)
        return -1;
    strncpy(sim_ssid, ssid, WIFI_SSID_LEN);
    bool known = false;
    for (unsigned i = 0; i < ARRAYLEN(sim_networks); ++i)
        if (!strcmp(sim_networks[i].ssid, ssid))
            known = true;
    if (!known)
        net_backend_event(NETB_NO_NETWORK);
    else if (!strcmp(ssid, "Neighbour"))
        net_backend_event(NETB_AUTH_FAILED);
    else
        net_backend_event(NETB_LINK_UP);
    return 0;
}

void netb_leave(void)
{
}

void netb_link_up(void)
{
    net_set_time(time(NULL));
    net_backend_event(NETB_GOT_IP);
}

void netb_link_down(void)
{
}

const char *netb_ip(char *buf, size_t len)
{
    snprintf(buf, len, "192.168.1.42");
    return buf;
}

int netb_rssi(void)
{
    return -48;
}

size_t net_entropy(uint8_t *out, size_t len)
{
    static unsigned long seed;
    if (!seed)
        seed = (unsigned long)time(NULL) ^ (unsigned long)getpid();
    for (size_t i = 0; i < len; ++i) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        out[i] = seed >> 33;
    }
    return len;
}

/* --- sockets --- */

int net_tcp_connect(const char *host, int port, int timeout_ms)
{
    struct addrinfo hints, *res = NULL;
    char ports[8];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(ports, sizeof(ports), "%d", port);
    if (getaddrinfo(host, ports, &hints, &res) || !res)
        return -1;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        freeaddrinfo(res);
        return -1;
    }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    int rc = connect(s, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && errno != EINPROGRESS) {
        close(s);
        return -1;
    }

    long end = current_tick + (long)timeout_ms * HZ / 1000;
    while (1) {
        fd_set w;
        struct timeval tv = { 0, 0 };
        FD_ZERO(&w);
        FD_SET(s, &w);
        if (select(s + 1, NULL, &w, NULL, &tv) > 0) {
            int err = 0;
            socklen_t el = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el);
            if (err) {
                close(s);
                return -1;
            }
            return s;
        }
        if (TIME_AFTER(current_tick, end)) {
            close(s);
            return -1;
        }
        sleep(1);
    }
}

int net_send(int s, const void *buf, size_t len)
{
    size_t done = 0;
    long end = current_tick + 10 * HZ;
    while (done < len) {
        ssize_t n = send(s, (const char *)buf + done, len - done, MSG_NOSIGNAL);
        if (n > 0) {
            done += n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
            !TIME_AFTER(current_tick, end)) {
            sleep(1);
            continue;
        }
        return -1;
    }
    return (int)done;
}

int net_recv(int s, void *buf, size_t len, int timeout_ms)
{
    long end = current_tick + (long)timeout_ms * HZ / 1000;
    while (1) {
        ssize_t n = recv(s, buf, len, 0);
        if (n > 0)
            return (int)n;
        if (n == 0)
            return 0;
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            return NET_ERR_CLOSED;
        if (TIME_AFTER(current_tick, end))
            return NET_ERR_TIMEOUT;
        sleep(1);
    }
}

void net_close(int s)
{
    if (s >= 0)
        close(s);
}
