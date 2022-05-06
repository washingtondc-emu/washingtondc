/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2022 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "log.h"
#include "washdc/error.h"
#include "mount.h"
#include "cdrom.h"

#include "chd.h"

static DEF_ERROR_STRING_ATTR(chd_error)
static DEF_ERROR_INT_ATTR(chd_track)

enum track_mode {
    TRACK_MODE_1_RAW,
    TRACK_MODE_AUDIO
};

struct chd_track {
    bool valid;
    enum track_mode mode;

    unsigned fad_start;    // block address offset
    unsigned ctrl;         // ???

    /*
     * for each track, pad = number of frames before the next frame
     * so it goes at the end of the track not the beginning
     */
    unsigned pad;
    unsigned n_frames; // number of frames

    // offset into the chd file in terms of hunks
    unsigned first_hunk;

    // (n_frames - pad) * frame_len == number of bytes in track

    /*
     * chd pads frames to multiple of 4, so the first fad in the CHD
     * file will not be the same as what is actually the first_fad;
     * so we need to keep track of this for converting addresses
     */
    unsigned chd_fad_start;
};

struct chd_mount {
    chd_file *file;

    struct chd_core_file stream;
    struct chd_track *tracks;
    unsigned n_tracks;
    unsigned frames_per_hunk; // this is generally always 8
    unsigned hunklen;
};

static int
mount_chd_read_toc(struct mount *mount, struct mount_toc *toc,
                   unsigned region);
static enum mount_disc_type chd_get_disc_type(struct mount* mount);
static bool mount_chd_has_hd_region(struct mount *mount);
static unsigned mount_chd_session_count(struct mount *mount);
static unsigned mount_chd_get_leadout(struct mount *mount);
static void chd_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad);
static int chd_read_sector(struct mount *mount, void *buf, unsigned fad);
static void mount_chd_cleanup(struct mount *mount);
static int mount_chd_get_meta(struct mount *mount, struct mount_meta *meta);

static struct mount_ops chd_mount_ops = {
    .session_count = mount_chd_session_count,
    .read_toc = mount_chd_read_toc,
    .read_sector = chd_read_sector,
    .cleanup = mount_chd_cleanup,
    .get_meta = mount_chd_get_meta,
    .get_leadout = mount_chd_get_leadout,
    .has_hd_region = mount_chd_has_hd_region,
    .get_disc_type = chd_get_disc_type,
    .get_session_start = chd_get_session_start
};

static UINT64 wrap_hostfile_fsize(struct chd_core_file* stream);
static size_t wrap_hostfile_fread(void* bufp, size_t size,
                                  size_t count, struct chd_core_file *stream);
static int wrap_hostfile_fclose(struct chd_core_file *stream);
static int wrap_hostfile_fseek(struct chd_core_file *stream, long offs, int whence);

