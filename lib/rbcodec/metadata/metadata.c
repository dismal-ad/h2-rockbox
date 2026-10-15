/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 Dave Chapman
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "string-extra.h"
#include "platform.h"
#include "debug.h"
#include "logf.h"
#include "metadata.h"

#ifdef UTF8PROC_EXPORTS
#include "rbunicode.h"
#endif

#include "metadata_parsers.h"

/* For trailing tag stripping and base audio data types */
#include "buffering.h"

#include "metadata/metadata_common.h"

static bool get_shn_metadata(int fd, struct mp3entry *id3)
{
    /* TODO: read the id3v2 header if it exists */
    id3->vbr = true;
    id3->filesize = ffilesize(fd);
    return skip_id3v2(fd, id3);
}


bool write_metadata_log = false;

const struct afmt_entry audio_formats[AFMT_NUM_CODECS] =
{
 /* NOTE enc_root_fname is ignored here but kept for consistency see get_codec_enc_root_fn() */
    /*AFMT_ENTRY("label", "root_fname", "enc_root_fname", func, "ext_list")*/
    /* Unknown file format */
    [0 ... AFMT_NUM_CODECS-1] =
        AFMT_ENTRY("???", NULL,    "N/A",        NULL, "\0"  ),

    /* MPEG Audio layer 2 */
    [AFMT_MPA_L2] =
        AFMT_ENTRY("MP2",   "mpa",  "N/A",       get_mp3_metadata,   "mpa\0mp2\0"),

    /* MPEG Audio layer 3 */
    [AFMT_MPA_L3] =
        AFMT_ENTRY("MP3",   "mpa",  "mp3_enc",  get_mp3_metadata,   "mp3\0"),

    /* MPEG Audio layer 1 */
    [AFMT_MPA_L1] =
        AFMT_ENTRY("MP1",   "mpa",  "N/A",       get_mp3_metadata,   "mp1\0"),
    /* Audio Interchange File Format */
    [AFMT_AIFF] =
        AFMT_ENTRY("AIFF",  "aiff", "aiff_enc", get_aiff_metadata,  "aiff\0aif\0"),
    /* Uncompressed PCM in a WAV file OR ATRAC3 stream in WAV file (.at3) */
    [AFMT_PCM_WAV] =
        AFMT_ENTRY("WAV",   "wav",  "wav_enc",  get_wave_metadata,  "wav\0"),
    /* Ogg Vorbis */
    [AFMT_OGG_VORBIS] =
        AFMT_ENTRY("Ogg", "vorbis", "N/A",       get_ogg_metadata,   "ogg\0oga\0"),
    /* FLAC */
    [AFMT_FLAC] =
        AFMT_ENTRY("FLAC",  "flac", "N/A",       get_flac_metadata,  "flac\0"),
    /* WavPack */
    [AFMT_WAVPACK] =
        AFMT_ENTRY("WV","wavpack","wavpack_enc",get_wavpack_metadata,"wv\0"),
    /* Apple Lossless Audio Codec */
    [AFMT_MP4_ALAC] =
        AFMT_ENTRY("ALAC",  "alac", "N/A",       get_mp4_metadata,   "m4a\0m4b\0"),
    /* Advanced Audio Coding in M4A container */
    [AFMT_MP4_AAC] =
        AFMT_ENTRY("AAC",   "aac",  "N/A",       get_mp4_metadata,   "mp4\0"),
    /* Shorten */
    [AFMT_SHN] =
        AFMT_ENTRY("SHN","shorten", "N/A",       get_shn_metadata,   "shn\0"),
    /* APE (Monkey's Audio) */
    [AFMT_APE] =
        AFMT_ENTRY("APE",   "ape",  "N/A",       get_monkeys_metadata,"ape\0mac\0"),
    /* WMA (WMAV1/V2 in ASF) */
    [AFMT_WMA] =
        AFMT_ENTRY("WMA",   "wma",  "N/A",       get_asf_metadata,"wma\0wmv\0asf\0"),
    /* WMA Professional in ASF */
    [AFMT_WMAPRO] =
        AFMT_ENTRY("WMAPro","wmapro","N/A",      NULL,            "wma\0wmv\0asf\0"),
    /* SMAF (Synthetic music Mobile Application Format) */
    [AFMT_SMAF] =
        AFMT_ENTRY("SMAF",  "smaf", "N/A",       get_smaf_metadata,  "mmf\0"),
    /* Sun Audio file */
    [AFMT_AU] =
        AFMT_ENTRY("AU",    "au",   "N/A",       get_au_metadata,    "au\0snd\0"),
    /* VOX (Dialogic telephony file formats) */
    [AFMT_VOX] =
        AFMT_ENTRY("VOX",   "vox",  "N/A",       get_vox_metadata,   "vox\0"),
    /* Wave64 */
    [AFMT_WAVE64] =
        AFMT_ENTRY("WAVE64","wav64","N/A",       get_wave64_metadata,"w64\0"),
    /* Advanced Audio Coding High Efficiency in M4A container */
    [AFMT_MP4_AAC_HE] =
        AFMT_ENTRY("AAC-HE","aac",  "N/A",       get_mp4_metadata,   "mp4\0"),
    /* Opus */
    [AFMT_OPUS] =
        AFMT_ENTRY("Opus", "opus", "N/A", get_ogg_metadata,   "opus\0"),
    /* AAC bitstream format */
    [AFMT_AAC_BSF] =
        AFMT_ENTRY("AAC", "aac_bsf", "N/A", get_aac_metadata,   "aac\0"),
    [AFMT_DSD] =
        AFMT_ENTRY("DSD", "dsd", "N/A", get_dsd_metadata, "dsf\0dff\0iso\0"),
};

