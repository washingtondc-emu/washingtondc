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

#include <string.h>

#include "washdc/error.h"
#include "cdrom.h"

#include "mount.h"

static bool mounted;
static struct mount img;

void mount_insert(struct mount_ops const *ops, void *ptr) {
    if (img.state)
        mount_eject();

    img.ops = ops;
    img.state = ptr;
    mounted = true;
}

void mount_eject(void) {
    if (img.ops->cleanup)
        img.ops->cleanup(&img);

    memset(&img, 0, sizeof(img));
    mounted = false;
}

bool mount_check(void) {
    return mounted;
}

unsigned mount_session_count(void) {
    if (mounted) {
        if (img.ops->session_count)
            return img.ops->session_count(&img);
        return 0;
    }

    error_set_wtf("calling mount_session_count when there's nothing mounted");
    RAISE_ERROR(ERROR_INTEGRITY);
}

enum mount_disc_type mount_get_disc_type(void) {
    if (mounted)
        return img.ops->get_disc_type(&img);

    error_set_wtf("calling mount_session_count when there's nothing mounted");
    RAISE_ERROR(ERROR_INTEGRITY);
}

int mount_read_toc(struct mount_toc* out, unsigned region) {
    if (mounted) {
        if ((region == MOUNT_HD_REGION && !mount_has_hd_region()) ||
            !img.ops->read_toc)
            return -1;
        return img.ops->read_toc(&img, out, region);
    } else {
        error_set_wtf("calling mount_read_toc when there's nothing mounted");
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

int mount_read_sectors(void *buf_out, unsigned fad_start,
                       unsigned sector_count) {
    if (!mount_check() || !img.ops->read_sector)
        return MOUNT_ERROR_NO_MEDIA;

    unsigned fad;
    for (fad = fad_start; fad < (fad_start + sector_count); fad++) {
        void *where = ((uint8_t*)buf_out) +
            CDROM_FRAME_DATA_SIZE * (fad - fad_start);
        int err_code;
        if ((err_code = img.ops->read_sector(&img, where, fad)) !=
            MOUNT_SUCCESS)
            return err_code;
    }

    return MOUNT_SUCCESS;
}

void const* mount_encode_toc(struct mount_toc const *toc) {
    static uint8_t toc_out[CDROM_TOC_SIZE];

    unsigned track_no;
    for (track_no = 1; track_no <= 99; track_no++) {
        unsigned track_idx = track_no - 1;
        struct mount_track const *trackp = toc->tracks + track_idx;
        if (trackp->valid) {
            uint32_t fad = trackp->fad;
            uint32_t fad_be = ((fad & 0xff0000) >> 16) |
                (fad & 0x00ff00) |
                ((fad & 0x0000ff) << 16);

            uint32_t track_bin = (trackp->adr & 0xf) |
                ((trackp->ctrl << 4) & 0xf0) |
                (fad_be << 8);

            memcpy(toc_out + 4 * track_idx, &track_bin, sizeof(track_bin));
        } else {
            memset(toc_out + 4 * track_idx, 0xff, 4);
        }
    }

    struct mount_track const *first_trackp =
        toc->tracks + (toc->first_track - 1);
    struct mount_track const *last_trackp =
        toc->tracks + (toc->last_track - 1);

    uint32_t first_track_bin = (first_trackp->adr & 0xf) |
        ((first_trackp->ctrl << 4) & 0xf0) |
        ((toc->first_track << 8) & 0xff00);
    uint32_t last_track_bin = (last_trackp->adr & 0xf) |
        ((last_trackp->ctrl << 4) & 0xf0) |
        ((toc->last_track << 8) & 0xff00);

    memcpy(toc_out + 99 * 4, &first_track_bin, sizeof(first_track_bin));
    memcpy(toc_out + 100 * 4, &last_track_bin, sizeof(last_track_bin));

    /*
     * It is not a mistake that this gets the ctrl from the last track's ctrl
     * val; that seems to be how this is supposed to work (I think)...
     */
    unsigned leadout_fad = toc->leadout;
    uint32_t leadout_bin = ((((leadout_fad & 0xff0000) >> 16) |
                             (leadout_fad & 0x00ff00) |
                             ((leadout_fad & 0x0000ff) << 16)) << 8) |
        toc->leadout_adr | ((last_trackp->ctrl << 4) & 0xf0);
    memcpy(toc_out + 101 * 4, &leadout_bin, sizeof(leadout_bin));

    return toc_out;
}

int mount_get_meta(struct mount_meta *meta) {
    if (!mount_check() || !img.ops->get_meta)
        return -1;

    return img.ops->get_meta(&img, meta);
}

unsigned mount_get_leadout(void) {
    return img.ops->get_leadout(&img);
}

bool mount_has_hd_region(void) {
    return img.ops->has_hd_region(&img);
}

void mount_get_session_start(unsigned session_no, unsigned* first_track,
                             unsigned* first_fad) {
    return img.ops->get_session_start(&img, session_no, first_track, first_fad);
}
