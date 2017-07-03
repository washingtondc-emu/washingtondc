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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geo_buf.h"
#include "gfx_thread.h"
#include "hw/sys/holly_intc.h"
#include "pvr2_core_reg.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"

#include "pvr2_ta.h"

#define PVR2_CMD_MAX_LEN 64

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_END_OF_STRIP_SHIFT 28
#define TA_CMD_END_OF_STRIP_MASK (1 << TA_CMD_END_OF_STRIP_SHIFT)

#define TA_CMD_DISP_LIST_SHIFT 24
#define TA_CMD_DISP_LIST_MASK (0x7 << TA_CMD_DISP_LIST_SHIFT)

#define TA_CMD_TEX_ENABLE_SHIFT 3
#define TA_CMD_TEX_ENABLE_MASK (1 << TA_CMD_TEX_ENABLE_SHIFT)

#define TA_CMD_TYPE_END_OF_LIST 0x0
#define TA_CMD_TYPE_USER_CLIP   0x1
// what is 2?
// what is 3?
#define TA_CMD_TYPE_POLY_HDR    0x4
#define TA_CMD_TYPE_SPRITE      0x5
// what is 6?
#define TA_CMD_TYPE_VERTEX      0x7

#define TA_COLOR_FMT_ARGB8888 0
#define TA_COLOR_FMT_FLOAT    1
// TODO: there ought to be two more color formats...

#define TA_COLOR_FMT_SHIFT 4
#define TA_COLOR_FMT_MASK (3 << TA_COLOR_FMT_SHIFT)

#define ISP_BACKGND_T_ADDR_SHIFT 1
#define ISP_BACKGND_T_ADDR_MASK (0x7ffffc << ISP_BACKGND_T_ADDR_SHIFT)

#define ISP_BACKGND_T_SKIP_SHIFT 24
#define ISP_BACKGND_T_SKIP_MASK (7 << ISP_BACKGND_T_SKIP_SHIFT)

static uint8_t ta_fifo[PVR2_CMD_MAX_LEN];

static unsigned expected_ta_fifo_len = 32;
static unsigned ta_fifo_byte_count = 0;

static struct poly_state {
    // used to store the previous two verts when we're rendering a triangle strip
    float strip_vert1[GEO_BUF_VERT_LEN];
    float strip_vert2[GEO_BUF_VERT_LEN];
    unsigned strip_len; // number of verts in the current triangle strip

    unsigned ta_color_fmt;

    bool tex_enable;
    unsigned tex_idx;

    int tex_fmt;
} poly_state;

