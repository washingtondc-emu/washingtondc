/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019, 2021 snickerbockers
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

#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "washdc/stringlib.h"
#include "washdc/error.h"
#include "mount.h"
#include "cdrom.h"
#include "log.h"

#include "gdi.h"

struct gdi_mount {
    struct gdi_info meta;
    washdc_hostfile *track_streams;
    size_t *track_lengths; // length of each track, in bytes
};

static void mount_gdi_cleanup(struct mount *mount);
static void cleanup_gdi_info(struct gdi_info *info);
static unsigned mount_gdi_session_count(struct mount *mount);
static int mount_gdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned session_no);
static int mount_read_sector(struct mount *mount, void *buf, unsigned fad);
static enum mount_disc_type gdi_get_disc_type(struct mount* mount);

// return true if this is a legitimate gd-rom; else return false
static bool gdi_validate_fmt(struct gdi_info const *info);

static int mount_gdi_get_meta(struct mount *mount, struct mount_meta *meta);

static unsigned mount_gdi_get_leadout(struct mount *mount);
static void parse_gdi(struct gdi_info *outp, char const *path);

/*
 * dumps the given gdi to stdout, this is really only here for
 * debugging/validation/logging.
 */
static void print_gdi(struct gdi_info const *gdi);

static bool mount_gdi_has_hd_region(struct mount *mount);

static void gdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad);

static struct mount_ops gdi_mount_ops = {
    .session_count = mount_gdi_session_count,
    .read_toc = mount_gdi_read_toc,
    .read_sector = mount_read_sector,
    .cleanup = mount_gdi_cleanup,
    .get_meta = mount_gdi_get_meta,
    .get_leadout = mount_gdi_get_leadout,
    .has_hd_region = mount_gdi_has_hd_region,
    .get_disc_type = gdi_get_disc_type,
    .get_session_start = gdi_get_session_start
};

/* enforce sane limits - MAX_TRACKS might need to be bigger tbh */
#define MAX_TRACKS 64
#define MAX_TRACK_FIELDS 16

/*
 * all gd-rom discs have at a minimum two tracks on
 * the first session and 1 on the third
 */
#define MIN_TRACKS 3

