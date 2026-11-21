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
#include "file.h"
#include "dir.h"
#include "misc.h"
#include "string-extra.h"
#include "net/net.h"
#include "net/http.h"
#include "radio_dir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************************
 * JSON: just enough for arrays of flat objects
 ***************************************************************************/

static const char *ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static void put_utf8(char *out, size_t outlen, size_t *o, unsigned cp)
{
    char b[4];
    int n;
    if (cp < 0x80) {
        b[0] = cp; n = 1;
    } else if (cp < 0x800) {
        b[0] = 0xc0 | (cp >> 6); b[1] = 0x80 | (cp & 0x3f); n = 2;
    } else if (cp < 0x10000) {
        b[0] = 0xe0 | (cp >> 12); b[1] = 0x80 | ((cp >> 6) & 0x3f);
        b[2] = 0x80 | (cp & 0x3f); n = 3;
    } else {
        b[0] = 0xf0 | (cp >> 18); b[1] = 0x80 | ((cp >> 12) & 0x3f);
        b[2] = 0x80 | ((cp >> 6) & 0x3f); b[3] = 0x80 | (cp & 0x3f); n = 4;
    }
    if (*o + n < outlen) {
        memcpy(out + *o, b, n);
        *o += n;
    }
}

static unsigned hex4(const char *p)
{
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return 0xffffffff;
    }
    return v;
}

/* p at '"'; returns after the closing quote, or NULL */
static const char *jstring(const char *p, char *out, size_t outlen)
{
    size_t o = 0;
    if (*p != '"')
        return NULL;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            char c = *p;
            if (!c)
                return NULL;
            if (c == 'u') {
                unsigned cp = hex4(p + 1);
                if (cp == 0xffffffff)
                    return NULL;
                p += 4;
                if (cp >= 0xd800 && cp < 0xdc00 && p[1] == '\\' && p[2] == 'u') {
                    unsigned lo = hex4(p + 3);
                    if (lo >= 0xdc00 && lo < 0xe000) {
                        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                        p += 6;
                    }
                }
                if (out)
                    put_utf8(out, outlen, &o, cp);
            } else if (out && o + 1 < outlen) {
                out[o++] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' :
                           c == 'b' ? '\b' : c == 'f' ? '\f' : c;
            }
            p++;
        } else {
            if (out && o + 1 < outlen)
                out[o++] = *p;
            p++;
        }
    }
    if (*p != '"')
        return NULL;
    if (out && outlen)
        out[o] = '\0';
    return p + 1;
}

/* skips any value; NULL on errors */
static const char *jskip(const char *p)
{
    p = ws(p);
    if (*p == '"')
        return jstring(p, NULL, 0);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        while (*p) {
            if (*p == '"') {
                p = jstring(p, NULL, 0);
                if (!p)
                    return NULL;
                continue;
            }
            if (*p == '{' || *p == '[')
                depth++;
            else if (*p == '}' || *p == ']') {
                if (--depth == 0)
                    return p + 1;
            }
            p++;
        }
        return NULL;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    return p;
}

/* Calls field() for each key of each object in a top-level array; object
 * ends with field(NULL). Returns the number of objects or -1. */
typedef void (*json_field_fn)(void *ctx, const char *key, const char *val);

static int json_array_of_objects(const char *p, json_field_fn field, void *ctx,
                                 int max)
{
    static char key[32], val[HTTP_URL_MAX];
    int count = 0;
    p = ws(p);
    if (*p != '[')
        return -1;
    p = ws(p + 1);
    while (*p && *p != ']' && count < max) {
        if (*p != '{')
            return -1;
        p = ws(p + 1);
        while (*p && *p != '}') {
            p = jstring(p, key, sizeof(key));
            if (!p)
                return -1;
            p = ws(p);
            if (*p != ':')
                return -1;
            p = ws(p + 1);
            if (*p == '"') {
                p = jstring(p, val, sizeof(val));
                if (!p)
                    return -1;
                field(ctx, key, val);
            } else if ((*p >= '0' && *p <= '9') || *p == '-') {
                const char *e = jskip(p);
                if (!e)
                    return -1;
                size_t n = MIN((size_t)(e - p), sizeof(val) - 1);
                memcpy(val, p, n);
                val[n] = '\0';
                field(ctx, key, val);
                p = e;
            } else {
                p = jskip(p);
                if (!p)
                    return -1;
            }
            p = ws(p);
            if (*p == ',')
                p = ws(p + 1);
        }
        if (*p != '}')
            return -1;
        field(ctx, NULL, NULL);
        count++;
        p = ws(p + 1);
        if (*p == ',')
            p = ws(p + 1);
    }
    return count;
}

