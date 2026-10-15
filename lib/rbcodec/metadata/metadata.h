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

#ifndef _METADATA_H
#define _METADATA_H

#include "platform.h"

#define METADATA_EXCLUDE_ID3_PATH  (0x01) /* don't copy filename path to the id3 path buffer */
#define METADATA_CLOSE_FD_ON_EXIT  (0x02) /* close the filedescriptor when finished */
#define METADATA_EXCLUDE_NORMALIZE (0x04) /* don't normalize id3 text entries */
/* Audio file types. */
/* NOTE: The values of the AFMT_* items are used for the %fc tag in the WPS
         - so new entries MUST be added to the end to maintain compatibility.
 */
enum
{
    AFMT_UNKNOWN = 0,  /* Unknown file format */

    /* start formats */
    AFMT_MPA_L1,       /* MPEG Audio layer 1 */
    AFMT_MPA_L2,       /* MPEG Audio layer 2 */
    AFMT_MPA_L3,       /* MPEG Audio layer 3 */

    AFMT_AIFF,         /* Audio Interchange File Format */
    AFMT_PCM_WAV,      /* Uncompressed PCM in a WAV file */
    AFMT_OGG_VORBIS,   /* Ogg Vorbis */
    AFMT_FLAC,         /* FLAC */
    AFMT_WAVPACK,      /* WavPack */
    AFMT_MP4_ALAC,     /* Apple Lossless Audio Codec */
    AFMT_MP4_AAC,      /* Advanced Audio Coding (AAC) in M4A container */
    AFMT_SHN,          /* Shorten */
    AFMT_APE,          /* Monkey's Audio (APE) */
    AFMT_WMA,          /* WMAV1/V2 in ASF */
    AFMT_WMAPRO,       /* WMA Professional in ASF */
    AFMT_SMAF,         /* SMAF */
    AFMT_AU,           /* Sun Audio file */
    AFMT_VOX,          /* VOX */
    AFMT_WAVE64,       /* Wave64 */
    AFMT_MP4_AAC_HE,   /* Advanced Audio Coding (AAC-HE) in M4A container */
    AFMT_OPUS,         /* Opus (see http://www.opus-codec.org ) */
    AFMT_AAC_BSF,
    AFMT_DSD,

    /* add new formats at any index above this line to have a sensible order -
       specified array index inits are used */
    /* format arrays defined in id3.c */

    AFMT_NUM_CODECS,

};

#define CODEC_EXTENSION "codec"
#define CODEC_PREFIX ""

#define AFMT_ENTRY(label, root_fname, enc_root_fname, func, ext_list) \
    { label, root_fname, func, ext_list }

/** Database of audio formats **/
/* record describing the audio format */
struct mp3entry;
struct afmt_entry
{
    const char *label;      /* format label */
    const char *codec_root_fn; /* root codec filename (sans _enc and .codec) */
    bool (*parse_func)(int fd, struct mp3entry *id3); /* return true on success */
    const char *ext_list;    /* NULL terminated extension
                               list for type with the first as
                               the default for recording */
};

/* database of labels and codecs. add formats per above enum */
extern const struct afmt_entry audio_formats[AFMT_NUM_CODECS];

#if MEMORYSIZE > 8
#define ID3V2_BUF_SIZE 1800
#define ID3V2_MAX_ITEM_SIZE 500
#elif MEMORYSIZE > 2
#define ID3V2_BUF_SIZE 900
#define ID3V2_MAX_ITEM_SIZE 240
#else
#define ID3V2_BUF_SIZE 300
#define ID3V2_MAX_ITEM_SIZE 90
#endif

enum {
    ID3_VER_1_0 = 1,
    ID3_VER_1_1,
    ID3_VER_2_2,
    ID3_VER_2_3,
    ID3_VER_2_4
};

#ifdef HAVE_ALBUMART

#define AA_FLAGS_SHIFT 4
#define AA_CLEAR_FLAGS_MASK ~(-1UL << AA_FLAGS_SHIFT)

enum mp3_aa_type {
    AA_TYPE_UNKNOWN,
    AA_TYPE_BMP,
    AA_TYPE_PNG,
    AA_TYPE_JPG,

    AA_FLAG_ID3_UNSYNC = 1 << (AA_FLAGS_SHIFT + 0),
    AA_FLAG_VORBIS_BASE64 = 1 << (AA_FLAGS_SHIFT + 1),
};

