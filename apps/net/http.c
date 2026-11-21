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

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "string-extra.h"
#include "net.h"
#include "tls.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONNECT_TIMEOUT 10000
#define HEADER_TIMEOUT  10000
#define USER_AGENT      "Rockbox-H2-Radio/1.0"

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static bool prefix_ci(const char *s, const char *p)
{
    while (*p)
        if (lower(*s++) != lower(*p++))
            return false;
    return true;
}

/* scheme://host[:port]/path */
static bool parse_url(const char *url, bool *https, char *host, size_t hostlen,
                      int *port, const char **path)
{
    const char *p;
    if (prefix_ci(url, "https://")) {
        *https = true;
        p = url + 8;
        *port = 443;
    } else if (prefix_ci(url, "http://")) {
        *https = false;
        p = url + 7;
        *port = 80;
    } else {
        return false;
    }
    const char *end = p;
    while (*end && *end != '/' && *end != ':' && *end != '?' && *end != '#')
        end++;
    size_t hl = end - p;
    if (hl == 0 || hl >= hostlen)
        return false;
    memcpy(host, p, hl);
    host[hl] = '\0';
    if (*end == ':') {
        *port = atoi(end + 1);
        while (*end && *end != '/' && *end != '?')
            end++;
        if (*port <= 0 || *port > 65535)
            return false;
    }
    *path = *end ? end : "/";
    return true;
}

static int raw_recv(struct http *h, void *buf, size_t len, int timeout_ms)
{
    if (h->tls)
        return tls_recv(h->tls, buf, len, timeout_ms);
    return net_recv(h->sock, buf, len, timeout_ms);
}

static int raw_send(struct http *h, const void *buf, size_t len)
{
    if (h->tls)
        return tls_send(h->tls, buf, len);
    return net_send(h->sock, buf, len);
}

/* buffered bytes of the connection */
static int buf_fill(struct http *h, int timeout_ms)
{
    if (h->pos < h->len)
        return (int)(h->len - h->pos);
    h->pos = h->len = 0;
    int n = raw_recv(h, h->buf, sizeof(h->buf), timeout_ms);
    if (n > 0)
        h->len = n;
    return n;
}

/* one line without CR LF; < 0 on errors */
static int read_line_http(struct http *h, char *line, size_t size)
{
    size_t n = 0;
    while (1) {
        int r = buf_fill(h, HEADER_TIMEOUT);
        if (r <= 0)
            return r == 0 ? (n ? (int)n : -1) : r;
        char c = h->buf[h->pos++];
        if (c == '\n')
            break;
        if (c != '\r' && n + 1 < size)
            line[n++] = c;
    }
    line[n] = '\0';
    return (int)n;
}

static void disconnect(struct http *h)
{
    if (h->tls) {
        tls_close(h->tls);
        h->tls = NULL;
    }
    if (h->sock >= 0) {
        net_close(h->sock);
        h->sock = -1;
    }
}

static void resolve_location(const char *base, const char *loc, char *out,
                             size_t outlen)
{
    if (prefix_ci(loc, "http://") || prefix_ci(loc, "https://")) {
        strmemccpy(out, loc, outlen);
        return;
    }
    /* scheme://host[:port] of the base */
    const char *p = strstr(base, "://");
    p = p ? p + 3 : base;
    const char *slash = strchr(p, '/');
    size_t hl = slash ? (size_t)(slash - base) : strlen(base);
    int n;
    if (loc[0] == '/') {
        n = snprintf(out, outlen, "%.*s%s", (int)hl, base, loc);
    } else {
        /* relative to the base's directory */
        const char *last = strrchr(base, '/');
        size_t dl = (last && last >= base + hl) ? (size_t)(last - base + 1) : hl;
        n = snprintf(out, outlen, "%.*s%s%s", (int)dl, base,
                     dl == hl ? "/" : "", loc);
    }
    if (n < 0 || (size_t)n >= outlen)
        out[0] = '\0';     /* too long: no redirect */
}

static int open_once(struct http *h, bool icy, char *location, char *err,
                     size_t errlen)
{
    bool https;
    char host[128];
    int port;
    const char *path;

    if (!parse_url(h->url, &https, host, sizeof(host), &port, &path)) {
        snprintf(err, errlen, "Bad address");
        return -1;
    }
    h->sock = net_tcp_connect(host, port, CONNECT_TIMEOUT);
    if (h->sock < 0) {
        snprintf(err, errlen, "Could not connect to %s", host);
        return -1;
    }
    if (https) {
        h->tls = tls_open(h->sock, host, err, errlen);
        if (!h->tls)
            return -1;
    }

    char req[HTTP_URL_MAX + 256];
    int rl = snprintf(req, sizeof(req),
                      "GET %s HTTP/1.0\r\n"
                      "Host: %s\r\n"
                      "User-Agent: " USER_AGENT "\r\n"
                      "Accept: */*\r\n"
                      "%s"
                      "Connection: close\r\n\r\n",
                      path, host, icy ? "Icy-MetaData: 1\r\n" : "");
    if (rl >= (int)sizeof(req) || raw_send(h, req, rl) < 0) {
        snprintf(err, errlen, "Request failed");
        return -1;
    }

    char line[512];
    if (read_line_http(h, line, sizeof(line)) < 0) {
        snprintf(err, errlen, "No answer from %s", host);
        return -1;
    }
    /* "HTTP/1.1 200 OK" or Shoutcast's "ICY 200 OK" */
    const char *sp = strchr(line, ' ');
    h->status = sp ? atoi(sp + 1) : 0;
    if (!(prefix_ci(line, "HTTP/") || prefix_ci(line, "ICY")) || h->status <= 0) {
        snprintf(err, errlen, "Not an HTTP server");
        return -1;
    }

    while (1) {
        int n = read_line_http(h, line, sizeof(line));
        if (n < 0) {
            snprintf(err, errlen, "Connection lost");
            return -1;
        }
        if (n == 0)
            break;
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        char *v = colon + 1;
        while (*v == ' ' || *v == '\t')
            v++;
        if (prefix_ci(line, "content-type") && !line[12])
            strmemccpy(h->content_type, v, sizeof(h->content_type));
        else if (prefix_ci(line, "content-length") && !line[14])
            h->content_length = strtol(v, NULL, 10);
        else if (prefix_ci(line, "transfer-encoding") && prefix_ci(v, "chunked"))
            h->chunked = true;
        else if (prefix_ci(line, "location") && !line[8])
            strmemccpy(location, v, HTTP_URL_MAX);
        else if (prefix_ci(line, "icy-metaint"))
            h->icy_metaint = atoi(v);
        else if (prefix_ci(line, "icy-name"))
            strmemccpy(h->icy_name, v, sizeof(h->icy_name));
        else if (prefix_ci(line, "icy-br"))
            h->icy_br = atoi(v);
    }
    return 0;
}

