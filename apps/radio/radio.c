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

/* Internet radio.
 *
 * A stream thread downloads the station (resolving playlists and removing
 * the in-band ICY titles) into a ring buffer. Playback sees the stream as
 * a file, /.radio/<n>.<ext>, through the virtual file hook: its first
 * HEAD_SIZE bytes are kept for the metadata parser and reopens, the rest
 * is read from the ring as it arrives. The file pretends to be very large;
 * playback reads it like a long track. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "audio.h"
#include "playlist.h"
#include "string-extra.h"
#include "logf.h"
#include "vfile.h"
#include "metadata.h"
#include "net/net.h"
#include "net/http.h"
#include "radio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RADIO_PREFIX    "/.radio/"
#define RING_SIZE       (1024 * 1024)
#define HEAD_SIZE       (64 * 1024)
#define META_SIZE       (16 * 1024)     /* what the metadata parser sees */
#define FAKE_SIZE       ((off_t)0x7FFF0000)
#define READY_BYTES     (24 * 1024)
#define READ_STALL      (20 * HZ)       /* playback gives up after this */
#define NOT_OURS_TICKS  (20 * HZ)       /* stop if playback moved on */
#define MAX_RECONNECTS  5

enum { Q_START = 1 };

/* --- state shared with the UI and playback --- */

static struct radio_station station;
static volatile enum radio_state rstate = RADIO_IDLE;
static char fail_text[128];
static volatile unsigned gen;           /* stream generation */
static volatile bool stop_req;
static volatile bool thread_busy;
static char cur_path[32];
static char cur_ext[8];
static char now_title[160];
static volatile unsigned title_gen;
static int stream_br;                   /* kbit/s */
static bool playback_started;

/* --- buffers --- */

static uint8_t *ring;
static uint8_t *head;
static volatile off_t total;            /* bytes written so far */
static volatile bool stream_eof;

struct rfile {
    bool used;
    bool meta;
    unsigned gen;
    off_t pos;
};
static struct rfile files[VFILE_FD_COUNT];

static struct event_queue radio_q;
static long radio_stack[(24 * 1024) / sizeof(long)];

/***************************************************************************
 * Virtual file
 ***************************************************************************/

static bool path_is_current(const char *path)
{
    return cur_path[0] && !strcmp(path, cur_path);
}

bool radio_is_path(const char *path)
{
    return path && !strncmp(path, RADIO_PREFIX, sizeof(RADIO_PREFIX) - 1);
}

static int rv_open(const char *path)
{
    if (!path_is_current(path) || !ring)
        return -1;
    for (int i = 0; i < VFILE_FD_COUNT; ++i) {
        if (!files[i].used) {
            files[i].used = true;
            files[i].meta = false;
            files[i].gen = gen;
            files[i].pos = 0;
            return VFILE_FD_BASE + i;
        }
    }
    return -1;
}

static struct rfile *rv_file(int fd)
{
    int i = fd - VFILE_FD_BASE;
    if (i < 0 || i >= VFILE_FD_COUNT || !files[i].used)
        return NULL;
    return &files[i];
}

static int rv_close(int fd)
{
    struct rfile *f = rv_file(fd);
    if (!f)
        return -1;
    f->used = false;
    return 0;
}

static off_t rv_filesize(int fd)
{
    struct rfile *f = rv_file(fd);
    if (!f)
        return -1;
    return f->meta ? META_SIZE : FAKE_SIZE;
}

static off_t rv_lseek(int fd, off_t off, int whence)
{
    struct rfile *f = rv_file(fd);
    if (!f)
        return -1;
    off_t base = whence == SEEK_CUR ? f->pos :
                 whence == SEEK_END ? rv_filesize(fd) : 0;
    off_t p = base + off;
    if (p < 0)
        return -1;
    f->pos = p;
    return p;
}

/* Lowest position a reader still needs, for the writer */
static off_t min_reader_pos(void)
{
    off_t m = -1;
    for (int i = 0; i < VFILE_FD_COUNT; ++i) {
        if (files[i].used && !files[i].meta && files[i].gen == gen &&
            (m < 0 || files[i].pos < m))
            m = files[i].pos;
    }
    /* before playback opens the stream, keep everything for it */
    if (m < 0)
        m = HEAD_SIZE;
    return MAX(m, (off_t)HEAD_SIZE);
}

