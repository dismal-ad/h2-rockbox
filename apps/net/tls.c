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

/* TLS on Mbed TLS: the platform hooks (heap, time, entropy), the CA bundle
 * and a client context per connection. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "logf.h"
#include "net.h"
#include "tls.h"
#include <stdio.h>
#include <string.h>

#define MBEDTLS_CONFIG_FILE "mbedtls_config_rb.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#pragma GCC diagnostic pop

/* MBEDTLS_ENTROPY_HARDWARE_ALT: our entropy source */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen);

struct tls {
    mbedtls_ssl_context ssl;
    int sock;
    int timeout_ms;
};

static bool tls_ready;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;
static mbedtls_x509_crt cacert;
static mbedtls_ssl_config conf;
static int nr_ca;

/* --- platform hooks --- */

static void *tls_calloc(size_t n, size_t size)
{
    return net_calloc(n, size);
}

static mbedtls_time_t tls_time(mbedtls_time_t *t)
{
    mbedtls_time_t now = net_time_known() ? (mbedtls_time_t)net_time() : 0;
    if (t)
        *t = now;
    return now;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)current_tick * (1000 / HZ);
}

/* UTC calendar from seconds (days from civil, H. Hinnant) */
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *tm)
{
    long long t = *tt;
    long long days = t / 86400;
    long secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    tm->tm_hour = secs / 3600;
    tm->tm_min = (secs % 3600) / 60;
    tm->tm_sec = secs % 60;
    tm->tm_wday = (int)((days + 4) % 7);
    if (tm->tm_wday < 0)
        tm->tm_wday += 7;

    days += 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
    tm->tm_year = (int)(y - 1900);
    tm->tm_mon = (int)m - 1;
    tm->tm_mday = (int)d;
    tm->tm_yday = 0;
    tm->tm_isdst = 0;
    return tm;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen)
{
    (void)data;
    *olen = net_entropy(output, len);
    return 0;
}

/* Without the time from the network, certificate dates can't be checked */
static int verify_cb(void *data, mbedtls_x509_crt *crt, int depth,
                     uint32_t *flags)
{
    (void)data;
    (void)crt;
    (void)depth;
    if (!net_time_known())
        *flags &= ~(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE);
    return 0;
}

/* --- set up once: RNG and CA bundle --- */

static bool tls_setup(char *err, size_t errlen)
{
    if (tls_ready)
        return true;

    mbedtls_platform_set_calloc_free(tls_calloc, net_free);
    mbedtls_platform_set_time(tls_time);

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    static const char pers[] = "rockbox-radio";
    if (mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)pers, sizeof(pers) - 1)) {
        snprintf(err, errlen, "TLS: no random numbers");
        return false;
    }

    int fd = open(TLS_CA_FILE, O_RDONLY);
    if (fd < 0) {
        snprintf(err, errlen, "Missing %s", TLS_CA_FILE);
        return false;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    char *pem = size > 0 && size < 2 * 1024 * 1024 ? net_malloc(size + 1) : NULL;
    if (!pem || read(fd, pem, size) != size) {
        close(fd);
        net_free(pem);
        snprintf(err, errlen, "Could not read the certificates");
        return false;
    }
    close(fd);
    pem[size] = '\0';

    mbedtls_x509_crt_init(&cacert);
    int bad = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)pem,
                                     size + 1);
    net_free(pem);
    nr_ca = 0;
    for (mbedtls_x509_crt *c = &cacert; c && c->raw.len; c = c->next)
        nr_ca++;
    if (nr_ca == 0) {
        snprintf(err, errlen, "No usable certificates (%d)", bad);
        return false;
    }
    logf("tls: %d CA certificates, %d skipped", nr_ca, bad > 0 ? bad : 0);

    mbedtls_ssl_config_init(&conf);
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT)) {
        snprintf(err, errlen, "TLS setup failed");
        return false;
    }
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
    mbedtls_ssl_conf_verify(&conf, verify_cb, NULL);
    tls_ready = true;
    return true;
}

/* --- I/O on the socket --- */

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    struct tls *t = ctx;
    int n = net_send(t->sock, buf, len);
    return n < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : n;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len, uint32_t tmo)
{
    struct tls *t = ctx;
    (void)tmo;
    int n = net_recv(t->sock, buf, len, t->timeout_ms);
    if (n > 0)
        return n;
    if (n == 0)
        return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    if (n == NET_ERR_TIMEOUT)
        return MBEDTLS_ERR_SSL_TIMEOUT;
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

struct tls *tls_open(int sock, const char *host, char *err, size_t errlen)
{
    if (!tls_setup(err, errlen))
        return NULL;

    struct tls *t = net_calloc(1, sizeof(*t));
    if (!t) {
        snprintf(err, errlen, "Out of memory");
        return NULL;
    }
    t->sock = sock;
    t->timeout_ms = 10000;
    mbedtls_ssl_init(&t->ssl);
    if (mbedtls_ssl_setup(&t->ssl, &conf) ||
        mbedtls_ssl_set_hostname(&t->ssl, host)) {
        snprintf(err, errlen, "TLS: out of memory");
        mbedtls_ssl_free(&t->ssl);
        net_free(t);
        return NULL;
    }
    mbedtls_ssl_set_bio(&t->ssl, t, bio_send, NULL, bio_recv);

    cpu_boost(true);
    int rc;
    do {
        rc = mbedtls_ssl_handshake(&t->ssl);
    } while (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
    cpu_boost(false);

    if (rc) {
        uint32_t flags = mbedtls_ssl_get_verify_result(&t->ssl);
        if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && flags) {
            char why[64];
            mbedtls_x509_crt_verify_info(why, sizeof(why), "", flags);
            char *nl = strchr(why, '\n');
            if (nl)
                *nl = '\0';
            snprintf(err, errlen, "Bad certificate: %s", why);
        } else {
            snprintf(err, errlen, "TLS handshake failed (-0x%04x)", -rc);
        }
        mbedtls_ssl_free(&t->ssl);
        net_free(t);
        return NULL;
    }
    return t;
}

int tls_send(struct tls *t, const void *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        int n = mbedtls_ssl_write(&t->ssl, (const unsigned char *)buf + done,
                                  len - done);
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (n <= 0)
            return -1;
        done += n;
    }
    return (int)done;
}

int tls_recv(struct tls *t, void *buf, size_t len, int timeout_ms)
{
    t->timeout_ms = timeout_ms;
    while (1) {
        int n = mbedtls_ssl_read(&t->ssl, buf, len);
        if (n > 0)
            return n;
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (n == MBEDTLS_ERR_SSL_TIMEOUT)
            return NET_ERR_TIMEOUT;
        if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
            return 0;
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
        if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            continue;
#endif
        return -1;
    }
}

void tls_close(struct tls *t)
{
    if (!t)
        return;
    t->timeout_ms = 500;
    mbedtls_ssl_close_notify(&t->ssl);
    mbedtls_ssl_free(&t->ssl);
    net_free(t);
}