/***************************************************************************
 * Radio Browser
 ***************************************************************************/

static const char *const servers[] = {
    "de1.api.radio-browser.info",
    "de2.api.radio-browser.info",
    "fi1.api.radio-browser.info",
    "nl1.api.radio-browser.info",
    "at1.api.radio-browser.info",
};
static int good_server;

static char *dir_get(const char *path, char *err, size_t errlen)
{
    char url[HTTP_URL_MAX];
#ifdef SIMULATOR
    /* tests: a local directory server */
    const char *local = getenv("RB_RADIO_DIR");
    if (local) {
        snprintf(url, sizeof(url), "https://%s%s", local, path);
        return http_get(url, 512 * 1024, NULL, err, errlen);
    }
#endif
    for (unsigned i = 0; i < ARRAYLEN(servers); ++i) {
        int s = (good_server + i) % ARRAYLEN(servers);
        snprintf(url, sizeof(url), "https://%s%s", servers[s], path);
        char *body = http_get(url, 512 * 1024, NULL, err, errlen);
        if (body) {
            good_server = s;
            return body;
        }
    }
    return NULL;
}

struct st_ctx {
    struct radio_station *out;
    int max, n;
    struct radio_station cur;
    char url[HTTP_URL_MAX];
    bool hls;
};

static void st_field(void *vctx, const char *key, const char *val)
{
    struct st_ctx *c = vctx;
    if (!key) {
        /* object done: keep stations we can play */
        if (c->cur.url[0] == '\0')
            strmemccpy(c->cur.url, c->url, sizeof(c->cur.url));
        bool ok = c->cur.url[0] && c->cur.name[0] && !c->hls;
        const char *k = c->cur.codec;
        if (k[0] && strcasecmp(k, "MP3") && strcasecmp(k, "AAC") &&
            strcasecmp(k, "AAC+") && strcasecmp(k, "OGG") &&
            strcasecmp(k, "OPUS") && strcasecmp(k, "FLAC") &&
            strcasecmp(k, "UNKNOWN"))
            ok = false;
        if (ok && c->n < c->max)
            c->out[c->n++] = c->cur;
        memset(&c->cur, 0, sizeof(c->cur));
        c->url[0] = '\0';
        c->hls = false;
        return;
    }
    if (!strcmp(key, "name")) {
        /* the directory's names often have spaces around them */
        const char *v = val;
        while (*v == ' ' || *v == '\t')
            v++;
        strmemccpy(c->cur.name, v, sizeof(c->cur.name));
        size_t n = strlen(c->cur.name);
        while (n && (c->cur.name[n - 1] == ' ' || c->cur.name[n - 1] == '\t'))
            c->cur.name[--n] = '\0';
    } else if (!strcmp(key, "url_resolved")) {
        strmemccpy(c->cur.url, val, sizeof(c->cur.url));
    } else if (!strcmp(key, "url")) {
        strmemccpy(c->url, val, sizeof(c->url));
    } else if (!strcmp(key, "codec")) {
        strmemccpy(c->cur.codec, val, sizeof(c->cur.codec));
    } else if (!strcmp(key, "bitrate")) {
        c->cur.bitrate = atoi(val);
    } else if (!strcmp(key, "countrycode")) {
        strmemccpy(c->cur.country, val, sizeof(c->cur.country));
    } else if (!strcmp(key, "tags")) {
        strmemccpy(c->cur.tags, val, sizeof(c->cur.tags));
    } else if (!strcmp(key, "hls")) {
        c->hls = atoi(val) != 0;
    }
}

static int stations_from(const char *path, struct radio_station *out, int max,
                         char *err, size_t errlen)
{
    char *body = dir_get(path, err, errlen);
    if (!body)
        return -1;
    struct st_ctx *c = net_calloc(1, sizeof(*c));
    if (!c) {
        net_free(body);
        snprintf(err, errlen, "Out of memory");
        return -1;
    }
    c->out = out;
    c->max = max;
    int r = json_array_of_objects(body, st_field, c, 1000);
    int n = c->n;
    net_free(c);
    net_free(body);
    if (r < 0) {
        snprintf(err, errlen, "Bad answer from the directory");
        return -1;
    }
    return n;
}

#define ST_QUERY "limit=%d&hidebroken=true&order=clickcount&reverse=true"

