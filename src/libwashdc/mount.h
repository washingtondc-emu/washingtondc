/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2021 snickerbockers
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

struct mount_meta {
#define MOUNT_META_HARDWARE_LEN 16
#define MOUNT_META_MAKER_LEN 16
#define MOUNT_META_DEV_INFO_LEN 16
#define MOUNT_META_REGION_LEN 8
#define MOUNT_META_PERIPH_LEN 8
#define MOUNT_META_PRODUCT_ID_LEN 10
#define MOUNT_META_PRODUCT_VERSION_LEN 6
#define MOUNT_META_REL_DATE_LEN 16
#define MOUNT_META_BOOT_FILE_LEN 16
#define MOUNT_META_COMPANY_LEN 16
#define MOUNT_META_TITLE_LEN 128

    /*
     * The strings all have length+1 because the DC metadata doesn't use a null
     * terminator, but I do.
     */
    char hardware[MOUNT_META_HARDWARE_LEN + 1];
    char maker[MOUNT_META_MAKER_LEN + 1];
    char dev_info[MOUNT_META_DEV_INFO_LEN + 1];
    char region[MOUNT_META_REGION_LEN + 1];
    char periph_support[MOUNT_META_PERIPH_LEN + 1];
    char product_id[MOUNT_META_PRODUCT_ID_LEN + 1];
    char product_version[MOUNT_META_PRODUCT_VERSION_LEN + 1];
    char rel_date[MOUNT_META_REL_DATE_LEN + 1];
    char boot_file[MOUNT_META_BOOT_FILE_LEN + 1];
    char company[MOUNT_META_COMPANY_LEN + 1];
    char title[MOUNT_META_TITLE_LEN + 1];
};

#define MOUNT_LD_REGION 0
#define MOUNT_HD_REGION 1

enum mount_disc_type {
    DISC_TYPE_CDDA = 0,
    DISC_TYPE_CDROM = 1,
    DISC_TYPE_CDROM_XA = 2,
    DISC_TYPE_CDI = 3, // i think this refers to phillips CD-I, not .cdi images
    DISC_TYPE_GDROM = 8
};

// error/success codes; for now these only apply to read_sector(s)
#define MOUNT_SUCCESS 0 // operation completed successfully
#define MOUNT_ERROR_NO_MEDIA -1 // there's nothing mounted
#define MOUNT_ERROR_FILE_IO -2 // one of the file io functions returned error
#define MOUNT_ERROR_OUT_OF_BOUNDS -3 // requested fad does not exist on media

struct mount_ops {
    // return the number of sessions on the disc (shouldn't be more than 2)
    unsigned(*session_count)(struct mount*);

    // read in the TOC for the given density region; return 0 on success or nonzero on error
    int(*read_toc)(struct mount*, struct mount_toc*, unsigned);

    int(*read_sector)(struct mount*, void*, unsigned);

    // release resources held by the mount
    void (*cleanup)(struct mount*);

    int (*get_meta)(struct mount*, struct mount_meta*);

    /* returns leadout for the whole disc in terms of lba */
    unsigned (*get_leadout)(struct mount*);

    // return true if this disc has a high-density region
    bool (*has_hd_region)(struct mount*);

    enum mount_disc_type (*get_disc_type)(struct mount*);

    /*
     * first output is the first track of the session, second output is the
     * starting fad of that track.
     */
    void (*get_session_start)(struct mount*, unsigned, unsigned*, unsigned*);
};

// mount an image as the current disc in the virtual gdrom drive
void mount_insert(struct mount_ops const *ops, void *ptr);

// unmount anything that may be mounted
void mount_eject(void);

// return true if there's an image mounted; else return false
bool mount_check(void);

// return the number of sessions in the disc
unsigned mount_session_count(void);

enum mount_disc_type mount_get_disc_type(void);

int mount_read_toc(struct mount_toc* out, unsigned session);

int mount_read_sectors(void *buf_out, unsigned fad, unsigned sector_count);

int mount_get_meta(struct mount_meta *meta);

unsigned mount_get_leadout(void);

bool mount_has_hd_region(void);

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

void mount_get_session_start(unsigned session_no, unsigned* first_track,
                             unsigned* first_fad);

#endif
