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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "stringlib.h"
#include "error.h"

#include "gdi.h"

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

            string_init(&trackp->path);
            string_get_col(&trackp->path, &cur_line, 4, " \t");
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
        string_cleanup(&track->path);
    }

    free(info->tracks);
    info->tracks = NULL;
    info->n_tracks = 0;
}

void print_gdi(struct gdi_info const *gdi) {
    printf("%u\n", gdi->n_tracks);

    unsigned track_no;
    for (track_no = 0; track_no < gdi->n_tracks; track_no++) {
        struct gdi_track const *trackp = gdi->tracks + track_no;
        printf("%u %u %u %u %s %u\n",
               track_no + 1, trackp->lba_start, trackp->ctrl,
               trackp->sector_size, string_get(&trackp->path), trackp->offset);
    }
}