/* Get the canonical AFMT type */
int get_audio_base_codec_type(int type)
{
    int base_type = type;
    switch (type) {
        case AFMT_MPA_L1:
        case AFMT_MPA_L2:
        case AFMT_MPA_L3:
            base_type = AFMT_MPA_L3;
            break;
        case AFMT_MP4_AAC:
        case AFMT_MP4_AAC_HE:
            base_type = AFMT_MP4_AAC;
            break;
        default:
            break;
    }

    return base_type;
}

static const struct afmt_entry *get_afmt_entry(int type)
{
    if (type < 0 || type >= AFMT_NUM_CODECS)
        type = AFMT_UNKNOWN;

    return &audio_formats[type];
}

const char * get_codec_string(int type)
{
    return get_afmt_entry(type)->label;
}

/* Get the basic audio type */
bool rbcodec_format_is_atomic(int afmt)
{
    /* only the (removed) chiptune/tracker formats had to be loaded whole */
    (void)afmt;
    return false;
}

/* Is the format allowed to buffer starting at some offset other than 0
   or first frame only for resume purposes? */
bool format_buffers_with_offset(int afmt)
{
    switch (afmt)
    {
    case AFMT_MPA_L1:
    case AFMT_MPA_L2:
    case AFMT_MPA_L3:
    case AFMT_WAVPACK:
        /* Format may be loaded at the first needed frame */
        return true;
    default:
        /* Format must be loaded from the beginning of the file
           (does not imply 'atomic', while 'atomic' implies 'no offset') */
        return false;
    }
}

/* Simple file type probing by looking at the filename extension. */
unsigned int probe_file_format(const char *filename)
{
    const char *suffix;
    unsigned int i;

    suffix = strrchr(filename, '.');

    if (suffix == NULL)
    {
        return AFMT_UNKNOWN;
    }

    /* skip '.' */
    suffix++;

    for (i = 1; i < AFMT_NUM_CODECS; i++)
    {
        /* search extension list for type */
        const char *ext = audio_formats[i].ext_list;

        do
        {
            if (strcasecmp(suffix, ext) == 0)
            {
                return i;
            }

            ext += strlen(ext) + 1;
        }
        while (*ext != '\0');
    }

    return AFMT_UNKNOWN;
}

/* Get metadata for track - return false if parsing showed problems with the
 * file that would prevent playback. supply a filedescriptor <0 and the file will be opened
 * and closed automatically within the get_metadata call
 * audio_fmt is AFMT_ enum provided by probe_file_format(trackname),
 * get_metadata_ex & afmt allow flags to change the way get_metadata behaves
 * METADATA_EXCLUDE_ID3_PATH  won't copy filename path to the id3 path buffer
 * METADATA_CLOSE_FD_ON_EXIT closes the open filedescriptor on exit
 * METADATA_EXCLUDE_NORMALIZE won't utf8 normalize the string type id3 entries
 */
