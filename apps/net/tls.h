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

/* TLS client on a net socket (Mbed TLS). Certificates are checked against
 * the CA bundle in /.rockbox/wifi/cacert.pem. */

#ifndef TLS_H
#define TLS_H

#include <stddef.h>

#define TLS_CA_FILE "/.rockbox/wifi/cacert.pem"

struct tls;

/* Handshake on a connected socket; the socket stays the caller's */
struct tls *tls_open(int sock, const char *host, char *err, size_t errlen);
int tls_send(struct tls *t, const void *buf, size_t len);
/* > 0 bytes, 0 closed, NET_ERR_TIMEOUT or < 0 */
int tls_recv(struct tls *t, void *buf, size_t len, int timeout_ms);
void tls_close(struct tls *t);

#endif /* TLS_H */
