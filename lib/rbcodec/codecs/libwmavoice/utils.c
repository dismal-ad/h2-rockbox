/*
 * utils for libavcodec
 * Copyright (c) 2001 Fabrice Bellard
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * utils.
 */

//#include "libavutil/avstring.h"
//#include "libavutil/integer.h"
//#include "libavutil/crc.h"
//#include "libavutil/pixdesc.h"
//#include "libavcore/imgutils.h"
#include "avcodec.h"
//#include "dsputil.h"
//#include "opt.h"
//#include "imgconvert.h"
//#include "audioconvert.h"
#include "internal.h"
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>

void av_log_missing_feature(void *avc, const char *feature, int want_sample)
{
    av_log(avc, AV_LOG_WARNING, "%s not implemented. Update your FFmpeg "
            "version to the newest one from SVN. If the problem still "
            "occurs, it means that your file has a feature which has not "
            "been implemented.", feature);
    if(want_sample)
        av_log_ask_for_sample(avc, NULL);
    else
        av_log(avc, AV_LOG_WARNING, "\n");
}

void av_log_ask_for_sample(void *avc, const char *msg)
{
    if (msg)
        av_log(avc, AV_LOG_WARNING, "%s ", msg);
    av_log(avc, AV_LOG_WARNING, "If you want to help, upload a sample "
            "of this file to ftp://upload.ffmpeg.org/MPlayer/incoming/ "
            "and contact the ffmpeg-devel mailing list.\n");
}
