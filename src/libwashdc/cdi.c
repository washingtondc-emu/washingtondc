/**
 * $Id$
 *
 * CDI CD-image file support
 *
 * Copyright (c) 2005 Nathan Keynes.
 * Copyright (c) 2019, 2020 snickerbockers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "cdi.h"
#include "washdc/error.h"
#include "washdc/hostfile.h"
#include "log.h"
#include "mount.h"
#include "cdrom.h"

#define CDI_V2_ID 0x80000004
#define CDI_V3_ID 0x80000005
#define CDI_V35_ID 0x80000006

struct cdi_info;

static bool cdi_image_is_valid( washdc_hostfile f );
static int parse_cdi(struct cdi_info *outp, char const *path);
static unsigned cdi_get_session_count(struct mount *mount);
static void cdi_cleanup(struct mount *mount);
static int cdi_read_toc(struct mount *mount, struct mount_toc *toc,
                        unsigned session_no);
static int cdi_read_sector(struct mount *mount, void *buf, unsigned fad);
static enum mount_disc_type cdi_get_disc_type(struct mount* mount);
static unsigned cdi_get_leadout(struct mount *mount);

static bool cdi_has_hd_region(struct mount *mount);

static void cdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad);

enum cdi_mode {
    CDI_SECTOR_MODE2_FORM1,
    CDI_SECTOR_SEMIRAW_MODE2,
    CDI_SECTOR_CDDA,
    CDI_SECTOR_MODE1
};

struct cdi_track {
    unsigned lba;
    unsigned n_sectors;
    unsigned offset;
    unsigned flags;
    enum cdi_mode mode;
};

struct cdi_session {
    unsigned n_tracks;
    unsigned first_track;
    struct cdi_track *tracks;
};

struct cdi_info {
    washdc_hostfile fp;
    unsigned n_sessions;
    struct cdi_session *sessions;
};

static int cdi_get_meta(struct mount *mount, struct mount_meta *meta);

static void cdi_dump(struct cdi_info const *info);

static struct mount_ops const cdi_mount_ops = {
    .session_count = cdi_get_session_count,
    .cleanup = cdi_cleanup,
    .get_meta = cdi_get_meta,
    .read_toc = cdi_read_toc,
    .read_sector = cdi_read_sector,
    .get_disc_type = cdi_get_disc_type,
    .get_leadout = cdi_get_leadout,
    .has_hd_region = cdi_has_hd_region,
    .get_session_start = cdi_get_session_start
};

static const char TRACK_START_MARKER[20] = { 0,0,1,0,0,0,255,255,255,255,
        0,0,1,0,0,0,255,255,255,255 };

struct cdi_trailer {
    uint32_t cdi_version;
    uint32_t header_offset;
};

struct cdi_track_data {
    uint32_t pregap_length;
    uint32_t length;
    char unknown2[6];
    uint32_t mode;
    char unknown3[0x0c];
    uint32_t start_lba;
    uint32_t total_length;
    char unknown4[0x10];
    uint32_t sector_size;
    char unknown5[0x1D];
} __attribute__((packed));

struct cdi_mount {
    struct cdi_info meta;
};

void mount_cdi(char const *path)
{
    struct cdi_mount *mount =
        (struct cdi_mount*)calloc(1, sizeof(struct cdi_mount));

    if (!mount)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    if (parse_cdi(&mount->meta, path) < 0) {
        error_set_file_path(path);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    mount_insert(&cdi_mount_ops, mount);
}

static void cdi_cleanup(struct mount *mount) {
    struct cdi_mount *cdi_mount = (struct cdi_mount*)mount->state;

    unsigned session_no;
    for (session_no = 0;
         session_no < cdi_mount->meta.n_sessions; session_no++) {
        free(cdi_mount->meta.sessions[session_no].tracks);
    }

    washdc_hostfile_close(cdi_mount->meta.fp);

    free(cdi_mount);
}

bool cdi_image_is_valid( washdc_hostfile f )
{
    int len;
    struct cdi_trailer trail;

    washdc_hostfile_seek( f, -8, WASHDC_HOSTFILE_SEEK_END );
    len = washdc_hostfile_tell(f)+8;
    if (washdc_hostfile_read(f, &trail, sizeof(trail)) != sizeof(trail))
        return false;
    if( trail.header_offset >= len ||
            trail.header_offset == 0 )
        return false;
    return trail.cdi_version == CDI_V2_ID || trail.cdi_version == CDI_V3_ID ||
    trail.cdi_version == CDI_V35_ID;
}

#define RETURN_PARSE_ERROR( ... ) do { SET_ERROR(err, LX_ERR_FILE_INVALID, __VA_ARGS__); return FALSE; } while(0)

static DEF_ERROR_INT_ATTR(cdrom_mode)

static inline unsigned get_sector_size(unsigned mode) {
    switch (mode) {
    case CDI_SECTOR_SEMIRAW_MODE2:
        return 2336;
    case CDI_SECTOR_CDDA:
        return 2352;
    default:
        error_set_cdrom_mode(mode);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static inline unsigned get_sector_data_offset(unsigned mode) {
    switch (mode) {
    case CDI_SECTOR_SEMIRAW_MODE2:
        return 8;
    case CDI_SECTOR_CDDA:
        return 0; // TODO: not sure how accurate this is but it seems right
    default:
        error_set_cdrom_mode(mode);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

#define CDROM_SECTOR_SIZE(mode) get_sector_size(mode)

static int parse_cdi(struct cdi_info *outp, char const *path)
{
    int i,j;
    uint16_t session_count;
    uint16_t track_count;
    int total_tracks = 0;
    int posn = 0;
    long len;
    struct cdi_trailer trail;
    char marker[20];

    memset(outp, 0, sizeof(*outp));

    washdc_hostfile f =
        washdc_hostfile_open(path,
                             WASHDC_HOSTFILE_READ | WASHDC_HOSTFILE_BINARY);

    if (f == WASHDC_HOSTFILE_INVALID)
        goto on_error;

    if (!cdi_image_is_valid(f))
        goto on_error;

    if (!f)
        return -1;
    washdc_hostfile_seek(f, -8, WASHDC_HOSTFILE_SEEK_END);
    len = washdc_hostfile_tell(f)+8;
    if (washdc_hostfile_read(f, &trail, sizeof(trail)) != sizeof(trail))
        goto on_error;
    if( trail.header_offset >= len ||
        trail.header_offset == 0 )
        goto on_error;

    if( trail.cdi_version != CDI_V2_ID && trail.cdi_version != CDI_V3_ID &&
        trail.cdi_version != CDI_V35_ID )
        goto on_error;

    if( trail.cdi_version == CDI_V35_ID ) {
        washdc_hostfile_seek(f, -(long)trail.header_offset,
                             WASHDC_HOSTFILE_SEEK_END);
    } else {
        washdc_hostfile_seek(f, trail.header_offset, WASHDC_HOSTFILE_SEEK_BEG);
    }
    if (washdc_hostfile_read(f, &session_count, sizeof(session_count)) !=
        sizeof(session_count))
        goto on_error;

    outp->sessions = calloc(session_count, sizeof(outp->sessions[0]));
    if (!outp->sessions)
        goto on_error;
    outp->n_sessions = session_count;

    for( i=0; i< session_count; i++ ) {
        if (washdc_hostfile_read(f, &track_count, sizeof(track_count)) !=
            sizeof(track_count))
            goto on_error;
        if( (i != session_count-1 && track_count < 1) || track_count > 99 )
            goto on_error;
        if( track_count + total_tracks > 99 ) {
            LOG_ERROR("Invalid number of tracks in disc, bad cdi image" );
            goto on_error;
        }
        outp->sessions[i].tracks = calloc(track_count, sizeof(outp->sessions[i].tracks[0]));
        if (!outp->sessions[i].tracks)
            goto on_error;
        outp->sessions[i].first_track = total_tracks;
        outp->sessions[i].n_tracks = track_count;
        for( j=0; j<track_count; j++ ) {
            struct cdi_track_data trk;
            uint32_t new_fmt = 0;
            uint8_t fnamelen = 0;
            if (washdc_hostfile_read(f, &new_fmt, sizeof(new_fmt)) !=
                sizeof(new_fmt))
                goto on_error;
            if( new_fmt != 0 ) { /* Additional data 3.00.780+ ?? */
                washdc_hostfile_seek(f, 8, WASHDC_HOSTFILE_SEEK_CUR); /* Skip */
            }
            if (washdc_hostfile_read(f, &marker, 20) != 20)
                goto on_error;
            if( memcmp( marker, TRACK_START_MARKER, 20) != 0 ) {
                LOG_ERROR( "Track start marker not found, error reading cdi image" );
                goto on_error;
            }
            washdc_hostfile_seek(f, 4, WASHDC_HOSTFILE_SEEK_CUR);
            if (washdc_hostfile_read(f, &fnamelen, 1) != 1)
                goto on_error;

            /* skip over the filename */
            washdc_hostfile_seek(f, (int)fnamelen, WASHDC_HOSTFILE_SEEK_CUR);
            washdc_hostfile_seek(f, 19, WASHDC_HOSTFILE_SEEK_CUR );

            if (washdc_hostfile_read(f, &new_fmt, sizeof(new_fmt)) !=
                sizeof(new_fmt))
                goto on_error;
            if( new_fmt == 0x80000000 ) {
                washdc_hostfile_seek(f, 10, WASHDC_HOSTFILE_SEEK_CUR);
            } else {
                washdc_hostfile_seek(f, 2, WASHDC_HOSTFILE_SEEK_CUR);
            }
            if (washdc_hostfile_read(f, &trk, sizeof(trk)) != sizeof(trk))
                goto on_error;
            outp->sessions[i].tracks[j].lba= trk.start_lba;
            unsigned sector_count = trk.length;
            outp->sessions[i].tracks[j].n_sectors = sector_count;
            enum cdi_mode mode;
            switch( trk.mode ) {
            case 0:
                mode = CDI_SECTOR_CDDA;
                outp->sessions[i].tracks[j].flags = 0x01;
                if( trk.sector_size != 2 ) {
                    LOG_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                    washdc_hostfile_close(f);
                    return -1;
                }
                break;
            case 1:
                mode = CDI_SECTOR_MODE1;
                outp->sessions[i].tracks[j].flags = 0x41;
                if( trk.sector_size != 0 ) {
                    LOG_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                    washdc_hostfile_close(f);
                    return -1;
                }
                break;
            case 2:
                outp->sessions[i].tracks[j].flags = 0x41;
                switch( trk.sector_size ) {
                case 0:
                    mode = CDI_SECTOR_MODE2_FORM1;
                    break;
                case 1:
                    mode = CDI_SECTOR_SEMIRAW_MODE2;
                    break;
                case 2:
                default:
                    LOG_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                    washdc_hostfile_close(f);
                    return -1;
                }
                break;
            default:
                LOG_ERROR( "Unsupported track mode %d", trk.mode );
                washdc_hostfile_close(f);
                return -1;
            }
            outp->sessions[i].tracks[j].mode = mode;
            uint32_t offset = posn +
                    trk.pregap_length * CDROM_SECTOR_SIZE(mode);
            outp->sessions[i].tracks[j].offset = offset;
            posn += trk.total_length * CDROM_SECTOR_SIZE(mode);
            total_tracks++;
            if( trail.cdi_version != CDI_V2_ID ) {
                uint32_t extmarker;
                washdc_hostfile_seek(f, 5, WASHDC_HOSTFILE_SEEK_CUR);
                if (washdc_hostfile_read(f, &extmarker, sizeof(extmarker)) !=
                    sizeof(extmarker))
                    goto on_error;
                if( extmarker == 0xFFFFFFFF )  {
                    washdc_hostfile_seek(f, 78, WASHDC_HOSTFILE_SEEK_CUR);
                }
            }
        }
        washdc_hostfile_seek(f, 12, WASHDC_HOSTFILE_SEEK_CUR);
        if( trail.cdi_version != CDI_V2_ID ) {
            washdc_hostfile_seek(f, 1, WASHDC_HOSTFILE_SEEK_CUR);
        }
    }

    outp->fp = f;

    cdi_dump(outp);

    return 0;

 on_error:
    LOG_ERROR( "Invalid CDI image\n" );

    unsigned sess_idx;
    for (sess_idx = 0; sess_idx < outp->n_sessions; sess_idx++)
        free(outp->sessions[sess_idx].tracks);

    free(outp->sessions);
    washdc_hostfile_close(f);
    return -1;
}

