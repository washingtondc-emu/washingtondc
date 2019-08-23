/**
 * $Id$
 *
 * CDI CD-image file support
 *
 * Copyright (c) 2005 Nathan Keynes.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "drivers/cdrom/cdimpl.h"

#define CDI_V2_ID 0x80000004
#define CDI_V3_ID 0x80000005
#define CDI_V35_ID 0x80000006


static gboolean cdi_image_is_valid( FILE *f );
static gboolean cdi_image_read_toc( cdrom_disc_t disc, ERROR *err );

struct cdrom_disc_factory cdi_disc_factory = { "DiscJuggler", "cdi",
        cdi_image_is_valid, NULL, cdi_image_read_toc };

static const char TRACK_START_MARKER[20] = { 0,0,1,0,0,0,255,255,255,255,
        0,0,1,0,0,0,255,255,255,255 };
//static const char EXT_MARKER[9] = {0,255,255,255,255,255,255,255,255 };

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

gboolean cdi_image_is_valid( FILE *f )
{
    int len;
    struct cdi_trailer trail;

    fseek( f, -8, SEEK_END );
    len = ftell(f)+8;
    fread( &trail, sizeof(trail), 1, f );
    if( trail.header_offset >= len ||
            trail.header_offset == 0 )
        return FALSE;
    return trail.cdi_version == CDI_V2_ID || trail.cdi_version == CDI_V3_ID ||
    trail.cdi_version == CDI_V35_ID;
}

#define RETURN_PARSE_ERROR( ... ) do { SET_ERROR(err, LX_ERR_FILE_INVALID, __VA_ARGS__); return FALSE; } while(0)

static gboolean cdi_image_read_toc( cdrom_disc_t disc, ERROR *err )
{
    int i,j;
    uint16_t session_count;
    uint16_t track_count;
    int total_tracks = 0;
    int posn = 0;
    long len;
    struct cdi_trailer trail;
    char marker[20];

    FILE *f = cdrom_disc_get_base_file(disc);
    fseek( f, -8, SEEK_END );
    len = ftell(f)+8;
    fread( &trail, sizeof(trail), 1, f );
    if( trail.header_offset >= len ||
            trail.header_offset == 0 ) {
        RETURN_PARSE_ERROR( "Invalid CDI image" );
    }

    if( trail.cdi_version != CDI_V2_ID && trail.cdi_version != CDI_V3_ID &&
            trail.cdi_version != CDI_V35_ID ) {
        RETURN_PARSE_ERROR( "Invalid CDI image" );
    }

    if( trail.cdi_version == CDI_V35_ID ) {
        fseek( f, -(long)trail.header_offset, SEEK_END );
    } else {
        fseek( f, trail.header_offset, SEEK_SET );
    }
    fread( &session_count, sizeof(session_count), 1, f );

    for( i=0; i< session_count; i++ ) {        
        fread( &track_count, sizeof(track_count), 1, f );
        if( (i != session_count-1 && track_count < 1) || track_count > 99 ) {
            RETURN_PARSE_ERROR("Invalid number of tracks (%d), bad cdi image", track_count);
        }
        if( track_count + total_tracks > 99 ) {
            RETURN_PARSE_ERROR("Invalid number of tracks in disc, bad cdi image" );
        }
        for( j=0; j<track_count; j++ ) {
            struct cdi_track_data trk;
            uint32_t new_fmt = 0;
            uint8_t fnamelen = 0;
            fread( &new_fmt, sizeof(new_fmt), 1, f );
            if( new_fmt != 0 ) { /* Additional data 3.00.780+ ?? */
                fseek( f, 8, SEEK_CUR ); /* Skip */
            }
            fread( marker, 20, 1, f );
            if( memcmp( marker, TRACK_START_MARKER, 20) != 0 ) {
                RETURN_PARSE_ERROR( "Track start marker not found, error reading cdi image" );
            }
            fseek( f, 4, SEEK_CUR );
            fread( &fnamelen, 1, 1, f );
            fseek( f, (int)fnamelen, SEEK_CUR ); /* skip over the filename */
            fseek( f, 19, SEEK_CUR );
            fread( &new_fmt, sizeof(new_fmt), 1, f );
            if( new_fmt == 0x80000000 ) {
                fseek( f, 10, SEEK_CUR );
            } else {
                fseek( f, 2, SEEK_CUR );
            }
            fread( &trk, sizeof(trk), 1, f );
            disc->track[total_tracks].sessionno = i+1;
            disc->track[total_tracks].lba = trk.start_lba;
            cdrom_count_t sector_count = trk.length;
            sector_mode_t mode;
            switch( trk.mode ) {
            case 0:
                mode = SECTOR_CDDA;
                disc->track[total_tracks].flags = 0x01;
                if( trk.sector_size != 2 ) {
                    RETURN_PARSE_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                }
                break;
            case 1:
                mode = SECTOR_MODE1;
                disc->track[total_tracks].flags = 0x41;
                if( trk.sector_size != 0 ) {
                    RETURN_PARSE_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                }
                break;
            case 2:
                disc->track[total_tracks].flags = 0x41;
                switch( trk.sector_size ) {
                case 0:
                    mode = SECTOR_MODE2_FORM1;
                    break;
                case 1:
                    mode = SECTOR_SEMIRAW_MODE2;
                    break;
                case 2:
                default:
                    RETURN_PARSE_ERROR( "Invalid combination of mode %d with size %d", trk.mode, trk.sector_size );
                }
                break;
            default:
                RETURN_PARSE_ERROR( "Unsupported track mode %d", trk.mode );
            }
            uint32_t offset = posn +
                    trk.pregap_length * CDROM_SECTOR_SIZE(mode);
            disc->track[total_tracks].source = file_sector_source_new_source( disc->base_source, mode, offset, sector_count );
            posn += trk.total_length * CDROM_SECTOR_SIZE(mode);
            total_tracks++;
            if( trail.cdi_version != CDI_V2_ID ) {
                uint32_t extmarker;
                fseek( f, 5, SEEK_CUR );
                fread( &extmarker, sizeof(extmarker), 1, f);
                if( extmarker == 0xFFFFFFFF )  {
                    fseek( f, 78, SEEK_CUR );
                }
            }
        }
        fseek( f, 12, SEEK_CUR );
        if( trail.cdi_version != CDI_V2_ID ) {
            fseek( f, 1, SEEK_CUR );
        }
    }

    disc->track_count = total_tracks;
    disc->session_count = session_count;
    return TRUE;
}