int radio_dir_search(const char *name, struct radio_station *out, int max,
                     char *err, size_t errlen)
{
    char q[192], path[320];
    http_urlencode(name, q, sizeof(q));
    snprintf(path, sizeof(path), "/json/stations/search?name=%s&" ST_QUERY,
             q, max);
    return stations_from(path, out, max, err, errlen);
}

int radio_dir_popular(struct radio_station *out, int max,
                      char *err, size_t errlen)
{
    char path[128];
    snprintf(path, sizeof(path), "/json/stations/topclick/%d?hidebroken=true",
             max);
    return stations_from(path, out, max, err, errlen);
}

int radio_dir_by_country(const char *code, struct radio_station *out, int max,
                         char *err, size_t errlen)
{
    char q[64], path[192];
    http_urlencode(code, q, sizeof(q));
    snprintf(path, sizeof(path), "/json/stations/bycountrycodeexact/%s?" ST_QUERY,
             q, max);
    return stations_from(path, out, max, err, errlen);
}

int radio_dir_by_tag(const char *tag, struct radio_station *out, int max,
                     char *err, size_t errlen)
{
    char q[96], path[224];
    http_urlencode(tag, q, sizeof(q));
    snprintf(path, sizeof(path), "/json/stations/bytagexact/%s?" ST_QUERY,
             q, max);
    return stations_from(path, out, max, err, errlen);
}

struct cat_ctx {
    struct radio_category *out;
    int max, n;
    struct radio_category cur;
    bool country;
};

static void cat_field(void *vctx, const char *key, const char *val)
{
    struct cat_ctx *c = vctx;
    if (!key) {
        if (c->cur.name[0] && c->cur.count > 0 && c->n < c->max) {
            if (!c->country)
                strmemccpy(c->cur.code, c->cur.name, sizeof(c->cur.code));
            if (c->cur.code[0])
                c->out[c->n++] = c->cur;
        }
        memset(&c->cur, 0, sizeof(c->cur));
        return;
    }
    if (!strcmp(key, "name"))
        strmemccpy(c->cur.name, val, sizeof(c->cur.name));
    else if (!strcmp(key, "iso_3166_1"))
        strmemccpy(c->cur.code, val, sizeof(c->cur.code));
    else if (!strcmp(key, "stationcount"))
        c->cur.count = atoi(val);
}

static int categories(const char *path, bool country,
                      struct radio_category *out, int max,
                      char *err, size_t errlen)
{
    char *body = dir_get(path, err, errlen);
    if (!body)
        return -1;
    struct cat_ctx c;
    memset(&c, 0, sizeof(c));
    c.out = out;
    c.max = max;
    c.country = country;
    int r = json_array_of_objects(body, cat_field, &c, 2000);
    net_free(body);
    if (r < 0) {
        snprintf(err, errlen, "Bad answer from the directory");
        return -1;
    }
    return c.n;
}

int radio_dir_countries(struct radio_category *out, int max,
                        char *err, size_t errlen)
{
    int n = categories("/json/countries?hidebroken=true", true, out, max,
                       err, errlen);
    /* by name */
    for (int i = 1; i < n; ++i) {
        struct radio_category t = out[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(out[j].name, t.name) > 0) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = t;
    }
    return n;
}

int radio_dir_tags(struct radio_category *out, int max,
                   char *err, size_t errlen)
{
    char path[128];
    snprintf(path, sizeof(path),
             "/json/tags?order=stationcount&reverse=true&hidebroken=true&limit=%d",
             max);
    return categories(path, false, out, max, err, errlen);
}

/***************************************************************************
 * .m3u / .pls
 ***************************************************************************/