static void parse_gdi(struct gdi_info *outp, char const *path) {
    unsigned track_count = 0;
    struct string whole_file_txt;
    struct gdi_track *tracks = NULL;

    washdc_hostfile stream = washdc_hostfile_open(path, WASHDC_HOSTFILE_READ |
                                                  WASHDC_HOSTFILE_TEXT);
    if (stream == WASHDC_HOSTFILE_INVALID) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    string_init(&whole_file_txt);
    string_load_hostfile(&whole_file_txt, stream);

    washdc_hostfile_close(stream);

    struct string_curs line_curs;
    struct string cur_line;
    string_tok_begin(&line_curs);
    string_init(&cur_line);

    unsigned line_no = 0;
    unsigned n_tracks_loaded = 0;
    while (string_tok_next(&cur_line, &line_curs, string_get(&whole_file_txt),
                           "\n")) {
        if (line_no == 0) {
            // first line - read track count
            track_count = atoi(string_get(&cur_line));

            if (track_count < MIN_TRACKS) {
                error_set_file_path(path);
                error_set_param_name("track_count");
                RAISE_ERROR(ERROR_TOO_SMALL);
            }

            if (track_count > MAX_TRACKS) {
                error_set_file_path(path);
                error_set_param_name("track_count");
                error_set_max_val(MAX_TRACKS);
                RAISE_ERROR(ERROR_TOO_BIG);
            }

            if (!(tracks = calloc(track_count, sizeof(struct gdi_track))))
                RAISE_ERROR(ERROR_FAILED_ALLOC);
        } else {
            // track info
            struct string col_track_no;
            unsigned track_no;
            string_init(&col_track_no);
            string_get_col(&col_track_no, &cur_line, 0, " \t");
            track_no = atoi(string_get(&col_track_no));
            string_cleanup(&col_track_no);

            if (track_no <= 0 || track_no > track_count) {
                error_set_file_path(path);
                error_set_param_name("track number");
                error_set_max_val(track_count);
                RAISE_ERROR(ERROR_TOO_BIG);
            }

            // .gdi files are 1-indexed instead of 0-indexed
            track_no--;

            struct gdi_track *trackp = tracks + track_no;

            if (trackp->valid) {
                error_set_param_name("track number");
                error_set_file_path(path);
                RAISE_ERROR(ERROR_DUPLICATE_DATA);
            }

            struct string fad_start_col;
            string_init(&fad_start_col);
            string_get_col(&fad_start_col, &cur_line, 1, " \t");
            trackp->fad_start =
                cdrom_lba_to_fad(atoi(string_get(&fad_start_col)));
            string_cleanup(&fad_start_col);

            struct string ctrl_col;
            string_init(&ctrl_col);
            string_get_col(&ctrl_col, &cur_line, 2, " \t");
            trackp->ctrl = atoi(string_get(&ctrl_col));
            string_cleanup(&ctrl_col);

            struct string sector_size_col;
            string_init(&sector_size_col);
            string_get_col(&sector_size_col, &cur_line, 3, " \t");
            trackp->sector_size = atoi(string_get(&sector_size_col));
            string_cleanup(&sector_size_col);

            struct string offset_col;
            string_init(&offset_col);
            string_get_col(&offset_col, &cur_line, 4, " \t");
            trackp->offset = atoi(string_get(&offset_col));
            string_cleanup(&offset_col);

            string_init(&trackp->rel_path);
            string_get_col(&trackp->rel_path, &cur_line, 4, " \t");

            // get absolute path
            string_init(&trackp->abs_path);
            string_dirname(&trackp->abs_path, path);
            string_append_char(&trackp->abs_path, washdc_hostfile_pathsep());
            string_append(&trackp->abs_path, string_get(&trackp->rel_path));

            n_tracks_loaded++;
            trackp->valid = true;
        }

        line_no++;
    }

    if (!track_count || n_tracks_loaded != track_count) {
        error_set_file_path(path);
        RAISE_ERROR(ERROR_MISSING_DATA);
    }

    string_cleanup(&cur_line);
    string_cleanup(&whole_file_txt);

    outp->n_tracks = track_count;
    outp->tracks = tracks;
}

static void cleanup_gdi_info(struct gdi_info *info) {
    unsigned track_no;

    for (track_no = 0; track_no < info->n_tracks; track_no++) {
        struct gdi_track *track = info->tracks + track_no;
        string_cleanup(&track->abs_path);
        string_cleanup(&track->rel_path);
    }

    free(info->tracks);
    info->tracks = NULL;
    info->n_tracks = 0;
}

static bool gdi_validate_fmt(struct gdi_info const *info) {
    return info->n_tracks >= 3;
}

static void print_gdi(struct gdi_info const *gdi) {
    LOG_INFO("%u\n", gdi->n_tracks);

    unsigned track_no;
    for (track_no = 0; track_no < gdi->n_tracks; track_no++) {
        struct gdi_track const *trackp = gdi->tracks + track_no;
        LOG_INFO("%u %u %u %u %s %u\n",
               track_no + 1, cdrom_fad_to_lba(trackp->fad_start), trackp->ctrl,
               trackp->sector_size, string_get(&trackp->rel_path), trackp->offset);
    }
}