enum display_list {
    DISPLAY_LIST_OPAQUE,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

char const *display_list_names[DISPLAY_LIST_COUNT] = {
    "Opaque",
    "Opaque Modifier Volume",
    "Transparent",
    "Transparent Modifier Volume",
    "Punch-through Polygon"
};

bool list_submitted[DISPLAY_LIST_COUNT];

// which display list is currently open
static enum display_list current_list = DISPLAY_LIST_NONE;

static void input_poly_fifo(uint8_t byte);

// this function gets called every time a full packet is received by the TA
static void on_packet_received(void);
static void on_polyhdr_received(void);
static void on_vertex_received(void);
static void on_end_of_list_received(void);

int pvr2_ta_fifo_poly_read(void *buf, size_t addr, size_t len) {
#ifdef PVR2_LOG_VERBOSE
    fprintf(stderr, "WARNING: trying to read %u bytes from the TA polygon FIFO "
            "(you get all 0s)\n", (unsigned)len);
#endif
    memset(buf, 0, len);

    return 0;
}

int pvr2_ta_fifo_poly_write(void const *buf, size_t addr, size_t len) {
#ifdef PVR2_LOG_VERBOSE
    fprintf(stderr, "WARNING: writing %u bytes to TA polygon FIFO:\n",
            (unsigned)len);

    unsigned len_copy = len;

    if (len_copy % 4 == 0) {
        uint32_t *ptr = (uint32_t*)buf;
        while (len_copy) {
            fprintf(stderr, "\t%08x\n", (unsigned)*ptr++);
            len_copy -= 4;
        }
    } else {
        uint8_t *ptr = (uint8_t*)buf;
        while (len_copy--)
            fprintf(stderr, "\t%02x\n", (unsigned)*ptr++);
    }
#endif

    uint8_t *ptr = (uint8_t*)buf;
    while (len--)
        input_poly_fifo(*ptr++);

    return 0;
}

static void input_poly_fifo(uint8_t byte) {
    ta_fifo[ta_fifo_byte_count++] = byte;

    if (ta_fifo_byte_count == expected_ta_fifo_len) {
        on_packet_received();
        expected_ta_fifo_len = 32;
        ta_fifo_byte_count = 0;
    }
}

static void on_packet_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch(cmd_tp) {
    case TA_CMD_TYPE_VERTEX:
        on_vertex_received();
        break;
    case TA_CMD_TYPE_POLY_HDR:
        on_polyhdr_received();
        break;
    case TA_CMD_TYPE_END_OF_LIST:
        on_end_of_list_received();
        break;
    default:
        printf("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
    }
}

static void on_polyhdr_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    enum display_list list =
        (enum display_list)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                            TA_CMD_DISP_LIST_SHIFT);

    // reset triangle strips
    poly_state.strip_len = 0;

    if (current_list == DISPLAY_LIST_NONE) {
        if (!list_submitted[list]) {
            printf("Opening display list %s\n", display_list_names[list]);
            current_list = list;
            list_submitted[list] = true;

            poly_state.ta_color_fmt = (ta_fifo32[0] & TA_COLOR_FMT_MASK) >>
                TA_COLOR_FMT_SHIFT;

            if (ta_fifo32[0] & TA_CMD_TEX_ENABLE_MASK) {
                poly_state.tex_enable = true;
                printf("texture enabled\n");

                if (ta_fifo32[3] & TEX_CTRL_NOT_TWIDDLED_MASK)
                    printf("not twiddled\n");
                else
                    printf("twiddled\n");

                printf("the texture format is %d\n",
                       (ta_fifo32[3] & TEX_CTRL_PIX_FMT_MASK) >>
                       TEX_CTRL_PIX_FMT_SHIFT);

                uint32_t tex_addr = ((ta_fifo32[3] & TEX_CTRL_TEX_ADDR_MASK) >>
                                     TEX_CTRL_TEX_ADDR_SHIFT) << 3;
                printf("The texture address ix 0x%08x\n", tex_addr);

                // TODO: how do I get texture width, height?
                unsigned tex_width_shift = 3 +
                    ((ta_fifo32[2] & TSP_TEX_WIDTH_MASK) >> TSP_TEX_WIDTH_SHIFT);
                unsigned tex_height_shift = 3 +
                    ((ta_fifo32[2] & TSP_TEX_HEIGHT_MASK) >> TSP_TEX_HEIGHT_SHIFT);

                unsigned pix_fmt = (ta_fifo32[3] & TEX_CTRL_PIX_FMT_MASK) >>
                    TEX_CTRL_PIX_FMT_SHIFT;
                struct pvr2_tex *ent =
                    pvr2_tex_cache_find(tex_addr, 1 << tex_width_shift,
                                        1 << tex_height_shift, pix_fmt);

                printf("texture dimensions are (%u, %u)\n",
                       1 << tex_width_shift, 1 << tex_height_shift);
                if (ent) {
                    printf("Texture 0x%08x found in cache\n", tex_addr);
                } else {
                    printf("Adding 0x%08x to texture cache...\n", tex_addr);
                    ent = pvr2_tex_cache_add(tex_addr, 1 << tex_width_shift,
                                             1 << tex_height_shift, pix_fmt);
                }

                if (!ent) {
                    fprintf(stderr, "WARNING: failed to add texture 0x%08x to "
                            "the texture cache\n", tex_addr);
                    poly_state.tex_enable = false;
                } else {
                    poly_state.tex_idx = pvr2_tex_cache_get_idx(ent);
                }
            } else {
                /*
                 * XXX - HACK
                 * we only clear the tex_enable flag when the STARTRENDER comes
                 * down because the geo_buf has no concept of separate polygon
                 * headers.  This means that textures caan be enabled for all
                 * polygons in the geo_buf or disabled for all polygons in the
                 * geo_buf.  It also measn that all polygons need to have the
                 * same texture bound.  This will be fixed eventually.
                 */
                /* poly_state.tex_enable = false; */
            }
        } else {
            printf("WARNING: unable to open list %s because it is already "
                   "closed\n", display_list_names[list]);
        }
    } else if (current_list != list) {
        printf("WARNING: attempting to input poly header for list %s without "
               "first closing %s\n", display_list_names[list],
               display_list_names[current_list]);
        return;
    }

    printf("POLY HEADER PACKET!\n");
}

static void on_vertex_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    float const *ta_fifo_float = (float const*)ta_fifo;