static ssize_t rv_read(int fd, void *buf, size_t n)
{
    struct rfile *f = rv_file(fd);
    if (!f || f->gen != gen)
        return -1;

    off_t p = f->pos;
    if (f->meta) {
        if (p >= META_SIZE)
            return 0;
        n = MIN(n, (size_t)(META_SIZE - p));
    }
    if (p >= FAKE_SIZE)
        return 0;
    /* far beyond the live position (a parser looking at the "end") */
    if (p > total + RING_SIZE)
        return 0;

    long start = current_tick;
    while (p >= total) {
        if (stream_eof || stop_req || f->gen != gen)
            return 0;
        if (TIME_AFTER(current_tick, start + READ_STALL))
            return -1;
        sleep(1);
    }

    size_t avail = (size_t)(total - p);
    n = MIN(n, avail);
    if (p < HEAD_SIZE) {
        n = MIN(n, (size_t)(HEAD_SIZE - p));
        memcpy(buf, head + p, n);
    } else {
        if (p < total - RING_SIZE)
            return -1;  /* overwritten */
        size_t off = (size_t)(p % RING_SIZE);
        size_t first = MIN(n, (size_t)(RING_SIZE - off));
        memcpy(buf, ring + off, first);
        if (first < n)
            memcpy((uint8_t *)buf + first, ring, n - first);
    }
    f->pos = p + n;
    return (ssize_t)n;
}

static bool rv_exists(const char *path)
{
    return path_is_current(path);
}

static const struct vfile_ops radio_vfile = {
    .prefix = RADIO_PREFIX,
    .open = rv_open,
    .close = rv_close,
    .read = rv_read,
    .lseek = rv_lseek,
    .filesize = rv_filesize,
    .exists = rv_exists,
};

/***************************************************************************
 * Metadata: the parser sees only the start; then no length, the name
 ***************************************************************************/

static bool meta_is_stream(const char *path)
{
    return radio_is_path(path);
}

static void meta_begin(int fd)
{
    struct rfile *f = rv_file(fd);
    if (f)
        f->meta = true;
}

static bool meta_end(struct mp3entry *id3, const char *path, bool ok)
{
    (void)path;
    if (!ok) {
        /* MP3 and AAC decoders find their way in the stream themselves */
        if (!strcmp(cur_ext, "mp3"))
            id3->codectype = AFMT_MPA_L3;
        else if (!strcmp(cur_ext, "aac"))
            id3->codectype = AFMT_AAC_BSF;
        else
            return false;
        if (!id3->frequency)
            id3->frequency = 44100;
        id3->first_frame_offset = 0;
    }
    id3->length = 0;
    id3->filesize = FAKE_SIZE;
    id3->elapsed = 0;
    id3->offset = 0;
    if (stream_br)
        id3->bitrate = stream_br;
    id3->vbr = false;
    id3->has_toc = false;

    /* the strings must live inside the entry: copies move them along */
    size_t n = strlen(station.name);
    n = MIN(n, (size_t)ID3V2_BUF_SIZE - 64);
    memcpy(id3->id3v2buf, station.name, n);
    id3->id3v2buf[n] = '\0';
    id3->title = id3->id3v2buf;
    const char *album = "Internet radio";
    strmemccpy(id3->id3v2buf + n + 1, album, ID3V2_BUF_SIZE - n - 1);
    id3->album = id3->id3v2buf + n + 1;
    id3->artist = NULL;
    id3->albumartist = NULL;
    id3->genre_string = NULL;
    id3->track_string = NULL;
    id3->disc_string = NULL;
    id3->year_string = NULL;
    id3->composer = NULL;
    id3->comment = NULL;
    id3->grouping = NULL;
    id3->mb_track_id = NULL;
    id3->tracknum = 0;
    id3->discnum = 0;
    id3->year = 0;
#ifdef HAVE_ALBUMART
    id3->has_embedded_albumart = false;
#endif
    return true;
}

static const struct metadata_stream_hooks radio_meta = {
    .is_stream = meta_is_stream,
    .begin = meta_begin,
    .end = meta_end,
};

/***************************************************************************
 * Stream thread
 ***************************************************************************/

