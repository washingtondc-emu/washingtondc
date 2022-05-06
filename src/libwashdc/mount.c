/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2022 snickerbockers
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
#include "washdc/log.h"
#include "log.h"
#include "cdrom.h"

#include "mount.h"

static bool mounted;
static struct mount img;

#define MOUNT_TRACE(msg, ...) LOG_DBG("MOUNT: " msg, ##__VA_ARGS__)

void mount_insert(struct mount_ops const *ops, void *ptr) {
    MOUNT_TRACE("%s - inserting media\n", __func__);

    if (img.state)
        mount_eject();

    img.ops = ops;
    img.state = ptr;
    mounted = true;
}

void mount_eject(void) {
    MOUNT_TRACE("%s - ejecting media\n", __func__);

    if (img.ops->cleanup)
        img.ops->cleanup(&img);

    memset(&img, 0, sizeof(img));
    mounted = false;
}

bool mount_check(void) {
    if (mounted)
        MOUNT_TRACE("%s - media mounted\n", __func__);
    else
        MOUNT_TRACE("%s - no media mounted\n", __func__);
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

static inline char const *mount_disc_type_str(enum mount_disc_type tp) {
    switch (tp) {
    case DISC_TYPE_CDDA:
        return "CDDA";
    case DISC_TYPE_CDROM:
        return "CD-ROM";
    case DISC_TYPE_CDROM_XA:
        return "XA";
    case DISC_TYPE_CDI:
        return "CD-i";
    case DISC_TYPE_GDROM:
        return "GD-ROM";
    default:
        return "UNKNOWN/ERROR";
    }
}

enum mount_disc_type mount_get_disc_type(void) {
    if (mounted) {
        enum mount_disc_type tp = img.ops->get_disc_type(&img);
        MOUNT_TRACE("%s - disc type is %s\n",
                    __func__, mount_disc_type_str(tp));
        return tp;
    }

    error_set_wtf("calling mount_session_count when there's nothing mounted");
    RAISE_ERROR(ERROR_INTEGRITY);
}

int mount_read_toc(struct mount_toc* out, unsigned region) {
    if (mounted) {
        if ((region == MOUNT_HD_REGION && !mount_has_hd_region()) ||
            !img.ops->read_toc)
            return -1;
        int err = img.ops->read_toc(&img, out, region);
        if (err == 0) {
            MOUNT_TRACE("%s TOC DUMP:\n", __func__);
            MOUNT_TRACE("\tfirst_track: %u\n", out->first_track);
            MOUNT_TRACE("\tlast_track: %u\n", out->last_track);
            MOUNT_TRACE("\tleadout_adr: %u\n", out->leadout_adr);
            unsigned row;
            for (row = 0; row < 10; row++) {
                unsigned col;
                for (col = 0; col < 10; col++) {
                    unsigned trackno = row * 10 + col + 1;
                    if (trackno >= 100)
                        break; // TODO: fix this shitty-ass math before merging to master
                    struct mount_track const *trackp = out->tracks + (trackno - 1);
                    MOUNT_TRACE("\ttrack %u: %s\n", trackno,
                              trackp->valid ? "valid" : "invalid");
                    MOUNT_TRACE("\t\tctrl: %u\n", trackp->ctrl);
                    MOUNT_TRACE("\t\tadr: %u\n", trackp->adr);
                    MOUNT_TRACE("\t\tfad: %u\n", trackp->fad);
                }
            }
        }
        return err;
    } else {
        error_set_wtf("calling mount_read_toc when there's nothing mounted");
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

int mount_read_sectors(void *buf_out, unsigned fad_start,
                       unsigned sector_count) {
    MOUNT_TRACE("request to read %u sectors starting from %u\n",
                sector_count, fad_start);

    if (!mount_check() || !img.ops->read_sector)
        return -1;

    unsigned fad;
    for (fad = fad_start; fad < (fad_start + sector_count); fad++) {
        void *where = ((uint8_t*)buf_out) +
            CDROM_FRAME_DATA_SIZE * (fad - fad_start);
        if (img.ops->read_sector(&img, where, fad) != 0)
            return -1;
    }

    return 0;
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
    if (!mount_check())
        return -1;

    if (!img.ops->get_meta) {
        LOG_ERROR("%s - unable to obtain metadata because the get_meta function is "
                  "not implemented for the given media.\n", __func__);
        return -1;
    }

    int err = img.ops->get_meta(&img, meta);
    if (err != 0) {
        LOG_ERROR("%s - failed because get_meta implementation returned %d\n",
                  __func__, err);
    }
    return err;
}

unsigned mount_get_leadout(void) {
    unsigned leadout = img.ops->get_leadout(&img);
    MOUNT_TRACE("%s - leadout %u\n", __func__, leadout);
    return leadout;
}

bool mount_has_hd_region(void) {
    bool has_hd = img.ops->has_hd_region(&img);
    if (has_hd)
        MOUNT_TRACE("%s - true\n", __func__);
    else
        MOUNT_TRACE("%s - false\n", __func__);
    return has_hd;
}

void mount_get_session_start(unsigned session_no, unsigned* first_track,
                             unsigned* first_fad) {
    img.ops->get_session_start(&img, session_no, first_track, first_fad);
    MOUNT_TRACE("%s - first_track=%u, first_fad=%u\n",
                __func__, *first_track, *first_fad);
}