struct mp3_albumart {
    enum mp3_aa_type type;
    int size;
    off_t pos;
};
#endif

enum character_encoding {
    CHAR_ENC_ISO_8859_1 = 1,
    CHAR_ENC_UTF_8,
    CHAR_ENC_UTF_16_LE,
    CHAR_ENC_UTF_16_BE,
};

/* cache embedded cuesheet details */
struct embedded_cuesheet {
    int size;
    off_t pos;
    enum character_encoding encoding;
};

struct mp3entry {
    char path[MAX_PATH];
    char* title;
    char* artist;
    char* album;
    char* genre_string;
    char* disc_string;
    char* track_string;
    char* year_string;
    char* composer;
    char* comment;
    char* albumartist;
    char* grouping;
    int discnum;
    int tracknum;
    int layer;
    int year;
    unsigned char id3version;
    unsigned int codectype;
    unsigned int bitrate;
    unsigned long frequency;
    unsigned long id3v2len;
    unsigned long id3v1len;
    unsigned long first_frame_offset; /* Byte offset to first real MP3 frame.
                                         Used for skipping leading garbage to
                                         avoid gaps between tracks. */
    unsigned long filesize; /* without headers; in bytes */
    unsigned long length;   /* song length in ms */
    unsigned long elapsed;  /* ms played */

    int lead_trim;          /* Number of samples to skip at the beginning */
    int tail_trim;          /* Number of samples to remove from the end */

    /* Added for Vorbis, used by mp4 parser as well. */
    uint64_t samples;  /* number of samples in track */

    /* MP3 stream specific info */
    unsigned long frame_count; /* number of frames in the file (if VBR) */

    /* Used for A52/AC3 */
    unsigned long bytesperframe; /* number of bytes per frame (if CBR) */

    /* Xing VBR fields */
    bool vbr;
    bool has_toc;           /* True if there is a VBR header in the file */
    unsigned char toc[100]; /* table of contents */

    /* Added for AAC HE SBR */
    bool needs_upsampling_correction; /* flag used by aac codec */

    /* these following two fields are used for local buffering */
    char id3v2buf[ID3V2_BUF_SIZE];
    char id3v1buf[4][92];

    /* resume related */
    unsigned long offset;  /* bytes played */
    int index;             /* playlist index */
    bool skip_resume_adjustments;

    unsigned char autoresumable; /* caches result of autoresumable() */

    /* runtime database fields */
    long tagcache_idx;     /* 0=invalid, otherwise idx+1 */
    int rating;
    int score;
    long playcount;
    long lastplayed;
    long playtime;

    /* replaygain support */
    long track_level;   /* holds the level in dB * (1<<FP_BITS) */
    long album_level;
    long track_gain;    /* s19.12 signed fixed point. 0 for no gain. */
    long album_gain;
    long track_peak;    /* s19.12 signed fixed point. 0 for no peak. */
    long album_peak;

#ifdef HAVE_ALBUMART
    bool has_embedded_albumart;
    struct mp3_albumart albumart;
#endif

    /* Cuesheet support */
    bool has_embedded_cuesheet;
    struct embedded_cuesheet embedded_cuesheet;
    struct cuesheet *cuesheet;

    /* Musicbrainz Track ID */
    char* mb_track_id;

    /* For ASF files with MP3 audio stream */
    bool is_asf_stream;
};

unsigned int probe_file_format(const char *filename); /* returns audio_fmt */
bool get_metadata(struct mp3entry* id3, int fd, const char* trackname);
bool get_metadata_ex(struct mp3entry* id3, int fd, const char* trackname, int flags);
bool get_metadata_afmt(struct mp3entry* id3, int fd, const char* trackname, int audio_fmt, int flags);
void adjust_mp3entry(struct mp3entry *entry, void *dest, const void *orig);
void copy_mp3entry(struct mp3entry *dest, const struct mp3entry *orig);
void wipe_mp3entry(struct mp3entry *id3);

void fill_metadata_from_path(struct mp3entry *id3, const char *trackname);
int get_audio_base_codec_type(int type);
const char * get_codec_string(int type);
bool rbcodec_format_is_atomic(int afmt);
bool format_buffers_with_offset(int afmt);

#endif
