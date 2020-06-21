/*******************************************************************************
 *
 * Copyright 2017, 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
