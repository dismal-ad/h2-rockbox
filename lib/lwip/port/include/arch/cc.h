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

/* lwIP compiler and platform glue for Rockbox */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifndef BYTE_ORDER
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN    4321
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

#define LWIP_NO_INTTYPES_H 1
#define U8_F  "u"
#define S8_F  "d"
#define X8_F  "x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"
#define SZT_F "lu"

#define LWIP_NO_CTYPE_H 1

void lwip_port_diag(const char *fmt, ...);
void lwip_port_assert(const char *msg, const char *file, int line);

#define LWIP_PLATFORM_DIAG(x)   do { lwip_port_diag x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) lwip_port_assert((x), __FILE__, __LINE__)

#endif /* LWIP_ARCH_CC_H */