static unsigned cdi_get_session_count(struct mount *mount) {
    return ((struct cdi_mount const*)mount->state)->meta.n_sessions;
}

static enum mount_disc_type cdi_get_disc_type(struct mount* mount) {
    return DISC_TYPE_CDROM_XA;
}

static int cdi_get_meta(struct mount *mount, struct mount_meta *meta) {
    memset(meta, 0, sizeof(*meta));

    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_info const *info = &cdi_mount->meta;

    if (info->n_sessions < 2) {
        LOG_ERROR("Unable to fetch image metadata: not enough sessions\n");
        return -1;
    }

    struct cdi_session *sess = info->sessions + 1;

    if (sess->n_tracks == 0) {
        LOG_ERROR("Unable to fetch image metadata: no tracks on second "
                  "session\n");
        return -1;
    }

    if (sess->tracks[0].n_sectors == 0) {
        LOG_ERROR("Unable to fetch image metadata: no data on first track of "
                  "second session.\n");
        return -1;
    }

    unsigned fad = cdrom_lba_to_fad(sess->tracks[0].lba);
    uint8_t buffer[2048];
    if (cdi_read_sector(mount, buffer, fad) < 0) {
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

static int cdi_read_toc(struct mount *mount, struct mount_toc *toc,
                        unsigned region) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_info const *info = &cdi_mount->meta;

    if (region != MOUNT_LD_REGION)
        return -1;

    memset(toc->tracks, 0, sizeof(toc->tracks));

    unsigned track_no;
    unsigned total_tracks = 0;
    unsigned sess_idx;
    for (sess_idx = 0; sess_idx < info->n_sessions; sess_idx++) {
        struct cdi_session *session = info->sessions + sess_idx;

        for (track_no = 0; track_no < session->n_tracks; track_no++) {
            unsigned track_idx = track_no + total_tracks;
            struct cdi_track *track = session->tracks + track_no;

            toc->tracks[track_idx].fad = cdrom_lba_to_fad(track->lba);
            toc->tracks[track_idx].adr = track->flags & 1;
            toc->tracks[track_idx].ctrl = (track->flags >> 4) & 0xf;
            toc->tracks[track_idx].valid = true;

            total_tracks++;
        }
    }
    toc->first_track = info->sessions[0].first_track + 1;
    toc->last_track = total_tracks;
    toc->leadout = cdrom_lba_to_fad(cdi_get_leadout(mount));
    toc->leadout_adr = 1;

    return 0;
}

static void cdi_get_session_start(struct mount *mount, unsigned session_no,
                                  unsigned *start_track, unsigned *fad) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_info const *info = &cdi_mount->meta;

    if (session_no >= info->n_sessions)
        RAISE_ERROR(ERROR_INTEGRITY);

    struct cdi_session *session = info->sessions + session_no;
    struct cdi_track *first_track = session->tracks /* + session->first_track */;

    *start_track = session->first_track;
    *fad = cdrom_lba_to_fad(first_track->lba);
}

