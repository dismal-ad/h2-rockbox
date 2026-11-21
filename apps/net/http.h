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

/* HTTP(S) GET client: redirects, Shoutcast/Icecast ("ICY") headers,
 * chunked bodies */

#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_URL_MAX 512

struct tls;

struct http {
    int sock;
    struct tls *tls;
    int status;
    char url[HTTP_URL_MAX];         /* after redirects */
    char content_type[64];
    long content_length;            /* -1 unknown */
    bool chunked;
    long chunk_left;
    bool body_done;
    int icy_metaint;                /* 0: no in-band titles */
    char icy_name[96];
    int icy_br;                     /* kbit/s, 0 unknown */
    /* receive buffer */
    uint8_t buf[2048];
    size_t pos, len;
};

/* Opens url (http:// or https://), follows up to 5 redirects. Asks for
 * in-band stream titles when icy is set. Returns 0 or < 0 with err. */
int http_open(struct http *h, const char *url, bool icy, char *err,
              size_t errlen);
/* Body bytes: > 0, 0 at the end, NET_ERR_TIMEOUT, or < 0 on errors */
int http_read(struct http *h, void *buf, size_t len, int timeout_ms);
void http_close(struct http *h);

/* The whole body (at most max bytes), NUL-terminated, on the network heap;
 * free it with net_free(). */
char *http_get(const char *url, size_t max, size_t *len, char *err,
               size_t errlen);

/* Percent-encodes s for a URL query into out */
void http_urlencode(const char *s, char *out, size_t outlen);

#endif /* HTTP_H */
