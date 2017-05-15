/*******************************************************************************
 *
 *
 *   WashingtonDC Dreamcast Emulator
 *   Copyright (C) 2017 snickerbockers
 *   chimerasaurusrex@gmail.com
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 ***************************************************************************/

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stringlib.h"
#include "error.h"
#include "mount.h"

#include "gdi.h"

struct gdi_mount {
    struct gdi_info meta;
    FILE **track_streams;
};

static void mount_gdi_cleanup(struct mount *mount);
static unsigned mount_gdi_session_count(struct mount *mount);
static int mount_gdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned session_no);

// return true if this is a legitimate gd-rom; else return false
static bool gdi_validate_fmt(struct gdi_info const *info);

static struct mount_ops gdi_mount_ops = {
    .session_count = mount_gdi_session_count,
    .read_toc = mount_gdi_read_toc,
    .cleanup = mount_gdi_cleanup
};

/* enforce sane limits - MAX_TRACKS might need to be bigger tbh */
#define MAX_TRACKS 16
#define MAX_TRACK_FIELDS 16

void parse_gdi(struct gdi_info *outp, char const *path) {
    unsigned track_count;
    long file_sz;
    struct string whole_file_txt;
    FILE *stream = fopen(path, "r");
    struct gdi_track *tracks;

    string_init(&whole_file_txt);
    string_load_stdio(&whole_file_txt, stream);

    fclose(stream);

    struct string_curs line_curs;
    struct string cur_line;
    string_tok_begin(&line_curs);
    string_init(&cur_line);

    unsigned line_no = 0;
    while (string_tok_next(&cur_line, &line_curs, string_get(&whole_file_txt),
                           "\n")) {
        if (line_no == 0) {
            // first line - read track count
            track_count = atoi(string_get(&cur_line));

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

            struct string lba_start_col;
            string_init(&lba_start_col);
            string_get_col(&lba_start_col, &cur_line, 1, " \t");
            trackp->lba_start = atoi(string_get(&lba_start_col));
            string_cleanup(&lba_start_col);

            // i have no idea what this is for
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
            string_append_char(&trackp->abs_path, '/');
            string_append(&trackp->abs_path, string_get(&trackp->rel_path));
        }

        line_no++;
    }

    string_cleanup(&cur_line);
    string_cleanup(&whole_file_txt);

    outp->n_tracks = track_count;
    outp->tracks = tracks;
}

void cleanup_gdi(struct gdi_info *info) {
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

void print_gdi(struct gdi_info const *gdi) {
    printf("%u\n", gdi->n_tracks);

    unsigned track_no;
    for (track_no = 0; track_no < gdi->n_tracks; track_no++) {
        struct gdi_track const *trackp = gdi->tracks + track_no;
        printf("%u %u %u %u %s %u\n",
               track_no + 1, trackp->lba_start, trackp->ctrl,
               trackp->sector_size, string_get(&trackp->rel_path), trackp->offset);
    }
}

void mount_gdi(char const *path) {
    struct gdi_mount *mount =
        (struct gdi_mount*)calloc(1, sizeof(struct gdi_mount));

    if (!mount)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    parse_gdi(&mount->meta, path);

    printf("about to (attempt to) mount the following image:\n");
    print_gdi(&mount->meta);

    if (!gdi_validate_fmt(&mount->meta))
        RAISE_ERROR(ERROR_INVALID_PARAM);

    mount->track_streams = (FILE**)calloc(mount->meta.n_tracks, sizeof(FILE*));
    if (!mount->track_streams)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    unsigned track_no;
    for (track_no = 0; track_no < mount->meta.n_tracks; track_no++) {
        struct string const *track_path =
            &mount->meta.tracks[track_no].abs_path;
        mount->track_streams[track_no] = fopen(string_get(track_path), "rb");
        if (!mount->track_streams[track_no]) {
            error_set_file_path(string_get(track_path));
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }
    }

    mount_insert(&gdi_mount_ops, mount);
}

static void mount_gdi_cleanup(struct mount *mount) {
    struct gdi_mount *state = (struct gdi_mount*)mount->state;

    unsigned track_no;
    for (track_no = 0; track_no < state->meta.n_tracks; track_no++)
        fclose(state->track_streams[track_no]);
    free(state->track_streams);
    free(state);
}

static unsigned mount_gdi_session_count(struct mount *mount) {
    return 2;
}

static int mount_gdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned session_no) {
    struct gdi_info const *info = (struct gdi_info const*)mount->state;

    // GD-ROM disks have two sessions
    if (session_no > 1)
        return -1;

    memset(toc->tracks, -1, sizeof(toc->tracks));
    /* memset(toc->entry, 0, sizeof(toc->entry)); */

    if (session_no == 0) {
        // session 0 contains the first two tracks
        toc->track_count = 2;

        // track 1
        toc->tracks[0].lba = info->tracks[0].lba_start;
        toc->tracks[0].adr = 1;
        toc->tracks[0].ctrl = info->tracks[0].ctrl;

        // track 2
        toc->tracks[1].lba = info->tracks[1].lba_start;
        toc->tracks[1].adr = 1;
        toc->tracks[1].ctrl = info->tracks[1].ctrl;

        toc->first_track = 1;
        toc->last_track = 2;

        /* // track 1 */
        /* unsigned lba = ((info->tracks[0].lba_start & 0x0000ff) << 0) | */
        /*     ((info->tracks[0].lba_start  & 0x00ff00) << 8) | */
        /*     ((info->tracks[0].lba_start  & 0xff0000) << 16); */
        /* toc->entry[0] = (info->tracks[0].ctrl << 4) | lba; */

        /* // track 2 */
        /* lba = ((info->tracks[1].lba_start & 0x0000ff) << 0) | */
        /*     ((info->tracks[1].lba_start  & 0x00ff00) << 8) | */
        /*     ((info->tracks[1].lba_start  & 0xff0000) << 16); */
        /* toc->entry[1] = (info->tracks[1].ctrl << 4) | lba; */

        /* toc->first_track = info->tracks[0].ctrl | (1 << 8); // track 1 */
        /* toc->last_track = info->tracks[1].ctrl | (2 << 8);  // track 2 */
    } else {
        // TODO - implement
        toc->track_count = info->n_tracks - 2;

        unsigned src_track_no;
        for (src_track_no = 2; src_track_no < info->n_tracks; src_track_no++) {
            toc->tracks[src_track_no - 2].lba = info->tracks[src_track_no].lba_start;
            toc->tracks[src_track_no - 2].adr = 0;
            toc->tracks[src_track_no - 2].ctrl = info->tracks[src_track_no].ctrl;

            toc->first_track = 3;
            toc->last_track = info->n_tracks;
        }
    }

    /*
     * confession: I don't know what this is yet
     *
     * I *think* it's supposed to point to the first block after the last track
     * in the session, but I need to confirm this.  It's surprisingly hard to
     * find documentation on the lower level aspects of CD even though it's
     * such a ubiquitous media.
     */
    toc->leadout = 0;

    return 0;
}