static void fail(const char *fmt, const char *arg)
{
    snprintf(fail_text, sizeof(fail_text), fmt, arg ? arg : "");
    rstate = RADIO_FAILED;
    logf("radio: %s", fail_text);
}

static bool cancelled(void)
{
    return stop_req;
}

/* Playback still plays (or is paused on) our stream */
static bool playback_is_ours(void)
{
    if (!(audio_status() & AUDIO_STATUS_PLAY))
        return false;
    struct mp3entry *id3 = audio_current_track();
    return id3 && path_is_current(id3->path);
}

/* first of the characters in set, or NULL (no strpbrk in Rockbox's libc) */
static char *find_any(char *s, const char *set)
{
    for (; *s; ++s)
        if (strchr(set, *s))
            return s;
    return NULL;
}

static bool ends_with_ci(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    if (b > a)
        return false;
    return !strcasecmp(s + a - b, suffix);
}

/* Path of the URL without the query */
static bool url_path_ends(const char *url, const char *suffix)
{
    char tmp[HTTP_URL_MAX];
    strmemccpy(tmp, url, sizeof(tmp));
    char *q = find_any(tmp, "?#");
    if (q)
        *q = '\0';
    return ends_with_ci(tmp, suffix);
}

static bool is_playlist(const struct http *h)
{
    const char *t = h->content_type;
    if (strcasestr(t, "scpls") || strcasestr(t, "mpegurl") ||
        strcasestr(t, "pls+xml"))
        return true;
    bool texty = !t[0] || strcasestr(t, "text/") ||
                 strcasestr(t, "octet-stream");
    return texty && (url_path_ends(h->url, ".pls") ||
                     url_path_ends(h->url, ".m3u") ||
                     url_path_ends(h->url, ".m3u8"));
}

/* The first stream URL of a .pls or .m3u */
static bool playlist_first_url(char *body, char *out, size_t outlen,
                               bool *hls)
{
    *hls = strstr(body, "#EXT-X-") != NULL;
    char *line = body;
    while (line && *line) {
        char *next = find_any(line, "\r\n");
        if (next) {
            *next++ = '\0';
            while (*next == '\r' || *next == '\n')
                next++;
        }
        while (*line == ' ' || *line == '\t')
            line++;
        char *url = NULL;
        if (!strncasecmp(line, "File", 4) && strchr(line, '='))
            url = strchr(line, '=') + 1;
        else if (!strncasecmp(line, "http://", 7) ||
                 !strncasecmp(line, "https://", 8))
            url = line;
        if (url) {
            while (*url == ' ')
                url++;
            strmemccpy(out, url, outlen);
            return true;
        }
        line = next;
    }
    return false;
}

/* Format from the type, or from the first bytes */
static const char *stream_ext(const char *type, const uint8_t *b, size_t n)
{
    if (n >= 4 && !memcmp(b, "OggS", 4)) {
        for (size_t i = 0; i + 8 <= n && i < 512; ++i)
            if (!memcmp(b + i, "OpusHead", 8))
                return "opus";
        return "ogg";
    }
    if (strcasestr(type, "mpeg") || strcasestr(type, "mp3"))
        return "mp3";
    if (strcasestr(type, "aac") || strcasestr(type, "aacp"))
        return "aac";
    if (strcasestr(type, "flac"))
        return "flac";
    if (n >= 3 && !memcmp(b, "ID3", 3))
        return "mp3";
    for (size_t i = 0; i + 1 < n && i < 4096; ++i) {
        if (b[i] == 0xff && (b[i + 1] & 0xf0) == 0xf0)
            return (b[i + 1] & 0x06) == 0 ? "aac" : "mp3";
    }
    return NULL;
}

/* --- ICY: titles inside the stream every icy_metaint bytes --- */

struct icy {
    int metaint;
    int left;           /* audio bytes until the next title block */
    int meta_len;       /* > 0 while reading a title block */
    int meta_pos;
    char meta[4081];
};

