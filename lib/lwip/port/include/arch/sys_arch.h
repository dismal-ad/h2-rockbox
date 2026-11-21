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

/* lwIP OS types on Rockbox's kernel objects */

#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include <stdbool.h>
#include "kernel.h"

/* Rockbox's queue event of that name clashes with lwIP's timeout pool */
#undef SYS_TIMEOUT

typedef struct {
    struct semaphore s;
    bool valid;
} sys_sem_t;

typedef struct {
    struct mutex m;
    bool valid;
} sys_mutex_t;

typedef struct {
    void **msgs;
    int size, head, count;
    struct semaphore filled;   /* counts queued messages */
    struct semaphore space;    /* counts free slots */
    bool valid;
} sys_mbox_t;

typedef unsigned int sys_thread_t;
typedef int sys_prot_t;

#define sys_sem_valid(s)        ((s)->valid)
#define sys_sem_set_invalid(s)  ((s)->valid = false)
#define sys_mutex_valid(m)      ((m)->valid)
#define sys_mutex_set_invalid(m) ((m)->valid = false)
#define sys_mbox_valid(b)       ((b)->valid)
#define sys_mbox_set_invalid(b) ((b)->valid = false)

#endif /* LWIP_ARCH_SYS_ARCH_H */
