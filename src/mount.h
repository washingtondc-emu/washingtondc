/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef MOUNT_H_
#define MOUNT_H_

/*
 * mount.h
 *
 * virtual interface for mounting disc-images of various formats such as .cdi,
 * .gdi, .cue, etc.  Currently only .gdi is supported.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mount_ops;

struct mount {
    struct mount_ops const *ops;
    void *state;
};

/*
 * CD-ROM table-of-contents structure
 * Unlike the actual table-of-contents structure this doesn't include the GDROM
 * LBA offset or the big-endiannes of the track's LBA, and every field is
 * separate.  The GDROM code over in hw/ is what converts this to the actual
 * table-of-contents structure.
 *
 * Like in an actual CD-ROM, the track numbers here are one-indexed,
 * not zero-indexed
 */
struct mount_track {
    unsigned ctrl;
    unsigned adr; // usually ignored (set to 0)
    unsigned fad;

    /*
     * if false, the track is unused, and will be
     * filled with all ones by mount_encode_toc.
     */
    bool valid;
};

struct mount_toc {
    struct mount_track tracks[99];
    unsigned first_track, last_track;
    unsigned leadout;
    unsigned leadout_adr;
};

struct mount_ops {
    // return the number of sessions on the disc (shouldn't be more than 2)
    unsigned(*session_count)(struct mount*);

    // read in the TOC for the given session; return 0 on success or nonzero on error
    int(*read_toc)(struct mount*, struct mount_toc*, unsigned);

    int(*read_sector)(struct mount*, void*, unsigned);

    // release resources held by the mount
    void (*cleanup)(struct mount*);
};

// mount an image as the current disc in the virtual gdrom drive
void mount_insert(struct mount_ops const *ops, void *ptr);

// unmount anything that may be mounted
void mount_eject(void);

// return true if there's an image mounted; else return false
bool mount_check(void);

// return the number of sessions in the disc
unsigned mount_session_count(void);

int mount_read_toc(struct mount_toc* out, unsigned session);

int mount_read_sectors(void *buf_out, unsigned fad, unsigned sector_count);

/*
 * size of an actual CD-ROM Table-Of-Contents structure.  This is the length of
 * the data returned by mount_encode_toc.
 */
#define CDROM_TOC_SIZE ((99 + 3) * 4)

/*
 * this function takes the given TOC and encodes it into the actual CD-ROM TOC
 * format.  the pointer returned points to a statically allocated buffer; it
 * should not be expected to remain consistent across multiple calls to
 * mount_encode_toc and it should not be freed.
 */
void const* mount_encode_toc(struct mount_toc const *toc);

#ifdef __cplusplus
}
#endif

#endif
