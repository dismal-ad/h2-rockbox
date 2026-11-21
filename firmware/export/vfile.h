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

/* Virtual files: paths under a prefix served by a provider instead of the
 * disk, read-only. Internet radio uses one so playback reads a stream as
 * if it were a file. */

#ifndef VFILE_H
#define VFILE_H

#include <stdbool.h>
#include <sys/types.h>

#define VFILE_FD_BASE   0x4000
#define VFILE_FD_COUNT  8

struct vfile_ops {
    const char *prefix;     /* e.g. "/.radio/" */
    /* returns VFILE_FD_BASE + n, or < 0 */
    int (*open)(const char *path);
    int (*close)(int fd);
    ssize_t (*read)(int fd, void *buf, size_t n);
    off_t (*lseek)(int fd, off_t offset, int whence);
    off_t (*filesize)(int fd);
    bool (*exists)(const char *path);
};

extern const struct vfile_ops *vfile_ops;

void vfile_register(const struct vfile_ops *ops);

static inline bool vfile_is_fd(int fd)
{
    return vfile_ops && fd >= VFILE_FD_BASE &&
           fd < VFILE_FD_BASE + VFILE_FD_COUNT;
}

bool vfile_is_path(const char *path);

#endif /* VFILE_H */
