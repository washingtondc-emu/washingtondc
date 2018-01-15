/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include <stdio.h>

#include "error.h"
#include "geo_buf.h"
#include "hw/pvr2/pvr2_ta.h" // for TexCtrlPixFmt

#include "geo_buf_serial.h"

#define WRITE_VAL(var)                                              \
    if (fwrite(&(var), sizeof(var), 1, stream) != 1) {              \
        goto on_io_error;                                           \
    }

static int save_geo_buf_tex(struct geo_buf_tex const *tex, FILE *stream) {
    WRITE_VAL(tex->pix_fmt);
    WRITE_VAL(tex->w_shift);
    WRITE_VAL(tex->h_shift);
    WRITE_VAL(tex->frame_stamp_last_used);
    WRITE_VAL(tex->state);

    unsigned bpp;
    switch (tex->pix_fmt) {
    case TEX_CTRL_PIX_FMT_ARGB_1555:
    case TEX_CTRL_PIX_FMT_RGB_565:
    case TEX_CTRL_PIX_FMT_ARGB_4444:
        bpp = 2;
        break;
    case TEX_CTRL_PIX_FMT_YUV_422:
    case TEX_CTRL_PIX_FMT_8_BPP_PAL:
        bpp = 1;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned n_pix = 1 << (tex->w_shift + tex->h_shift);

    if (tex->state == GEO_BUF_TEX_DIRTY)
        if (fwrite(tex->dat, bpp, n_pix, stream) != n_pix)
            goto on_io_error;

    return 0;

 on_io_error:
    LOG_ERROR("Unable to save geo_buf texture\n");
    return -1;
}

static int save_poly_group(struct poly_group const *grp, FILE *stream) {
    WRITE_VAL(grp->n_verts);
    if (fwrite(grp->verts, sizeof(float) * GEO_BUF_VERT_LEN,
               GEO_BUF_VERT_COUNT, stream) != GEO_BUF_VERT_COUNT)
        goto on_io_error;

    WRITE_VAL(grp->tex_enable);
    WRITE_VAL(grp->tex_idx);
    WRITE_VAL(grp->tex_inst);
    WRITE_VAL(grp->tex_filter);
    WRITE_VAL(grp->tex_wrap_mode[0]);
    WRITE_VAL(grp->tex_wrap_mode[1]);
    WRITE_VAL(grp->src_blend_factor);
    WRITE_VAL(grp->dst_blend_factor);
    WRITE_VAL(grp->enable_depth_writes);
    WRITE_VAL(grp->depth_func);

    return 0;

 on_io_error:
    LOG_ERROR("Unable to save geo_buf poly_group\n");
    return -1;
}

static int save_display_list(struct display_list const *dl, FILE *stream) {
    unsigned group_no;
    WRITE_VAL(dl->n_groups);
    for (group_no = 0; group_no < dl->n_groups; group_no++)
        if (save_poly_group(dl->groups + group_no, stream) < 0)
            goto on_io_error;
    WRITE_VAL(dl->blend_enable);

    return 0;

 on_io_error:
    LOG_ERROR("Unable to save geo_buf display_list\n");
    return -1;
}

void save_geo_buf(struct geo_buf const *geo, FILE *stream) {
    unsigned tex_no;
    for (tex_no = 0; tex_no < GEO_BUF_TEX_CACHE_SIZE; tex_no++)
        if (save_geo_buf_tex(geo->tex_cache + tex_no, stream) != 0)
            goto on_io_error;

    unsigned list_no;
    for (list_no = 0; list_no < DISPLAY_LIST_COUNT; list_no++)
        save_display_list(geo->lists + list_no, stream);

    WRITE_VAL(geo->frame_stamp);
    WRITE_VAL(geo->screen_width);
    WRITE_VAL(geo->screen_height);
    WRITE_VAL(geo->bgcolor[0]);
    WRITE_VAL(geo->bgcolor[1]);
    WRITE_VAL(geo->bgcolor[2]);
    WRITE_VAL(geo->bgcolor[3]);
    WRITE_VAL(geo->bgdepth);
    WRITE_VAL(geo->clip_min);
    WRITE_VAL(geo->clip_max);

    return;

 on_io_error:
    LOG_ERROR("unable to save geo_buf\n");
}