int http_open(struct http *h, const char *url, bool icy, char *err,
              size_t errlen)
{
    char location[HTTP_URL_MAX];
    strmemccpy(h->url, url, sizeof(h->url));
    h->sock = -1;
    h->tls = NULL;

    for (int redirects = 0; redirects <= 5; ++redirects) {
        h->status = 0;
        h->content_type[0] = '\0';
        h->content_length = -1;
        h->chunked = false;
        h->chunk_left = 0;
        h->body_done = false;
        h->icy_metaint = 0;
        h->icy_name[0] = '\0';
        h->icy_br = 0;
        h->pos = h->len = 0;
        location[0] = '\0';

        if (open_once(h, icy, location, err, errlen)) {
            disconnect(h);
            return -1;
        }
        if (h->status >= 300 && h->status < 400 && location[0]) {
            disconnect(h);
            char next[HTTP_URL_MAX];
            resolve_location(h->url, location, next, sizeof(next));
            strmemccpy(h->url, next, sizeof(h->url));
            continue;
        }
        if (h->status != 200) {
            snprintf(err, errlen, "Server error %d", h->status);
            disconnect(h);
            return -1;
        }
        return 0;
    }
    snprintf(err, errlen, "Too many redirects");
    disconnect(h);
    return -1;
}

int http_read(struct http *h, void *out, size_t len, int timeout_ms)
{
    if (h->body_done || len == 0)
        return 0;
    if (h->chunked && h->chunk_left == 0) {
        char line[32];
        int n = read_line_http(h, line, sizeof(line));
        if (n == 0)     /* the CRLF after the previous chunk */
            n = read_line_http(h, line, sizeof(line));
        if (n < 0)
            return n;
        h->chunk_left = strtol(line, NULL, 16);
        if (h->chunk_left <= 0) {
            h->body_done = true;
            return 0;
        }
    }
    int r = buf_fill(h, timeout_ms);
    if (r <= 0) {
        if (r == 0)
            h->body_done = true;
        return r;
    }
    size_t n = MIN(len, (size_t)r);
    if (h->chunked)
        n = MIN(n, (size_t)h->chunk_left);
    if (h->content_length >= 0)
        n = MIN(n, (size_t)h->content_length);
    memcpy(out, h->buf + h->pos, n);
    h->pos += n;
    if (h->chunked)
        h->chunk_left -= n;
    if (h->content_length >= 0) {
        h->content_length -= n;
        if (h->content_length == 0)
            h->body_done = true;
    }
    return (int)n;
}

void http_close(struct http *h)
{
    disconnect(h);
}

char *http_get(const char *url, size_t max, size_t *len, char *err,
               size_t errlen)
{
    if (errlen)
        err[0] = '\0';
    struct http *h = net_calloc(1, sizeof(*h));
    if (!h) {
        snprintf(err, errlen, "Out of memory");
        return NULL;
    }
    if (http_open(h, url, false, err, errlen)) {
        net_free(h);
        return NULL;
    }
    size_t cap = h->content_length > 0 ? (size_t)h->content_length + 1 : 32768;
    cap = MIN(cap, max + 1);
    char *body = net_malloc(cap);
    size_t n = 0;
    while (body) {
        if (n + 1 >= cap) {
            if (cap >= max + 1)
                break;
            size_t ncap = MIN(cap * 2, max + 1);
            char *nb = net_realloc(body, ncap);
            if (!nb) {
                net_free(body);
                body = NULL;
                break;
            }
            body = nb;
            cap = ncap;
        }
        int r = http_read(h, body + n, cap - 1 - n, 15000);
        if (r == 0)
            break;
        if (r < 0) {
            net_free(body);
            body = NULL;
            snprintf(err, errlen, r == NET_ERR_TIMEOUT ? "The server is too slow"
                                                       : "Connection lost");
            break;
        }
        n += r;
    }
    http_close(h);
    net_free(h);
    if (!body) {
        if (!err[0])
            snprintf(err, errlen, "Out of memory");
        return NULL;
    }
    body[n] = '\0';
    if (len)
        *len = n;
    return body;
}

void http_urlencode(const char *s, char *out, size_t outlen)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; *s && o + 4 < outlen; ++s) {
        unsigned char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            out[o++] = c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15];
        }
    }
    out[o] = '\0';
}