bool get_metadata_afmt(struct mp3entry* id3, int fd, const char* trackname, int audio_fmt, int flags)
{
    bool success = true;
    const struct afmt_entry *entry;
    int logfd = 0;
    DEBUGF("Read metadata for %s\n", trackname);
    const char *res_str = "\n";

    /* Clear the mp3entry to avoid having bogus pointers appear */
    wipe_mp3entry(id3);

    if (fd < 0)
    {
        fd = open(trackname, O_RDONLY);
        flags |= METADATA_CLOSE_FD_ON_EXIT;
        if (fd < 0)
        {
            DEBUGF("Error opening %s\n", trackname);
            res_str = " - [Error opening]\n";
            success = false;
            goto log_on_exit;
        }
    }

    /* Take our best guess at the codec type based on file extension */
    id3->codectype = audio_fmt; /* use probe_file_format(trackname); */

    /* default values for embedded cuesheets */
    id3->has_embedded_cuesheet = false;
    id3->embedded_cuesheet.pos = 0;

    id3->tracknum = -1;

    entry = get_afmt_entry(id3->codectype);

    /* Load codec specific track tag information and confirm the codec type. */
    if (!entry->parse_func)
    {
        DEBUGF("nothing to parse for %s (format %s)\n", trackname, entry->label);
        res_str = " - [No parser]\n";
        success = false;
    }
    else if (!entry->parse_func(fd, id3))
    {
        DEBUGF("parsing %s failed (format: %s)\n", trackname, entry->label);
        res_str = " - [Parser failed]\n";
        success = false;
        wipe_mp3entry(id3); /* ensure the mp3entry is clear */
    }

#ifdef UTF8PROC_EXPORTS
    if (success && (flags & METADATA_EXCLUDE_NORMALIZE) == 0) {
        utf8_normalize(id3->title);
        utf8_normalize(id3->artist);
        utf8_normalize(id3->album);
        utf8_normalize(id3->genre_string);
        utf8_normalize(id3->disc_string);
        utf8_normalize(id3->track_string);
        utf8_normalize(id3->year_string);
        utf8_normalize(id3->composer);
        utf8_normalize(id3->comment);
        utf8_normalize(id3->albumartist);
        utf8_normalize(id3->grouping);
    }
#endif

    if ((flags & METADATA_CLOSE_FD_ON_EXIT))
        close(fd);
    else
        lseek(fd, 0, SEEK_SET);

    if (success && (flags & METADATA_EXCLUDE_ID3_PATH) == 0)
    {
        strlcpy(id3->path, trackname, sizeof(id3->path));
    }
    /* have we successfully read the metadata from the file? */
log_on_exit:
    if (write_metadata_log)
    {
        logfd = open("/metadata.log", O_WRONLY | O_APPEND | O_CREAT, 0666);
        if (logfd >= 0)
        {
            write(logfd, trackname, strlen(trackname));
            write(logfd, res_str, strlen(res_str));
            close(logfd);
        }
    }

    return success;
}

bool get_metadata_ex(struct mp3entry* id3, int fd, const char* trackname, int flags)
{
    return get_metadata_afmt(id3, fd, trackname, probe_file_format(trackname), flags);
}

bool get_metadata(struct mp3entry* id3, int fd, const char* trackname)
{
    return get_metadata_ex(id3, fd, trackname, 0);
}

#define MOVE_ENTRY(x) if (x) x += offset;

void adjust_mp3entry(struct mp3entry *entry, void *dest, const void *orig)
{
    long offset;
    if (orig > dest)
        offset = -((size_t)orig - (size_t)dest);
    else
        offset =  ((size_t)dest - (size_t)orig);

    MOVE_ENTRY(entry->title)
    MOVE_ENTRY(entry->artist)
    MOVE_ENTRY(entry->album)

    if (entry->genre_string > (char*)orig &&
        entry->genre_string < (char*)orig + sizeof(struct mp3entry))
        /* Don't adjust that if it points to an entry of the "genres" array */
        entry->genre_string += offset;

    MOVE_ENTRY(entry->track_string)
    MOVE_ENTRY(entry->disc_string)
    MOVE_ENTRY(entry->year_string)
    MOVE_ENTRY(entry->composer)
    MOVE_ENTRY(entry->comment)
    MOVE_ENTRY(entry->albumartist)
    MOVE_ENTRY(entry->grouping)
    MOVE_ENTRY(entry->mb_track_id)
}

void copy_mp3entry(struct mp3entry *dest, const struct mp3entry *orig)
{
    if (dest != orig)
    {
        memcpy(dest, orig, sizeof(struct mp3entry));
        adjust_mp3entry(dest, dest, orig);
    }
}

/* A shortcut to simplify the common task of clearing the struct */
void wipe_mp3entry(struct mp3entry *id3)
{
    memset(id3, 0, sizeof (struct mp3entry));
}

/* Glean what is possible from the filename alone - does not parse metadata */
void fill_metadata_from_path(struct mp3entry *id3, const char *trackname)
{
    char *p;

    /* Clear the mp3entry to avoid having bogus pointers appear */
    wipe_mp3entry(id3);

    /* Find the filename portion of the path */
    const char *pt = strrchr(trackname, '/');
    strlcpy(id3->id3v2buf, pt ? ++pt : id3->path, ID3V2_BUF_SIZE);

    /* Get the format from the extension and trim it off */
    p = strrchr(id3->id3v2buf, '.');
    if (p)
    {
        /* Might be wrong for container formats - should we bother? */
        id3->codectype = probe_file_format(p);

        if (id3->codectype != AFMT_UNKNOWN)
            *p = '\0';
    }

    /* Set the filename as the title */
    id3->title = id3->id3v2buf;

    /* Copy the path info */
    strlcpy(id3->path, trackname, sizeof (id3->path));
}
