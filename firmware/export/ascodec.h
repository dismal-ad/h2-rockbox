/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2008 by Dave Chapman
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

#ifndef _ASCODEC_H
#define _ASCODEC_H

#include "config.h"
#include <stdbool.h>

#include "as3514.h"

# error Only for AS3514!

void ascodec_init(void) INIT_ATTR;
void ascodec_close(void);

void ascodec_lock(void);
void ascodec_unlock(void);

void ascodec_write(unsigned int index, unsigned int value);

int ascodec_read(unsigned int index);

void ascodec_readbytes(unsigned int index, unsigned int len, unsigned char *data);

void ascodec_wait_adc_finished(void);

#if CONFIG_CHARGING
bool ascodec_endofch(void);
bool ascodec_chg_status(void);
void ascodec_monitor_endofch(void);
void ascodec_write_charger(int value);
int ascodec_read_charger(void);
#endif

#endif