/* Reads stations from an .m3u (#EXTINF names) or .pls file */
static int load_playlist_file(const char *path, struct radio_station *out,
                              int max)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    char line[HTTP_URL_MAX + 32];
    char name[RADIO_NAME_LEN] = "";
    int n = 0;
    /* .m3u: #EXTINF names; .pls: FileN= and TitleN= */
    while (n < max && read_line(fd, line, sizeof(line)) > 0) {
        char *l = line;
        while (*l == ' ' || *l == '\t')
            l++;
        if (!strncasecmp(l, "#EXTINF:", 8)) {
            char *c = strchr(l, ',');
            strmemccpy(name, c ? c + 1 : "", sizeof(name));
            continue;
        }
        if (!strncasecmp(l, "Title", 5) && strchr(l, '=')) {
            /* .pls title of the previous or next File line */
            int idx = atoi(l + 5);
            const char *t = strchr(l, '=') + 1;
            if (idx >= 1 && idx <= n && !out[idx - 1].name[0])
                strmemccpy(out[idx - 1].name, t, sizeof(out[0].name));
            else
                strmemccpy(name, t, sizeof(name));
            continue;
        }
        const char *url = NULL;
        if (!strncasecmp(l, "File", 4) && strchr(l, '='))
            url = strchr(l, '=') + 1;
        else if (!strncasecmp(l, "http://", 7) || !strncasecmp(l, "https://", 8))
            url = l;
        if (!url)
            continue;
        struct radio_station *st = &out[n++];
        memset(st, 0, sizeof(*st));
        strmemccpy(st->url, url, sizeof(st->url));
        strmemccpy(st->name, name, sizeof(st->name));
        name[0] = '\0';
    }
    close(fd);
    /* nameless entries: the host of the URL */
    for (int i = 0; i < n; ++i) {
        if (out[i].name[0])
            continue;
        const char *h = strstr(out[i].url, "://");
        h = h ? h + 3 : out[i].url;
        strmemccpy(out[i].name, h, sizeof(out[i].name));
        char *slash = strchr(out[i].name, '/');
        if (slash)
            *slash = '\0';
    }
    return n;
}

int radio_fav_load(struct radio_station *out, int max)
{
    return load_playlist_file(RADIO_FAV_FILE, out, max);
}

static void write_playlist_file(const char *path,
                                const struct radio_station *list, int n)
{
    mkdir("/.rockbox/radio");
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    fdprintf(fd, "#EXTM3U\n");
    for (int i = 0; i < n; ++i)
        fdprintf(fd, "#EXTINF:-1,%s\n%s\n", list[i].name, list[i].url);
    close(fd);
}

static void fav_write(const struct radio_station *list, int n)
{
    write_playlist_file(RADIO_FAV_FILE, list, n);
}

void radio_last_save(const struct radio_station *st)
{
    write_playlist_file(RADIO_LAST_FILE, st, 1);
}

bool radio_last_load(struct radio_station *out)
{
    return load_playlist_file(RADIO_LAST_FILE, out, 1) == 1;
}

bool radio_fav_has(const struct radio_station *st)
{
    struct radio_station *list = net_malloc(RADIO_MAX_STATIONS * sizeof(*list));
    if (!list)
        return false;
    int n = radio_fav_load(list, RADIO_MAX_STATIONS);
    bool has = false;
    for (int i = 0; i < n; ++i)
        if (!strcmp(list[i].url, st->url))
            has = true;
    net_free(list);
    return has;
}

void radio_fav_add(const struct radio_station *st)
{
    struct radio_station *list = net_malloc(RADIO_MAX_STATIONS * sizeof(*list));
    if (!list)
        return;
    int n = radio_fav_load(list, RADIO_MAX_STATIONS - 1);
    for (int i = 0; i < n; ++i)
        if (!strcmp(list[i].url, st->url)) {
            net_free(list);
            return;
        }
    list[n++] = *st;
    fav_write(list, n);
    net_free(list);
}

void radio_fav_remove(const struct radio_station *st)
{
    struct radio_station *list = net_malloc(RADIO_MAX_STATIONS * sizeof(*list));
    if (!list)
        return;
    int n = radio_fav_load(list, RADIO_MAX_STATIONS);
    int o = 0;
    for (int i = 0; i < n; ++i)
        if (strcmp(list[i].url, st->url))
            list[o++] = list[i];
    fav_write(list, o);
    net_free(list);
}

static bool is_station_file(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".m3u") || !strcasecmp(dot, ".m3u8") ||
                   !strcasecmp(dot, ".pls"));
}

int radio_files_list(char names[][64], int max)
{
    DIR *d = opendir(RADIO_FILES_DIR);
    if (!d)
        return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        if (e->d_name[0] == '.' || !is_station_file((char *)e->d_name))
            continue;
        strmemccpy(names[n++], (char *)e->d_name, 64);
    }
    closedir(d);
    /* by name */
    for (int i = 1; i < n; ++i) {
        char t[64];
        strmemccpy(t, names[i], sizeof(t));
        int j = i - 1;
        while (j >= 0 && strcasecmp(names[j], t) > 0) {
            strmemccpy(names[j + 1], names[j], 64);
            j--;
        }
        strmemccpy(names[j + 1], t, 64);
    }
    return n;
}

int radio_file_load(const char *name, struct radio_station *out, int max)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), RADIO_FILES_DIR "/%s", name);
    return load_playlist_file(path, out, max);
}
