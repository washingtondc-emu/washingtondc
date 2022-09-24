/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020, 2022 snickerbockers
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "mount.h"
#include "cdrom.h"
#include "washdc/hostfile.h"
#include "washdc/error.h"

struct cdi_track {
    /*
     * start of the pregap before the track.  You'll need to add
     * (pregap_len * sector_sz) to get to the actual start of the track.
     */
    unsigned start;
    unsigned pregap_len;
    unsigned track_len;
    unsigned sector_sz;
    unsigned start_lba;

    unsigned ctrl;

    /*
     * this is always pregap_len + track_len, but it's a separate field in the
     * .cdi so might as well take it seriously in case there are any cdi files
     * out there with weird padding or something.
     */
    unsigned total_len;
};

struct cdi_session {
    unsigned n_tracks;
    unsigned first_track;
    struct cdi_track *tracks;
};

struct cdi_mount {
    washdc_hostfile stream;
    unsigned n_sessions;
    struct cdi_session *sessions;
};

static void read_session(washdc_hostfile stream,
                         struct cdi_session *sess,
                         size_t *total_pos, unsigned ver);
static void read_track(washdc_hostfile stream,
                       struct cdi_track *track,
                       size_t *total_pos, unsigned ver);
static unsigned cdi_get_leadout(struct mount *mount);

static unsigned mount_cdi_session_count(struct mount *mount);
static int mount_cdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned region);
static int mount_cdi_read_sector(struct mount *mount,
                                 void *buf, unsigned fad);
static void mount_cdi_cleanup(struct mount *mount);
static int mount_cdi_get_meta(struct mount *mount, struct mount_meta *meta);
static enum mount_disc_type cdi_get_disc_type(struct mount *mount);
static void cdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad);
static bool mount_cdi_has_hd_region(struct mount *mount);
static enum mount_disc_type cdi_get_disc_type(struct mount* mount);

static struct mount_ops cdi_mount_ops = {
    .session_count = mount_cdi_session_count,
    .read_toc = mount_cdi_read_toc,
    .read_sector = mount_cdi_read_sector,
    .cleanup = mount_cdi_cleanup,
    .get_meta = mount_cdi_get_meta,
    .get_leadout = cdi_get_leadout,
    .has_hd_region = mount_cdi_has_hd_region,
    .get_disc_type = cdi_get_disc_type,
    .get_session_start = cdi_get_session_start
};

static DEF_ERROR_U32_ATTR(cdi_version)
static DEF_ERROR_U32_ATTR(sector_size_constant)