void mount_chd(char const *path) {
    struct chd_mount *mount = calloc(1, sizeof(struct chd_mount));

    if (!mount) {
        error_set_length(sizeof(struct chd_mount));
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    washdc_hostfile stream = washdc_hostfile_open(path, WASHDC_HOSTFILE_READ |
                                                  WASHDC_HOSTFILE_BINARY);
    if (stream == WASHDC_HOSTFILE_INVALID) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    mount->stream.argp = stream;
    mount->stream.fsize = wrap_hostfile_fsize;
    mount->stream.fread = wrap_hostfile_fread;
    mount->stream.fclose = wrap_hostfile_fclose;
    mount->stream.fseek = wrap_hostfile_fseek;

    // CHD_EXPORT const char *chd_error_string(chd_error err);
    chd_error err = chd_open_core_file(&mount->stream, CHD_OPEN_READ, NULL, &mount->file);

    if (err != CHDERR_NONE) {
        error_set_chd_error(chd_error_string(err));
        RAISE_ERROR(ERROR_FILE_IO);
    }

    LOG_INFO("CHD file \"%s\" successfully opened.\n", path);

    chd_header const *hdr = chd_get_header(mount->file);

    LOG_INFO("CHD HEADER:\n");
    LOG_INFO("\tlength: %u\n", (unsigned)hdr->length);
    LOG_INFO("\tversion: %u\n", (unsigned)hdr->version);
    LOG_INFO("\tflags: %08x\n", (unsigned)hdr->flags);
    LOG_INFO("\tcompression[0]: %08x\n", (unsigned)hdr->compression[0]);
    LOG_INFO("\tcompression[1]: %08x\n", (unsigned)hdr->compression[1]);
    LOG_INFO("\tcompression[2]: %08x\n", (unsigned)hdr->compression[2]);
    LOG_INFO("\tcompression[3]: %08x\n", (unsigned)hdr->compression[3]);
    LOG_INFO("\thunkbytes: %u\n", (unsigned)hdr->hunkbytes);
    LOG_INFO("\ttotalhunks: %u\n", (unsigned)hdr->totalhunks);
    LOG_INFO("\tlogicalbytes: %llu\n", (unsigned long long)hdr->logicalbytes);
    LOG_INFO("\tmetaoffset: %llu\n", (unsigned long long)hdr->metaoffset);
    LOG_INFO("\tmapoffset: %llu\n", (unsigned long long)hdr->mapoffset);
    // skip the checksum bullshit
    LOG_INFO("\tunitbytes: %u\n", (unsigned)hdr->unitbytes);
    LOG_INFO("\tunitcount: %llu\n", (unsigned long long)hdr->unitcount);
    LOG_INFO("\thunkcount: %u\n", (unsigned)hdr->hunkcount);
    LOG_INFO("\tmapentrybytes: %u\n", (unsigned)hdr->mapentrybytes);

    if (hdr->hunkbytes % hdr->unitbytes) {
        LOG_ERROR("failure to mount; hunks are not aligned to frames");
        RAISE_ERROR(ERROR_FILE_IO);
    }
    mount->frames_per_hunk = hdr->hunkbytes / hdr->unitbytes;
    mount->hunklen = hdr->hunkbytes;

#define META_MAX 1024
    char **meta_blocks = malloc(sizeof(char*));
    if (!meta_blocks) {
        error_set_length(sizeof(char*));
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }
    *meta_blocks = calloc(META_MAX, sizeof(char));
    if (!*meta_blocks) {
        error_set_length(META_MAX);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }
    unsigned meta_blocks_alloc = 1;

    UINT32 meta_len, meta_tag;
    UINT8 meta_flags;
    // read in meta blocks
    while (chd_get_metadata(mount->file, GDROM_TRACK_METADATA_TAG, meta_blocks_alloc - 1,
                            meta_blocks[meta_blocks_alloc - 1],
                            META_MAX, &meta_len, &meta_tag, &meta_flags) == CHDERR_NONE) {
        if (meta_len >= META_MAX) {
            error_set_length(meta_len);
            RAISE_ERROR(ERROR_OVERFLOW);
        }
        meta_blocks[meta_blocks_alloc - 1][META_MAX - 1] = '\0';
        char **new_meta_blocks = realloc(meta_blocks, sizeof(char*) * ++meta_blocks_alloc);
        if (!new_meta_blocks) {
            error_set_length(sizeof(char*) * meta_blocks_alloc);
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        }
        meta_blocks = new_meta_blocks;
        meta_blocks[meta_blocks_alloc - 1] = calloc(META_MAX, sizeof(char));
        if (!meta_blocks[meta_blocks_alloc - 1]) {
            error_set_length(META_MAX);
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        }
    }

    mount->n_tracks = meta_blocks_alloc - 1;
    mount->tracks = calloc(mount->n_tracks, sizeof(struct chd_track));
    if (!mount->tracks) {
        error_set_length(mount->n_tracks);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    unsigned track_start = 0;
    unsigned hunkno = 0;
    unsigned idx;
    unsigned chd_track_start = 0;
    for (idx = 0; idx < mount->n_tracks; idx++) {
        char const *meta = meta_blocks[idx];
        LOG_INFO("meta string index %u: \"%s\"\n", idx, meta);

#define META_FIELD_MAX 64
        char type[META_FIELD_MAX], subtype[META_FIELD_MAX],
            pgtype[META_FIELD_MAX], pgsub[META_FIELD_MAX];
        int track, frames, pad, pregap, postgap;

        sscanf(meta, "TRACK:%d TYPE:%63s SUBTYPE:%63s FRAMES:%d PAD:%d PREGAP:%d "
               "PGTYPE:%63s PGSUB:%63s POSTGAP:%d", &track, type, subtype, &frames,
               &pad, &pregap, pgtype, pgsub, &postgap);

        type[META_FIELD_MAX - 1] = '\0';
        subtype[META_FIELD_MAX - 1] = '\0';
        pgtype[META_FIELD_MAX - 1] = '\0';
        pgsub[META_FIELD_MAX - 1] = '\0';

        LOG_INFO("\ttrack: %d\n", track);
        LOG_INFO("\tstart: %u\n", track_start);
        LOG_INFO("\ttype: \"%s\"\n", type);
        LOG_INFO("\tsubtype: \"%s\"\n", subtype);
        LOG_INFO("\tframes: %d\n", frames);
        LOG_INFO("\tpad: %d\n", pad);
        LOG_INFO("\tpregap: %d\n", pregap);
        LOG_INFO("\tpgtype: \"%s\"\n", pgtype);
        LOG_INFO("\tpgsub: \"%s\"\n", pgsub);
        LOG_INFO("\tpostgap: %d\n", postgap);
        LOG_INFO("\tfirst hunk: %u\n", hunkno);

        if (track < 1) {
            LOG_ERROR("INVALID TRACK NUMBER %d\n", track);
            error_set_chd_track(track);
            RAISE_ERROR(ERROR_TOO_SMALL);
        }
        int track_idx = track - 1;
        if (track_idx >= mount->n_tracks) {
            LOG_ERROR("INVALID TRACK NUMBER %d\n", track);
            RAISE_ERROR(ERROR_OVERFLOW);
        }

        if (mount->tracks[track_idx].valid) {
            LOG_ERROR("ERROR: TRACK %d IS SPECIFIED TWICE!\n", track);
            RAISE_ERROR(ERROR_FILE_IO);
        }
        mount->tracks[track_idx].valid = true;

        if (strcmp(type, "MODE1_RAW") == 0) {
            mount->tracks[track_idx].mode = TRACK_MODE_1_RAW;
            mount->tracks[track_idx].ctrl = 4;
        } else if (strcmp(type, "AUDIO") == 0) {
            mount->tracks[track_idx].mode = TRACK_MODE_AUDIO;
            mount->tracks[track_idx].ctrl = 0;
        } else {
            LOG_ERROR("UNKNOWN TRACK TYPE \"%s\"\n", type);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        mount->tracks[track_idx].n_frames = frames;
        mount->tracks[track_idx].pad = pad;
        mount->tracks[track_idx].fad_start = cdrom_lba_to_fad(track_start);
        mount->tracks[track_idx].first_hunk = hunkno;
        mount->tracks[track_idx].chd_fad_start = chd_track_start;
        track_start += frames /* + pad */;

        chd_track_start = ((chd_track_start + frames + 3) / 4) * 4;

        hunkno = ((hunkno + frames + 3) / 4) * 4; // pad to multiple of 4
    }

    // make sure all tracks were initialized
    for (idx = 0; idx < mount->n_tracks; idx++) {
        if (!mount->tracks[idx].valid) {
            LOG_ERROR("ERROR: TRACK %u WAS NEVER SPECIFIED!\n", idx + 1);
            RAISE_ERROR(ERROR_FILE_IO);
        }
    }

    // free meta strings
    for (idx = 0; idx < meta_blocks_alloc; idx++)
        free(meta_blocks[idx]);
    free(meta_blocks);

    mount_insert(&chd_mount_ops, mount);
}

static void mount_chd_cleanup(struct mount *mount) {
    struct chd_mount *state = (struct chd_mount*)mount->state;
    chd_close(state->file);
    free(state->tracks);
}

static enum mount_disc_type chd_get_disc_type(struct mount* mount) {
    return DISC_TYPE_GDROM;
}

static bool mount_chd_has_hd_region(struct mount *mount) {
    return true;
}

static unsigned mount_chd_session_count(struct mount *mount) {
    return 1;
}

static unsigned mount_chd_get_leadout(struct mount *mount) {
    struct chd_mount const *chd_mount = (struct chd_mount const*)mount->state;
    unsigned n_tracks = chd_mount->n_tracks;
    struct chd_track const *last_track = chd_mount->tracks + (n_tracks - 1);

    unsigned last_track_len = chd_mount->tracks[n_tracks - 1].n_frames;
    unsigned last_track_offs = cdrom_fad_to_lba(last_track->fad_start);

    return last_track_len + last_track_offs;
}

static void chd_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad) {
    if (session_no != 0)
        RAISE_ERROR(ERROR_INTEGRITY);// there's only one session on a GD-ROM

    struct chd_mount const *chd_mount = (struct chd_mount const*)mount->state;

    if (!chd_mount->n_tracks)
        RAISE_ERROR(ERROR_INTEGRITY);

    *start_track = 0;
    *fad = chd_mount->tracks[0].fad_start;
}

static int
mount_chd_read_toc(struct mount *mount, struct mount_toc *toc,
                   unsigned region) {
    struct chd_mount const *chd = (struct chd_mount const*)mount->state;

    if (region == MOUNT_LD_REGION) {
        // the LD region contains the first two tracks

        // track 1
        toc->tracks[0].fad = chd->tracks[0].fad_start;
        toc->tracks[0].adr = 1;
        toc->tracks[0].ctrl = chd->tracks[0].ctrl;
        toc->tracks[0].valid = true;

        // track 2
        toc->tracks[1].fad = chd->tracks[1].fad_start;
        toc->tracks[1].adr = 1;
        toc->tracks[1].ctrl = chd->tracks[1].ctrl;
        toc->tracks[1].valid = true;

        toc->first_track = 1;
        toc->last_track = 2;
    } else {
        // the HD region contains all tracks but the first two

        unsigned src_track_no;
        for (src_track_no = 3; src_track_no <= chd->n_tracks; src_track_no++) {
            struct chd_track const *src_track = chd->tracks + src_track_no - 1;
            struct mount_track *dst_track = toc->tracks + src_track_no - 1;
            dst_track->fad = src_track->fad_start;
            dst_track->adr = 1;
            dst_track->ctrl = src_track->ctrl;
            dst_track->valid = true;
        }

        toc->first_track = 3;
        toc->last_track = chd->n_tracks;
    }

    unsigned trackno;
    for (trackno = toc->last_track + 1; trackno <= 99; trackno++)
        memset(toc->tracks - 1 + trackno, 0, sizeof(toc->tracks[trackno - 1]));

    /*
     * confession: I don't know what this is yet
     *
     * I *think* it's supposed to point to the first block after the last track
     * in the session, but I need to confirm this.  It's surprisingly hard to
     * find documentation on the lower level aspects of CD even though it's
     * such a ubiquitous media.
     */
    struct chd_track *last_track = chd->tracks + chd->n_tracks - 1;
    toc->leadout = last_track->n_frames + last_track->fad_start;
    toc->leadout_adr = 1;

    return 0;
}

static int chd_read_sector(struct mount *mount, void *buf, unsigned fad) {
    struct chd_mount const *chd_mount = (struct chd_mount const*)mount->state;

    unsigned trackno = 1;
    unsigned frame_idx = 0;
    for (trackno = 1, frame_idx = 0; trackno <= 99; trackno++) {
        if (!chd_mount->tracks[trackno - 1].valid)
            continue;
        unsigned first_fad = chd_mount->tracks[trackno - 1].fad_start;
        unsigned n_fad = chd_mount->tracks[trackno - 1].n_frames;
        unsigned last_fad = first_fad + n_fad - 1;
        LOG_DBG("consider track %u [%u - %u]\n", trackno, first_fad, last_fad);
        if (fad >= first_fad && fad <= last_fad) {
            frame_idx += fad - first_fad;

            unsigned chd_fad = fad - first_fad +
                chd_mount->tracks[trackno - 1].chd_fad_start;
            unsigned hunkno = chd_fad / chd_mount->frames_per_hunk;

            LOG_DBG("****** Select track %u (%u blocks starting from %u, "
                    "hunk %u)\n", trackno, n_fad, first_fad, hunkno);

            char *hunkbuf = malloc(chd_mount->hunklen);
            if (!hunkbuf) {
                error_set_length(chd_mount->hunklen);
                RAISE_ERROR(ERROR_FAILED_ALLOC);
            }

            chd_error err = chd_read(chd_mount->file, hunkno, hunkbuf);
            if (err != CHDERR_NONE) {
                error_set_chd_error(chd_error_string(err));
                RAISE_ERROR(ERROR_FILE_IO);
            }

            unsigned framelen = chd_mount->hunklen / chd_mount->frames_per_hunk;
            char *start =
                hunkbuf + (chd_fad % chd_mount->frames_per_hunk) * framelen +
                CDROM_MODE1_DATA_OFFSET;

            memcpy(buf, start, framelen < 2048 ? framelen : 2048);

            free(hunkbuf);
            return 0;
        }

        // the continue statement above deliberately skips this increment
        frame_idx += chd_mount->tracks[trackno - 1].n_frames;
    }

    return -1;// error
}

static int
mount_chd_get_meta(struct mount *mount, struct mount_meta *meta) {
    struct chd_mount const *chd_mount = (struct chd_mount const*)mount->state;

    char *buf = calloc(2448, 1);
    if (!buf ||
        chd_mount->n_tracks < 3 ||
        chd_read_sector(mount, buf, 45150) != 0) {
        free(buf);
        return -1;
    }

    memset(meta, 0, sizeof(*meta));

    memcpy(meta->hardware, buf, MOUNT_META_HARDWARE_LEN);
    memcpy(meta->maker, buf + 16, MOUNT_META_MAKER_LEN);
    memcpy(meta->dev_info, buf + 32, MOUNT_META_DEV_INFO_LEN);
    memcpy(meta->region, buf + 48, MOUNT_META_REGION_LEN);
    memcpy(meta->periph_support, buf + 56, MOUNT_META_PERIPH_LEN);
    memcpy(meta->product_id, buf + 64, MOUNT_META_PRODUCT_ID_LEN);
    memcpy(meta->product_version, buf + 74, MOUNT_META_PRODUCT_VERSION_LEN);
    memcpy(meta->rel_date, buf + 80, MOUNT_META_REL_DATE_LEN);
    memcpy(meta->boot_file, buf + 96, MOUNT_META_BOOT_FILE_LEN);
    memcpy(meta->company, buf + 112, MOUNT_META_COMPANY_LEN);
    memcpy(meta->title, buf + 128, MOUNT_META_TITLE_LEN);

    return 0;
}

static UINT64 wrap_hostfile_fsize(struct chd_core_file* stream) {
    washdc_hostfile fp = stream->argp;
    /* long pos = washdc_hostfile_tell(fp); */
    washdc_hostfile_seek(fp, 0, WASHDC_HOSTFILE_SEEK_END);
    long sz = washdc_hostfile_tell(fp);
    /* washdc_hostfile_seek(fp, pos, WASHDC_HOSTFILE_SEEK_BEG); */
    return sz;
}

static size_t wrap_hostfile_fread(void* bufp, size_t size,
                                  size_t count, struct chd_core_file *stream) {
    washdc_hostfile fp = stream->argp;
    return washdc_hostfile_read(fp, bufp, size * count);
}

static int wrap_hostfile_fclose(struct chd_core_file *stream) {
    washdc_hostfile fp = stream->argp;
    washdc_hostfile_close(fp);
    return 0;
}

static int wrap_hostfile_fseek(struct chd_core_file *stream, long offs, int whence) {
    washdc_hostfile fp = stream->argp;
    enum washdc_hostfile_seek_origin origin;
    switch (whence) {
    default:
    case SEEK_SET:
        origin = WASHDC_HOSTFILE_SEEK_BEG;
        break;
    case SEEK_END:
        origin = WASHDC_HOSTFILE_SEEK_END;
        break;
    case SEEK_CUR:
        origin = WASHDC_HOSTFILE_SEEK_CUR;
        break;
    }
    return washdc_hostfile_seek(fp, offs, origin);
}