static void set_title_from_meta(const char *m)
{
    const char *p = strstr(m, "StreamTitle='");
    if (!p)
        return;
    p += 13;
    const char *e = strstr(p, "';");
    if (!e)
        e = p + strlen(p);
    char t[sizeof(now_title)];
    size_t n = MIN((size_t)(e - p), sizeof(t) - 1);
    memcpy(t, p, n);
    t[n] = '\0';

    /* not UTF-8: take it as Latin-1 */
    bool utf8 = true;
    for (size_t i = 0; i < n && utf8; ) {
        unsigned char c = t[i];
        int k = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1 :
                (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
        if (k < 0)
            utf8 = false;
        for (int j = 1; j <= k && utf8; ++j)
            if (i + j >= n || (t[i + j] & 0xc0) != 0x80)
                utf8 = false;
        i += k + 1;
    }
    if (!utf8) {
        char u[sizeof(now_title)];
        size_t o = 0;
        for (size_t i = 0; i < n && o + 3 < sizeof(u); ++i) {
            unsigned char c = t[i];
            if (c < 0x80) {
                u[o++] = c;
            } else {
                u[o++] = 0xc0 | (c >> 6);
                u[o++] = 0x80 | (c & 0x3f);
            }
        }
        u[o] = '\0';
        strmemccpy(t, u, sizeof(t));
    }
    if (strcmp(t, now_title)) {
        strmemccpy(now_title, t, sizeof(now_title));
        title_gen++;
    }
}

/* Writes audio bytes, waiting while the readers are a ring behind */
static bool ring_write(const uint8_t *d, size_t n)
{
    while (n) {
        long waited = current_tick;
        while (total + 1 - min_reader_pos() >= RING_SIZE) {
            if (stop_req)
                return false;
            /* nobody reads: playback is gone */
            if (playback_started && !playback_is_ours() &&
                TIME_AFTER(current_tick, waited + NOT_OURS_TICKS))
                return false;
            sleep(1);
        }
        size_t space = RING_SIZE - (size_t)(total - min_reader_pos()) - 1;
        size_t k = MIN(n, space);
        size_t off = (size_t)(total % RING_SIZE);
        k = MIN(k, (size_t)(RING_SIZE - off));
        memcpy(ring + off, d, k);
        if (total < HEAD_SIZE) {
            size_t h = MIN(k, (size_t)(HEAD_SIZE - total));
            memcpy(head + total, d, h);
        }
        total += k;
        d += k;
        n -= k;
    }
    return true;
}

static bool icy_feed(struct icy *ic, const uint8_t *d, size_t n)
{
    if (!ic->metaint)
        return ring_write(d, n);
    while (n) {
        if (ic->meta_len > 0) {
            size_t k = MIN(n, (size_t)(ic->meta_len - ic->meta_pos));
            memcpy(ic->meta + ic->meta_pos, d, k);
            ic->meta_pos += k;
            d += k;
            n -= k;
            if (ic->meta_pos == ic->meta_len) {
                ic->meta[ic->meta_len] = '\0';
                set_title_from_meta(ic->meta);
                ic->meta_len = 0;
                ic->left = ic->metaint;
            }
        } else if (ic->left == 0) {
            /* the length byte, in units of 16 */
            ic->meta_len = d[0] * 16;
            ic->meta_pos = 0;
            d++;
            n--;
            if (ic->meta_len == 0)
                ic->left = ic->metaint;
        } else {
            size_t k = MIN(n, (size_t)ic->left);
            if (!ring_write(d, k))
                return false;
            ic->left -= k;
            d += k;
            n -= k;
        }
    }
    return true;
}

/* Opens the station's stream, through playlists. 0 or < 0 (fail_text). */
static int open_stream(struct http *h, const char *url)
{
    char cur[HTTP_URL_MAX];
    char err[96];
    strmemccpy(cur, url, sizeof(cur));
    for (int depth = 0; depth < 3; ++depth) {
        memset(h, 0, sizeof(*h));
        if (http_open(h, cur, true, err, sizeof(err))) {
            fail("%s", err);
            return -1;
        }
        if (!is_playlist(h))
            return 0;

        char *body = net_malloc(64 * 1024 + 1);
        size_t n = 0;
        while (body && n < 64 * 1024) {
            int r = http_read(h, body + n, 64 * 1024 - n, 10000);
            if (r <= 0)
                break;
            n += r;
        }
        http_close(h);
        if (!body) {
            fail("Out of memory%s", "");
            return -1;
        }
        body[n] = '\0';
        bool hls;
        bool found = playlist_first_url(body, cur, sizeof(cur), &hls);
        net_free(body);
        if (hls) {
            fail("HLS streams are not supported%s", "");
            return -1;
        }
        if (!found) {
            fail("No stream in the station's playlist%s", "");
            return -1;
        }
    }
    fail("Too many playlists%s", "");
    return -1;
}

static void run_stream(unsigned my_gen)
{
    static struct http h;
    static struct icy ic;
    static uint8_t chunk[4096];

    rstate = RADIO_NETWORK;
    net_acquire();
    if (!net_wait_up(45 * HZ, cancelled)) {
        if (!stop_req) {
            char st[96];
            fail("%s", net_status_text(st, sizeof(st)));
        }
        net_release();
        return;
    }

    ring = net_malloc(RING_SIZE);
    head = net_malloc(HEAD_SIZE);
    if (!ring || !head) {
        net_free(ring);
        net_free(head);
        ring = head = NULL;
        fail("Out of memory%s", "");
        net_release();
        return;
    }
    total = 0;
    stream_eof = false;
    playback_started = false;
    now_title[0] = '\0';
    title_gen++;

    rstate = RADIO_CONNECTING;
    if (open_stream(&h, station.url)) {
        net_free(ring);
        net_free(head);
        ring = head = NULL;
        net_release();
        return;
    }

    /* the first bytes tell the format if the type doesn't */
    memset(&ic, 0, sizeof(ic));
    ic.metaint = h.icy_metaint;
    ic.left = ic.metaint;
    stream_br = h.icy_br ? h.icy_br : station.bitrate;
    if (h.icy_name[0] && !station.name[0])
        strmemccpy(station.name, h.icy_name, sizeof(station.name));

    rstate = RADIO_BUFFERING;
    const char *ext = NULL;
    int reconnects = 0;
    long last_check = current_tick;
    long not_ours_since = 0;

    while (!stop_req && my_gen == gen) {
        int r = http_read(&h, chunk, sizeof(chunk), 2000);
        if (r > 0) {
            if (!icy_feed(&ic, chunk, r))
                break;
            if (!ext && total >= 512) {
                ext = stream_ext(h.content_type, head, MIN((size_t)total,
                                                           (size_t)HEAD_SIZE));
                if (!ext) {
                    fail("Unsupported stream (%s)", h.content_type[0] ?
                         h.content_type : "unknown type");
                    break;
                }
                strmemccpy(cur_ext, ext, sizeof(cur_ext));
                snprintf(cur_path, sizeof(cur_path), RADIO_PREFIX "%u.%s",
                         my_gen, ext);
            }
            reconnects = 0;
        } else if (r == NET_ERR_TIMEOUT) {
            /* nothing for a while: keep waiting, playback has a buffer */
        } else {
            /* the server closed or the connection broke: MP3 and AAC
             * continue after a reconnect, Ogg can't */
            http_close(&h);
            bool can_resume = ext && (!strcmp(ext, "mp3") || !strcmp(ext, "aac"));
            if (!can_resume || ++reconnects > MAX_RECONNECTS || stop_req)
                break;
            sleep(HZ * reconnects);
            if (!net_wait_up(30 * HZ, cancelled) || open_stream(&h, station.url))
                break;
            ic.metaint = h.icy_metaint;
            ic.left = ic.metaint;
            ic.meta_len = 0;
            continue;
        }

        if (rstate == RADIO_BUFFERING && ext && total >= READY_BYTES)
            rstate = RADIO_PLAYING;     /* radio_poll_start() takes over */

        /* stop once playback has moved on */
        if (TIME_AFTER(current_tick, last_check + HZ)) {
            last_check = current_tick;
            if (playback_started) {
                if (playback_is_ours()) {
                    not_ours_since = 0;
                } else if (!not_ours_since) {
                    not_ours_since = current_tick;
                } else if (TIME_AFTER(current_tick, not_ours_since + NOT_OURS_TICKS)) {
                    break;
                }
            }
        }
    }
    http_close(&h);
    stream_eof = true;
    if (!stop_req && my_gen == gen && rstate == RADIO_BUFFERING)
        fail(ext ? "The stream ended too soon%s" : "Not an audio stream%s", "");

    /* readers may still be inside rv_read: let them leave */
    while (1) {
        bool open = false;
        for (int i = 0; i < VFILE_FD_COUNT; ++i)
            if (files[i].used && files[i].gen == my_gen)
                open = true;
        if (!open || !playback_is_ours())
            break;
        sleep(HZ / 2);
    }
    for (int i = 0; i < VFILE_FD_COUNT; ++i)
        if (files[i].gen == my_gen)
            files[i].used = false;

    if (rstate != RADIO_FAILED)
        rstate = RADIO_IDLE;
    if (my_gen == gen)
        cur_path[0] = '\0';
    net_free(ring);
    net_free(head);
    ring = head = NULL;
    net_release();
}

static void radio_thread(void)
{
    struct queue_event ev;
    while (1) {
        queue_wait(&radio_q, &ev);
        if (ev.id == Q_START) {
            thread_busy = true;
            stop_req = false;
            run_stream((unsigned)ev.data);
            thread_busy = false;
        }
    }
}

/***************************************************************************
 * API
 ***************************************************************************/

void radio_init(void)
{
    vfile_register(&radio_vfile);
    metadata_stream_hooks = &radio_meta;
    queue_init(&radio_q, true);
    create_thread(radio_thread, radio_stack, sizeof(radio_stack), 0, "radio"
                  IF_PRIO(, PRIORITY_BUFFERING) IF_COP(, CPU));
}

void radio_stop(void)
{
    if (playback_is_ours())
        audio_stop();
    stop_req = true;
    /* the thread lets go of everything within a few seconds */
    long end = current_tick + 5 * HZ;
    while (thread_busy && !TIME_AFTER(current_tick, end))
        sleep(HZ / 20);
    cur_path[0] = '\0';
    if (rstate != RADIO_FAILED)
        rstate = RADIO_IDLE;
}

void radio_start(const struct radio_station *st)
{
    radio_stop();
    if (audio_status())
        audio_stop();
    station = *st;
    fail_text[0] = '\0';
    stream_br = st->bitrate;
    rstate = RADIO_NETWORK;
    stop_req = false;
    gen++;
    queue_post(&radio_q, Q_START, gen);
}

bool radio_poll_start(void)
{
    if (rstate != RADIO_PLAYING || playback_started || !cur_path[0])
        return playback_started;
    if (playlist_create(NULL, NULL) < 0 ||
        playlist_insert_track(NULL, cur_path, PLAYLIST_INSERT_LAST,
                              false, true) < 0) {
        fail("Could not start playback%s", "");
        stop_req = true;
        return false;
    }
    playlist_start(0, 0, 0);
    playback_started = true;
    return true;
}

enum radio_state radio_get_state(void)
{
    return rstate;
}

const char *radio_status_text(char *buf, size_t len)
{
    switch (rstate) {
    case RADIO_IDLE:
        snprintf(buf, len, "Stopped");
        break;
    case RADIO_NETWORK: {
        char st[96];
        snprintf(buf, len, "%s", net_status_text(st, sizeof(st)));
        break;
    }
    case RADIO_CONNECTING:
        snprintf(buf, len, "Connecting to %s...", station.name);
        break;
    case RADIO_BUFFERING: {
        long pct = total * 100 / READY_BYTES;
        snprintf(buf, len, "Buffering %ld%%", MIN(pct, 99L));
        break;
    }
    case RADIO_PLAYING:
        snprintf(buf, len, "Playing");
        break;
    case RADIO_FAILED:
        snprintf(buf, len, "%s", fail_text);
        break;
    }
    return buf;
}

const struct radio_station *radio_current(void)
{
    return station.url[0] ? &station : NULL;
}

const char *radio_now_title(void)
{
    return now_title;
}

unsigned radio_title_gen(void)
{
    return title_gen;
}

const char *radio_format_text(char *buf, size_t len)
{
    const char *f = !strcmp(cur_ext, "mp3") ? "MP3" :
                    !strcmp(cur_ext, "aac") ? "AAC" :
                    !strcmp(cur_ext, "ogg") ? "Ogg Vorbis" :
                    !strcmp(cur_ext, "opus") ? "Opus" :
                    !strcmp(cur_ext, "flac") ? "FLAC" : station.codec;
    if (stream_br > 0)
        snprintf(buf, len, "%s %d kbps", f, stream_br);
    else
        snprintf(buf, len, "%s", f);
    return buf;
}