void mount_cdi(char const *path) {
    struct cdi_mount *mount =
        (struct cdi_mount*)calloc(1, sizeof(struct cdi_mount));
    if (!mount)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    washdc_hostfile stream = washdc_hostfile_open(path, WASHDC_HOSTFILE_READ |
                                                  WASHDC_HOSTFILE_BINARY);
    if (stream == WASHDC_HOSTFILE_INVALID) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    mount->stream = stream;

    if (washdc_hostfile_seek(stream, -8, WASHDC_HOSTFILE_SEEK_END) != 0) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    int32_t type_and_header_pos[2];
    if (washdc_hostfile_read(stream, type_and_header_pos,
                             sizeof(type_and_header_pos)) !=
        sizeof(type_and_header_pos)) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint32_t ver = type_and_header_pos[0];
    LOG_INFO("CDI version is %08X\n", (unsigned)ver);

    switch (ver) {
    case 0x80000004:
    case 0x80000005:
        if (washdc_hostfile_seek(stream, type_and_header_pos[1],
                                 WASHDC_HOSTFILE_SEEK_BEG) != 0) {
            error_set_file_path(path);
            error_set_errno_val(errno);
            error_set_cdi_version(ver);
            RAISE_ERROR(ERROR_FILE_IO);
        }
        break;
    case 0x80000006:
        if (washdc_hostfile_seek(stream, -type_and_header_pos[1],
                                 WASHDC_HOSTFILE_SEEK_END) != 0) {
            error_set_file_path(path);
            error_set_errno_val(errno);
            error_set_cdi_version(ver);
            RAISE_ERROR(ERROR_FILE_IO);
        }
        break;
    default:
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint16_t n_sessions;
    if (washdc_hostfile_read(stream, &n_sessions, sizeof(n_sessions)) !=
        sizeof(n_sessions)) {
        error_set_file_path(path);
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    LOG_INFO("this CDI image contains %u sessions\n", (unsigned)n_sessions);
    mount->n_sessions = n_sessions;
    mount->sessions = calloc(n_sessions, sizeof(struct cdi_session));
    if (!mount->sessions)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    size_t total_pos = 0;
    unsigned sess_no;
    unsigned total_n_tracks = 0;
    for (sess_no = 0; sess_no < n_sessions; sess_no++) {
        // idk why i have to do this but i do
        if (sess_no != 0 && ver == 0x80000004)
            if (washdc_hostfile_seek(stream, 2,
                                     WASHDC_HOSTFILE_SEEK_CUR) != 0) {
                error_set_file_path(path);
                error_set_errno_val(errno);
                error_set_cdi_version(ver);
                RAISE_ERROR(ERROR_FILE_IO);
            }
        read_session(stream, mount->sessions + sess_no, &total_pos, ver);
        mount->sessions[sess_no].first_track = total_n_tracks;
        total_n_tracks += mount->sessions[sess_no].n_tracks;
    }

    mount_insert(&cdi_mount_ops, mount);
}

static void read_session(washdc_hostfile stream,
                         struct cdi_session *sess, size_t *total_pos,
                         unsigned ver) {
    // get number of tracks, otherwise skip session structure
    uint16_t n_tracks;
    if (washdc_hostfile_read(stream, &n_tracks, sizeof(n_tracks)) !=
        sizeof(n_tracks)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    unsigned skip = (ver == 0x80000005 ? 18 : 10);
    if (washdc_hostfile_seek(stream, skip, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    LOG_INFO("Session has %u tracks\n", (unsigned)n_tracks);

    sess->n_tracks = n_tracks;
    sess->tracks =
        (struct cdi_track*)calloc(n_tracks, sizeof(struct cdi_track));
    if (!sess->tracks)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    unsigned track_no;
    for (track_no = 0; track_no < sess->n_tracks; track_no++)
        read_track(stream, sess->tracks + track_no, total_pos, ver);
}

static void read_track(washdc_hostfile stream,
                       struct cdi_track *track,
                       size_t *total_pos, unsigned ver) {
    static uint8_t const start_pattern_expect[14] =
        { 255, 255, 255, 255, 0, 0, 1, 0, 0, 0, 255, 255, 255, 255 };

    static uint8_t start_pattern[14];
    if (washdc_hostfile_read(stream, start_pattern, sizeof(start_pattern)) !=
        sizeof(start_pattern)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    if (memcmp(start_pattern, start_pattern_expect,
               sizeof(start_pattern)) != 0) {
        LOG_ERROR("unrecognizable track start pattern!\n");
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint8_t path_len;
    if (washdc_hostfile_seek(stream, 4, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    if (washdc_hostfile_read(stream, &path_len, sizeof(path_len)) !=
        sizeof(path_len)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    unsigned seek_amt = path_len;
    if (ver == 0x80000006) {
        seek_amt += 33;
    } else if (ver == 0x80000004 || ver == 0x80000005) {
        seek_amt += 25;
    } else {
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (washdc_hostfile_seek(stream, seek_amt, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint32_t pregap_len;
    if (washdc_hostfile_read(stream, &pregap_len, sizeof(pregap_len)) !=
        sizeof(pregap_len)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint32_t track_len;
    if (washdc_hostfile_read(stream, &track_len, sizeof(track_len)) !=
        sizeof(track_len)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    if (washdc_hostfile_seek(stream, 22, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint32_t start_lba;
    if (washdc_hostfile_read(stream, &start_lba, sizeof(start_lba)) !=
        sizeof(start_lba)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint32_t total_len;
    if (washdc_hostfile_read(stream, &total_len, sizeof(total_len)) !=
        sizeof(total_len)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    LOG_INFO("start_lba is %u\n", (unsigned)start_lba);
    LOG_INFO("pregap length for this track is %u blocks\n",
             (unsigned)pregap_len);
    LOG_INFO("track length is %u\n", (unsigned)track_len);
    LOG_INFO("total length is %u\n", (unsigned)total_len);

    if (washdc_hostfile_seek(stream, 16, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint8_t sector_tp;
    unsigned sector_sz;
    if (washdc_hostfile_read(stream, &sector_tp, sizeof(sector_tp)) !=
        sizeof(sector_tp)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    switch (sector_tp) {
    case 2:
        sector_sz = 2352;
        break;
    case 1:
        sector_sz = 2336;
        break;
    default:
        error_set_sector_size_constant(sector_tp);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    LOG_INFO("each sector is %u bytes long\n", sector_sz);

    if (washdc_hostfile_seek(stream, 3, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    uint8_t ctrl;
    if (washdc_hostfile_read(stream, &ctrl, sizeof(ctrl)) != sizeof(ctrl)) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    LOG_INFO("control byte is %x\n", (unsigned)ctrl);

    if (ver == 0x80000004)
        seek_amt = 38;
    else
        seek_amt = 128;

    if (washdc_hostfile_seek(stream, seek_amt, WASHDC_HOSTFILE_SEEK_CUR) != 0) {
        error_set_errno_val(errno);
        error_set_cdi_version(ver);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    track->start_lba = start_lba;
    track->start = *total_pos;
    track->pregap_len = pregap_len;
    track->track_len = track_len;
    track->sector_sz = sector_sz;
    track->total_len = total_len;
    track->ctrl = ctrl;

    *total_pos += (size_t)total_len * sector_sz;
}

static void mount_cdi_cleanup(struct mount *mount) {
    struct cdi_mount *state = (struct cdi_mount*)mount->state;

    unsigned sess_no;
    for (sess_no = 0; sess_no < state->n_sessions; sess_no++)
        free(state->sessions[sess_no].tracks);
    free(state->sessions);
    washdc_hostfile_close(state->stream);
    free(state);
    mount->state = NULL;
}

static unsigned mount_cdi_session_count(struct mount *mount) {
    struct cdi_mount *state = (struct cdi_mount*)mount->state;
    return state->n_sessions;
}

static enum mount_disc_type cdi_get_disc_type(struct mount *mount) {
    return DISC_TYPE_CDROM_XA;
}

static int mount_cdi_read_toc(struct mount *mount, struct mount_toc *toc,
                              unsigned region) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;

    if (region != MOUNT_LD_REGION)
        return -1;

    memset(toc->tracks, 0, sizeof(toc->tracks));

    unsigned sess_no, absolute_track_no = 0;
    for (sess_no = 0; sess_no < cdi_mount->n_sessions; sess_no++) {
        struct cdi_session *sess = cdi_mount->sessions + sess_no;
        unsigned track_no;
        for (track_no = 0; track_no < sess->n_tracks; track_no++) {
            struct cdi_track *src_track = sess->tracks + track_no;
            struct mount_track *dst_track = toc->tracks + absolute_track_no;
            dst_track->ctrl = src_track->ctrl;
            dst_track->adr = 1;
            dst_track->valid = 1;
            dst_track->fad = cdrom_lba_to_fad(src_track->start_lba);
            absolute_track_no++;
        }
    }

    toc->first_track = 1;
    toc->last_track = absolute_track_no;
    toc->leadout = cdrom_lba_to_fad(cdi_get_leadout(mount));
    toc->leadout_adr = 1;

    LOG_INFO("request to read cdi TOC\n");

    return 0;
}

static unsigned cdi_get_leadout(struct mount *mount) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_session const *last_session =
        cdi_mount->sessions + (cdi_mount->n_sessions - 1);
    struct cdi_track const *last_track =
        last_session->tracks + (last_session->n_tracks - 1);
    return last_track->start_lba + last_track->track_len;
}

static int mount_cdi_read_sector(struct mount *mount,
                                 void *buf, unsigned fad) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;

    unsigned lba = cdrom_fad_to_lba(fad);
    LOG_INFO("CDI Request to read LBA %u\n", lba);
    unsigned track_no, session_no;
    for (session_no = 0; session_no < cdi_mount->n_sessions; session_no++) {
        struct cdi_session *sess = cdi_mount->sessions + session_no;
        for (track_no = 0; track_no < sess->n_tracks; track_no++) {
            struct cdi_track *track = sess->tracks + track_no;
            if (lba >= track->start_lba && lba <
                track->start_lba + track->track_len) {
                LOG_INFO("Session %u, track %u\n", session_no, track_no);
                LOG_INFO("\ttrack start is %u\n", track->start);
                LOG_INFO("\ttrack pregap length is %u blocks\n", track->pregap_len);
                LOG_INFO("\ttrack length is %u blocks\n", track->track_len);
                LOG_INFO("\ttrack sector size is %u\n", track->sector_sz);
                LOG_INFO("\ttrack first LBA is %u\n", track->start_lba);
                LOG_INFO("\ttrack control nibble is %u\n", track->ctrl);
                LOG_INFO("\ttrack total length is %u\n", track->total_len);

                size_t byte_offset = (size_t)track->start +
                    (size_t)track->pregap_len * track->sector_sz + 8 +
                    (size_t)(lba - track->start_lba) * track->sector_sz;

                if (washdc_hostfile_seek(cdi_mount->stream,
                                         byte_offset,
                                         WASHDC_HOSTFILE_SEEK_BEG) != 0) {
                    LOG_ERROR("failure to seek to track (byte offset %llx)\n",
                              (unsigned long long)byte_offset);
                    return -1;
                }
                size_t bytes_read;
                if ((bytes_read = washdc_hostfile_read(cdi_mount->stream,
                                                       buf, 2048)) != 2048) {
                    LOG_ERROR("Failure to read from cdi file (returned length %llu)\n",
                              (long long unsigned)bytes_read);
                    return -1;
                }
                return 0;
            }
        }
    }
    LOG_ERROR("unable to locate LBA %u\n", lba);
    return -1;
}

static void cdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;

    if (session_no >= cdi_mount->n_sessions)
        RAISE_ERROR(ERROR_INTEGRITY);

    struct cdi_session *session = cdi_mount->sessions + session_no;
    struct cdi_track *first_track = session->tracks;

    *start_track = session->first_track;
    *fad = cdrom_lba_to_fad(first_track->start_lba);
}

static int mount_cdi_get_meta(struct mount *mount, struct mount_meta *meta) {
    memset(meta, 0, sizeof(*meta));

    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;

    if (cdi_mount->n_sessions < 2) {
        LOG_ERROR("Unable to fetch image metadata: not enough sessions\n");
        return -1;
    }

    struct cdi_session *sess = cdi_mount->sessions + 1;

    if (sess->n_tracks == 0) {
        LOG_ERROR("Unable to fetch image metadata: no tracks on second "
                  "session\n");
        return -1;
    }

    struct cdi_track *track = sess->tracks;
    if (track->track_len == 0) {
        LOG_ERROR("Unable to fetch image metadata: no data on first track of "
                  "second session.\n");
        return -1;
    }

    unsigned fad = cdrom_lba_to_fad(sess->tracks[0].start_lba);
    uint8_t buffer[2048];
    if (mount_cdi_read_sector(mount, buffer, fad) < 0) {
        LOG_ERROR("Unable to fetch image metadata: failure to read.\n");
        return -1;
    }

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

static bool mount_cdi_has_hd_region(struct mount *mount) {
    return false;
}
