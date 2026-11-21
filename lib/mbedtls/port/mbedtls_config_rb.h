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

/* Mbed TLS configuration for Rockbox: a TLS 1.2 client for HTTPS streams
 * and the station directory. Certificates are checked against the CA
 * bundle on the card. Memory comes from the network heap, time from SNTP,
 * entropy from the platform (see apps/net/tls.c). */

#ifndef MBEDTLS_CONFIG_RB_H
#define MBEDTLS_CONFIG_RB_H

#include <stdio.h>

/* --- platform --- */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO     snprintf
#define MBEDTLS_PLATFORM_PRINTF_MACRO(...)  0
#define MBEDTLS_PLATFORM_FPRINTF_MACRO(...) 0
#define MBEDTLS_PLATFORM_EXIT_MACRO(x)      do { } while (0)
#define MBEDTLS_PLATFORM_SETBUF_MACRO(...)  do { } while (0)
#define MBEDTLS_PLATFORM_STD_EXIT_SUCCESS   0
#define MBEDTLS_PLATFORM_STD_EXIT_FAILURE   1
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_TIME_ALT
#define MBEDTLS_PLATFORM_GMTIME_R_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_DEPRECATED_REMOVED

/* --- TLS --- */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_RENEGOTIATION
#define MBEDTLS_SSL_SESSION_TICKETS
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_IN_CONTENT_LEN          16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN         4096
#define MBEDTLS_SSL_CIPHERSUITES \
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, \
    MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, \
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, \
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, \
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, \
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, \
    MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256, \
    MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256, \
    MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA

#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED

/* --- certificates --- */
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_RSASSA_PSS_SUPPORT
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_OID_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_PARSE_EC_EXTENDED
#define MBEDTLS_PK_PARSE_EC_COMPRESSED

/* --- crypto --- */
#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_GCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECP_WINDOW_SIZE             4
#define MBEDTLS_ECP_FIXED_POINT_OPTIM       0
#define MBEDTLS_MPI_WINDOW_SIZE             3
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C

#endif /* MBEDTLS_CONFIG_RB_H */
