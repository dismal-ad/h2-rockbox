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

/* lwIP's OS layer on the Rockbox kernel. Rockbox threads only switch when
 * they block, sleep or yield, so the protection is only against interrupt
 * handlers, which never call lwIP: it is a no-op. */

#include <stdarg.h>
#include <stdio.h>
#include "config.h"
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "panic.h"
#include "logf.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/mem.h"

static u32_t ms_to_ticks(u32_t ms)
{
    u32_t t = (ms * HZ + 999) / 1000;
    return t ? t : 1;
}

void sys_init(void)
{
}

u32_t sys_now(void)
{
    return (u32_t)current_tick * (1000 / HZ);
}

sys_prot_t sys_arch_protect(void)
{
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
}

/* --- semaphores --- */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    semaphore_init(&sem->s, 0x7fff, count);
    sem->valid = true;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    sem->valid = false;
}

void sys_sem_signal(sys_sem_t *sem)
{
    semaphore_release(&sem->s);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    long start = current_tick;
    int rc = semaphore_wait(&sem->s, timeout ? (int)ms_to_ticks(timeout)
                                             : TIMEOUT_BLOCK);
    if (rc != OBJ_WAIT_SUCCEEDED)
        return SYS_ARCH_TIMEOUT;
    u32_t ms = (u32_t)(current_tick - start) * (1000 / HZ);
    if (timeout && ms >= timeout)
        ms = timeout - 1;
    return ms;
}

/* --- mutexes --- */

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    mutex_init(&mutex->m);
    mutex->valid = true;
    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    mutex->valid = false;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    mutex_lock(&mutex->m);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    mutex_unlock(&mutex->m);
}

/* --- mailboxes: a ring of pointers counted by two semaphores --- */

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (size <= 0)
        size = 16;
    mbox->msgs = mem_calloc(size, sizeof(void *));
    if (!mbox->msgs)
        return ERR_MEM;
    mbox->size = size;
    mbox->head = 0;
    mbox->count = 0;
    semaphore_init(&mbox->filled, size, 0);
    semaphore_init(&mbox->space, size, size);
    mbox->valid = true;
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    if (mbox->msgs)
        mem_free(mbox->msgs);
    mbox->msgs = NULL;
    mbox->valid = false;
}

static void mbox_put(sys_mbox_t *mbox, void *msg)
{
    int tail = (mbox->head + mbox->count) % mbox->size;
    mbox->msgs[tail] = msg;
    mbox->count++;
    semaphore_release(&mbox->filled);
}

static void *mbox_get(sys_mbox_t *mbox)
{
    void *msg = mbox->msgs[mbox->head];
    mbox->head = (mbox->head + 1) % mbox->size;
    mbox->count--;
    semaphore_release(&mbox->space);
    return msg;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    semaphore_wait(&mbox->space, TIMEOUT_BLOCK);
    mbox_put(mbox, msg);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (semaphore_wait(&mbox->space, TIMEOUT_NOBLOCK) != OBJ_WAIT_SUCCEEDED)
        return ERR_MEM;
    mbox_put(mbox, msg);
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    long start = current_tick;
    int rc = semaphore_wait(&mbox->filled, timeout ? (int)ms_to_ticks(timeout)
                                                   : TIMEOUT_BLOCK);
    if (rc != OBJ_WAIT_SUCCEEDED) {
        if (msg)
            *msg = NULL;
        return SYS_ARCH_TIMEOUT;
    }
    void *m = mbox_get(mbox);
    if (msg)
        *msg = m;
    u32_t ms = (u32_t)(current_tick - start) * (1000 / HZ);
    if (timeout && ms >= timeout)
        ms = timeout - 1;
    return ms;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    if (semaphore_wait(&mbox->filled, TIMEOUT_NOBLOCK) != OBJ_WAIT_SUCCEEDED)
        return SYS_MBOX_EMPTY;
    void *m = mbox_get(mbox);
    if (msg)
        *msg = m;
    return 0;
}

/* --- threads: lwIP only starts the tcpip thread --- */

#define LWIP_MAX_THREADS 1

static struct {
    lwip_thread_fn fn;
    void *arg;
} lwip_threads[LWIP_MAX_THREADS];
static int nr_lwip_threads;

static long tcpip_stack[TCPIP_THREAD_STACKSIZE / sizeof(long)];

static void lwip_thread0(void)
{
    lwip_threads[0].fn(lwip_threads[0].arg);
    /* lwIP threads don't return */
    while (1)
        sleep(HZ);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                            void *arg, int stacksize, int prio)
{
    (void)stacksize;
    (void)prio;
    if (nr_lwip_threads >= LWIP_MAX_THREADS)
        panicf("lwIP: too many threads");
    int i = nr_lwip_threads++;
    lwip_threads[i].fn = thread;
    lwip_threads[i].arg = arg;
    return create_thread(lwip_thread0, tcpip_stack, sizeof(tcpip_stack), 0,
                         name IF_PRIO(, PRIORITY_SYSTEM) IF_COP(, CPU));
}

/* --- diagnostics --- */

void lwip_port_diag(const char *fmt, ...)
{
#ifdef ROCKBOX_HAS_LOGF
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logf("lwip: %s", buf);
#else
    (void)fmt;
#endif
}

void lwip_port_assert(const char *msg, const char *file, int line)
{
    const char *f = strrchr(file, '/');
    panicf("lwIP: %s (%s:%d)", msg, f ? f + 1 : file, line);
}