static int cdi_read_sector(struct mount *mount, void *buf, unsigned fad) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_info const *info = &cdi_mount->meta;

    unsigned lba = cdrom_fad_to_lba(fad);
    LOG_INFO("CDI Request to read LBA %u\n", lba);
    unsigned track_no, session_no;
    for (session_no = 0; session_no < info->n_sessions; session_no++) {
        struct cdi_session *session = info->sessions + session_no;
        for (track_no = 0; track_no < session->n_tracks; track_no++) {
            struct cdi_track *track = session->tracks + track_no;
            if (lba >= track->lba && lba < track->lba + track->n_sectors) {
                LOG_INFO("Session %u, track %u\n", session_no, track_no);
                LOG_INFO("\ttrack offset is %X\n", track->offset);
                LOG_INFO("\ttrack lba is %u\n", track->lba);
                LOG_INFO("\ttrack has %u sectors\n", track->n_sectors);
                unsigned lba_rel = lba - track->lba;
                LOG_INFO("\tlba_rel is %u\n", lba_rel);
                unsigned byte_offset = get_sector_size(track->mode) * lba_rel
                    + get_sector_data_offset(track->mode) + track->offset;
                LOG_INFO("\tbyte_offset is %X\n", byte_offset);
                LOG_INFO("\tmode is %X\n", track->mode);

                if (track->mode == CDI_SECTOR_CDDA) {
                    LOG_WARN("\tThis track is a CDDA track.  I'm not sure if "
                             "you\'re supposed to be able to read data from "
                             "it or not.\n");
                }

                washdc_hostfile_seek(info->fp, byte_offset, WASHDC_HOSTFILE_SEEK_BEG);
                if (washdc_hostfile_read(info->fp, buf, 2048) != 2048)
                    return -1;
                return 0;
            }
        }
    }
    return -1;
}