void mount_gdi(char const *path) {
    struct gdi_mount *mount =
        (struct gdi_mount*)calloc(1, sizeof(struct gdi_mount));

    if (!mount)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    parse_gdi(&mount->meta, path);

    LOG_INFO("about to (attempt to) mount the following image:\n");
    print_gdi(&mount->meta);

    if (!gdi_validate_fmt(&mount->meta))
        RAISE_ERROR(ERROR_INVALID_PARAM);

    mount->track_streams = (washdc_hostfile*)calloc(mount->meta.n_tracks, sizeof(washdc_hostfile));
    if (!mount->track_streams)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    mount->track_lengths = (size_t*)calloc(mount->meta.n_tracks,
                                           sizeof(size_t));
    if (!mount->track_lengths) {
        free(mount->track_streams);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    unsigned track_no;
    for (track_no = 0; track_no < mount->meta.n_tracks; track_no++) {
        struct string const *track_path =
            &mount->meta.tracks[track_no].abs_path;
        mount->track_streams[track_no] =
            washdc_hostfile_open(string_get(track_path),
                                 WASHDC_HOSTFILE_READ | WASHDC_HOSTFILE_BINARY);
        if (!mount->track_streams[track_no]) {
            error_set_file_path(string_get(track_path));
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        if (washdc_hostfile_seek(mount->track_streams[track_no], 0,
                                 WASHDC_HOSTFILE_SEEK_END) != 0) {
            error_set_file_path(string_get(track_path));
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        long len = washdc_hostfile_tell(mount->track_streams[track_no]);
        if (len < 0) {
            error_set_file_path(string_get(track_path));
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        mount->track_lengths[track_no] = len;
    }

    mount_insert(&gdi_mount_ops, mount);
}

static void mount_gdi_cleanup(struct mount *mount) {
    struct gdi_mount *state = (struct gdi_mount*)mount->state;

    unsigned track_no;
    for (track_no = 0; track_no < state->meta.n_tracks; track_no++)
        washdc_hostfile_close(state->track_streams[track_no]);
    free(state->track_streams);

    cleanup_gdi_info(&state->meta);

    free(state);
}

static unsigned mount_gdi_session_count(struct mount *mount) {
    return 1;
}

static enum mount_disc_type gdi_get_disc_type(struct mount* mount) {
    return DISC_TYPE_GDROM;
}

static int mount_gdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned region) {
    struct gdi_mount const *gdi_mount = (struct gdi_mount const*)mount->state;
    struct gdi_info const *info = &gdi_mount->meta;

    memset(toc->tracks, 0, sizeof(toc->tracks));

    if (region == MOUNT_LD_REGION) {
        // the LD region contains the first two tracks

        // track 1
        toc->tracks[0].fad = info->tracks[0].fad_start;
        toc->tracks[0].adr = 1;
        toc->tracks[0].ctrl = info->tracks[0].ctrl;
        toc->tracks[0].valid = true;

        // track 2
        toc->tracks[1].fad = info->tracks[1].fad_start;
        toc->tracks[1].adr = 1;
        toc->tracks[1].ctrl = info->tracks[1].ctrl;
        toc->tracks[1].valid = true;

        toc->first_track = 1;
        toc->last_track = 2;
    } else {
        // the HD region contains all tracks but the first two

        unsigned src_track_no;
        for (src_track_no = 3; src_track_no <= info->n_tracks; src_track_no++) {
            toc->tracks[src_track_no - 1].fad =
                info->tracks[src_track_no - 1].fad_start;
            toc->tracks[src_track_no - 1].adr = 1;
            toc->tracks[src_track_no - 1].ctrl =
                info->tracks[src_track_no - 1].ctrl;
            toc->tracks[src_track_no - 1].valid = true;
        }

        toc->first_track = 3;
        toc->last_track = info->n_tracks;
    }

    /*
     * confession: I don't know what this is yet
     *
     * I *think* it's supposed to point to the first block after the last track
     * in the session, but I need to confirm this.  It's surprisingly hard to
     * find documentation on the lower level aspects of CD even though it's
     * such a ubiquitous media.
     */
    toc->leadout = gdi_mount->track_lengths[toc->last_track - 1] /
        info->tracks[toc->last_track - 1].sector_size +
        info->tracks[toc->last_track - 1].fad_start;
    toc->leadout_adr = 1;

    return 0;
}

static int mount_read_sector(struct mount *mount, void *buf, unsigned fad) {
    struct gdi_mount const *gdi_mount = (struct gdi_mount const*)mount->state;
    struct gdi_info const *info = &gdi_mount->meta;

    unsigned track_idx;
    for (track_idx = 0; track_idx < info->n_tracks; track_idx++) {
        struct gdi_track const *trackp = info->tracks + track_idx;

        unsigned track_fad_count =
            gdi_mount->track_lengths[track_idx] / CDROM_FRAME_SIZE;
        if ((fad >= trackp->fad_start) &&
            (fad < (trackp->fad_start + track_fad_count))) {

            // TODO: support MODE2 FORM1, MODE2 FORM2, CDDA, etc...
            unsigned fad_relative = fad - trackp->fad_start;
            unsigned byte_offset = CDROM_FRAME_SIZE * fad_relative +
                CDROM_MODE1_DATA_OFFSET;

            LOG_DBG("Select track %d (%u blocks starting from %u)\n",
                     track_idx + 1, track_fad_count, (unsigned)trackp->fad_start);
            LOG_DBG("read 1 sector starting at byte %u\n", byte_offset);

            // TODO: don't ignore the offset
            if (washdc_hostfile_seek(gdi_mount->track_streams[track_idx],
                                     byte_offset,
                                     WASHDC_HOSTFILE_SEEK_BEG) != 0) {
                return MOUNT_ERROR_FILE_IO;
            }

            if (washdc_hostfile_read(gdi_mount->track_streams[track_idx], buf,
                                     2048) != 2048)
                return MOUNT_ERROR_FILE_IO;

            return MOUNT_SUCCESS;
        }
    }

    return MOUNT_ERROR_OUT_OF_BOUNDS;
}

static int mount_gdi_get_meta(struct mount *mount, struct mount_meta *meta) {
    struct gdi_mount const *gdi_mount = (struct gdi_mount const*)mount->state;
    struct gdi_info const *info = &gdi_mount->meta;
    uint8_t buffer[256];

    if (info->n_tracks < 3)
        return -1;

    if (washdc_hostfile_seek(gdi_mount->track_streams[2], 16,
                             WASHDC_HOSTFILE_SEEK_BEG))
        return -1;

    if (washdc_hostfile_read(gdi_mount->track_streams[2], buffer,
                             sizeof(buffer)) != sizeof(buffer))
        return -1;

    memset(meta, 0, sizeof(*meta));

    memcpy(meta->hardware, buffer, MOUNT_META_HARDWARE_LEN);
    memcpy(meta->maker, buffer + 16, MOUNT_META_MAKER_LEN);
    memcpy(meta->dev_info, buffer + 32, MOUNT_META_DEV_INFO_LEN);
    memcpy(meta->region, buffer + 48, MOUNT_META_REGION_LEN);
    memcpy(meta->periph_support, buffer + 56, MOUNT_META_PERIPH_LEN);
    memcpy(meta->product_id, buffer + 64, MOUNT_META_PRODUCT_ID_LEN);
    memcpy(meta->product_version, buffer + 74, MOUNT_META_PRODUCT_VERSION_LEN);
    memcpy(meta->rel_date, buffer + 80, MOUNT_META_REL_DATE_LEN);
    memcpy(meta->boot_file, buffer + 96, MOUNT_META_BOOT_FILE_LEN);
    memcpy(meta->company, buffer + 112, MOUNT_META_COMPANY_LEN);
    memcpy(meta->title, buffer + 128, MOUNT_META_TITLE_LEN);

    return 0;
}

static unsigned mount_gdi_get_leadout(struct mount *mount) {
    struct gdi_mount const *gdi_mount = (struct gdi_mount const*)mount->state;
    unsigned n_tracks = gdi_mount->meta.n_tracks;
    struct gdi_track const *last_track = gdi_mount->meta.tracks + (n_tracks - 1);
    unsigned sector_size = last_track->sector_size;

    unsigned last_track_len = gdi_mount->track_lengths[n_tracks - 1] / sector_size;
    unsigned last_track_offs = cdrom_fad_to_lba(last_track->fad_start);

    return last_track_len + last_track_offs;
}

static bool mount_gdi_has_hd_region(struct mount *mount) {
    return true;
}

static void gdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad) {
    if (session_no != 0)
        RAISE_ERROR(ERROR_INTEGRITY);// there's only one session on a GD-ROM

    struct gdi_mount const *gdi_mount = (struct gdi_mount const*)mount->state;

    if (!gdi_mount->meta.n_tracks)
        RAISE_ERROR(ERROR_INTEGRITY);

    *start_track = 0;
    *fad = gdi_mount->meta.tracks[0].fad_start;
}