#ifdef PVR2_LOG_VERBOSE
    printf("vertex received!\n");
#endif
    struct geo_buf *geo = geo_buf_get_prod();

    if (current_list != DISPLAY_LIST_OPAQUE) {
        /* printf("display list %d ignored\n", current_list); */
        return;
    }

    /*
     * un-strip triangle strips by duplicating the previous two vertices.
     *
     * TODO: obviously it would be best to preserve the triangle strips and
     * send them to OpenGL via GL_TRIANGLE_STRIP in the rendering backend, but
     * then I need to come up with some way to signal the renderer to stop and
     * re-start strips.  It might also be possible to stitch separate strips
     * together with degenerate triangles...
     */
    if (poly_state.strip_len >= 3) {
        if (geo->n_verts < GEO_BUF_VERT_COUNT) {
            memcpy(geo->verts + GEO_BUF_VERT_LEN * geo->n_verts,
                   poly_state.strip_vert1, sizeof(poly_state.strip_vert1));
            geo->n_verts++;
        }

        if (geo->n_verts < GEO_BUF_VERT_COUNT) {
            memcpy(geo->verts + GEO_BUF_VERT_LEN * geo->n_verts,
                   poly_state.strip_vert2, sizeof(poly_state.strip_vert2));
            geo->n_verts++;
        }
    }

    if (geo->n_verts < GEO_BUF_VERT_COUNT) {
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_POS_OFFSET + 0] =
            ta_fifo_float[1];
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_POS_OFFSET + 1] =
            ta_fifo_float[2];
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_POS_OFFSET + 2] =
            ta_fifo_float[3];

        if (poly_state.tex_enable) {
            unsigned dst_uv_offset =
                GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_TEX_COORD_OFFSET;
            geo->verts[dst_uv_offset + 0] = ta_fifo_float[4];
            geo->verts[dst_uv_offset + 1] = ta_fifo_float[5];
        }

        float color_r, color_g, color_b, color_a;

        switch (poly_state.ta_color_fmt) {
        case TA_COLOR_FMT_ARGB8888:
            color_a = (float)((ta_fifo32[6] & 0xff000000) >> 24) / 255.0f;
            color_r = (float)((ta_fifo32[6] & 0x00ff0000) >> 16) / 255.0f;
            color_g = (float)((ta_fifo32[6] & 0x0000ff00) >> 8) / 255.0f;
            color_b = (float)((ta_fifo32[6] & 0x000000ff) >> 0) / 255.0f;
            break;
        default:
            color_r = color_g = color_b = color_a = 1.0f;
            fprintf(stderr, "WARNING: unknown TA color format %u\n", poly_state.ta_color_fmt);
        }

        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_COLOR_OFFSET + 0] =
            color_r;
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_COLOR_OFFSET + 1] =
            color_g;
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_COLOR_OFFSET + 2] =
            color_b;
        geo->verts[GEO_BUF_VERT_LEN * geo->n_verts + GEO_BUF_COLOR_OFFSET + 3] =
            color_a;

        if (ta_fifo32[0] & TA_CMD_END_OF_STRIP_SHIFT) {
            /*
             * TODO: handle degenerate cases where the user sends an
             * end-of-strip on the first or second vertex
             */
            poly_state.strip_len = 0;
        } else {
            /*
             * shift the new vert into strip_vert2 and
             * shift strip_vert2 into strip_vert1
             */
            memcpy(poly_state.strip_vert1, poly_state.strip_vert2,
                   sizeof(poly_state.strip_vert1));
            memcpy(poly_state.strip_vert2,
                   geo->verts + GEO_BUF_VERT_LEN * geo->n_verts,
                   sizeof(poly_state.strip_vert2));
            poly_state.strip_len++;
        }

        geo->n_verts++;
    } else {
        fprintf(stderr, "WARNING: dropped vertices: geo_buf contains %u "
                "verts\n", geo->n_verts);
#ifdef INVARIANTS
        abort();
#endif
    }
}

static void on_end_of_list_received(void) {
    printf("END-OF-LIST PACKET!\n");
    printf("Display list \"%s\" closed\n", display_list_names[current_list]);

    // TODO: In a real dreamcast this probably would not happen instantly
    switch (current_list) {
    case DISPLAY_LIST_OPAQUE:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE);
        break;
    case DISPLAY_LIST_OPAQUE_MOD:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE);
        break;
    case DISPLAY_LIST_TRANS:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE);
        break;
    case DISPLAY_LIST_TRANS_MOD:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE);
        break;
    case DISPLAY_LIST_PUNCH_THROUGH:
        holly_raise_nrm_int(HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE);
        break;
    default:
        printf("WARNING: not raising interrupt for closing of list type %d "
               "(invalid)\n", current_list);
    }

    current_list = DISPLAY_LIST_NONE;
 }

