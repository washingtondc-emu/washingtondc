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

// CD-ROM table-of-contents structure
struct mount_toc {
    unsigned entry[99];
    unsigned first_track, last_track;
    unsigned leadout;
};

struct mount_ops {
    // return the number of sessions on the disc (shouldn't be more than 2)
    unsigned(*session_count)(struct mount*);

    // read in the TOC for the given session; return 0 on success or nonzero on error
    int(*read_toc)(struct mount*, struct mount_toc*, unsigned);

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

#ifdef __cplusplus
}
#endif

#endif
