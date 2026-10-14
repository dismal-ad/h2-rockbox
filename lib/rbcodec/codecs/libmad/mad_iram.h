/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 Dave Chapman
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

/* Define how IRAM is used on the various targets.  Note that this
   file is included by both .c and .S files so must not contain any C
   code. 
*/

#ifndef _LIBMAD_IRAM_H
#define _LIBMAD_IRAM_H

#include "config.h"

#define ICODE_SECTION_MPA_ARM .text
#define IBSS_SECTION_MPA_ARM .bss
#define ICODE_ATTR_MPA_SYNTH
#define ICONST_ATTR_MPA_HUFFMAN

#endif /* MAD_IRAM_H */