void pvr2_ta_startrender(void) {
    printf("STARTRENDER requested!\n");

    unsigned tile_w = get_glob_tile_clip_x() << 5;
    unsigned tile_h = get_glob_tile_clip_y() << 5;
    unsigned x_clip_min = get_fb_x_clip_min();
    unsigned x_clip_max = get_fb_x_clip_max();
    unsigned y_clip_min = get_fb_y_clip_min();
    unsigned y_clip_max = get_fb_y_clip_max();

    unsigned x_min = x_clip_min;
    unsigned y_min = y_clip_min;
    unsigned x_max = tile_w < x_clip_max ? tile_w : x_clip_max;
    unsigned y_max = tile_h < y_clip_max ? tile_h : y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    /*
     * backgnd_info points to a structure containing some ISP/TSP parameters
     * and three vertices (potentially including texture coordinate and
     * color data).  These are used to draw a background plane.  isp_backgnd_d
     * contians some sort of depth value which is used in auto-sorting mode (I
     * think?).
     *
     * Obviously, I I don't actually understand how this works, nor do I
     * understand why the vertex coordinates are relevant when it's just going
     * to draw an infinite plane, so I just save the background color from the
     * first vertex in the geo_buf so the renderer can use it to glClear.  I
     * also save the depth value from isp_backgnd_d even though I don't have
     * auto-sorting implemented yet.
     *
     * This hack inspired by MAME's powervr2 code.
     */
    uint32_t backgnd_tag = get_isp_backgnd_t();
    addr32_t backgnd_info_addr = (backgnd_tag & ISP_BACKGND_T_ADDR_MASK) >>
        ISP_BACKGND_T_ADDR_SHIFT;
    uint32_t backgnd_skip = ((ISP_BACKGND_T_SKIP_MASK & backgnd_tag) >>
                             ISP_BACKGND_T_SKIP_SHIFT) + 3;
    uint32_t const *backgnd_info = (uint32_t*)(pvr2_tex32_mem + backgnd_info_addr);

    /* printf("background skip is %d\n", (int)backgnd_skip); */

    /* printf("ISP_BACKGND_D is %f\n", (double)frak); */
    /* printf("ISP_BACKGND_T is 0x%08x\n", (unsigned)backgnd_tag); */

    uint32_t bg_color_src = *(backgnd_info + 3 + 0 * backgnd_skip + 3);

    float bg_color_a = (float)((bg_color_src & 0xff000000) >> 24) / 255.0f;
    float bg_color_r = (float)((bg_color_src & 0x00ff0000) >> 16) / 255.0f;
    float bg_color_g = (float)((bg_color_src & 0x0000ff00) >> 8) / 255.0f;
    float bg_color_b = (float)((bg_color_src & 0x000000ff) >> 0) / 255.0f;
    geo_buf_get_prod()->bgcolor[0] = bg_color_r;
    geo_buf_get_prod()->bgcolor[1] = bg_color_g;
    geo_buf_get_prod()->bgcolor[2] = bg_color_b;
    geo_buf_get_prod()->bgcolor[3] = bg_color_a;

    uint32_t backgnd_depth_as_int = get_isp_backgnd_d();
    memcpy(&geo_buf_get_prod()->bgdepth, &backgnd_depth_as_int, sizeof(float));

    geo_buf_get_prod()->screen_width = width;
    geo_buf_get_prod()->screen_height = height;

    pvr2_tex_cache_xmit(geo_buf_get_prod());

    if (poly_state.tex_enable) {
        printf("tex_enable should be true\n");
        geo_buf_get_prod()->tex_enable = true;
        geo_buf_get_prod()->tex_idx = poly_state.tex_idx;
    } else {
        printf("tex_enable should be false\n");
        geo_buf_get_prod()->tex_enable = false;
    }

    geo_buf_produce();
    gfx_thread_render_geo_buf();

    memset(list_submitted, 0, sizeof(list_submitted));
    current_list = DISPLAY_LIST_NONE;

    // TODO: This irq definitely should not be triggered immediately
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);

    poly_state.tex_enable = false;
}

void pvr2_ta_reinit(void) {
    memset(list_submitted, 0, sizeof(list_submitted));
}
