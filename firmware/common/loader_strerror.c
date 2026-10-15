/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2013 Marcin Bukat
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

#include "loader_strerror.h"

char *loader_strerror(enum error_t error)
{
    switch(error)
    {
    case EFILE_EMPTY:
        return "File empty";
    case EFILE_NOT_FOUND:
        return "File not found";
    case EREAD_CHKSUM_FAILED:
        return "Read failed (chksum)";
    case EREAD_MODEL_FAILED:
        return "Read failed (model)";
    case EREAD_IMAGE_FAILED:
        return "Read failed (image)";
    case EBAD_CHKSUM:
        return "Bad checksum";
    case EFILE_TOO_BIG:
        return "File too big";
    case EINVALID_FORMAT:
        return "Invalid file format";
    default:
        return "Unknown error";
    }
}