static void cdi_dump(struct cdi_info const *info) {
    unsigned sess_no;
    LOG_INFO("%u sessions\n", info->n_sessions);
    for (sess_no = 0; sess_no < info->n_sessions; sess_no++) {
        LOG_INFO("session %u\n", sess_no + 1);
        struct cdi_session *sess = info->sessions + sess_no;
        LOG_INFO("\t%u tracks\n", sess->n_tracks);
        unsigned track_no;
        for (track_no = 0; track_no < sess->n_tracks; track_no++) {
            LOG_INFO("\ttrack %u\n", track_no + 1);
            struct cdi_track *track = sess->tracks + track_no;
            LOG_INFO("\t\tlba: %u\n", track->lba);
            LOG_INFO("\t\tn_sectors: %u\n", track->n_sectors);
            LOG_INFO("\t\toffset: %u\n", track->offset);
            LOG_INFO("\t\tmode: %u\n", (unsigned)track->mode);
        }
    }
}

static unsigned cdi_get_leadout(struct mount *mount) {
    struct cdi_mount const *cdi_mount = (struct cdi_mount const*)mount->state;
    struct cdi_info const *info = &cdi_mount->meta;
    struct cdi_session const *last_session = info->sessions + (info->n_sessions - 1);
    struct cdi_track const *last_track =
        last_session->tracks + (last_session->n_tracks - 1);
    return last_track->lba + last_track->n_sectors;
}

static bool cdi_has_hd_region(struct mount *mount) {
    return false;
}
