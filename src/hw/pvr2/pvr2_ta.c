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

#include "error.h"
#include "geo_buf.h"
#include "gfx/gfx_thread.h"
#include "hw/sys/holly_intc.h"
#include "pvr2_core_reg.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"

#include "pvr2_ta.h"

#define PVR2_CMD_MAX_LEN 64

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_END_OF_STRIP_SHIFT 28
#define TA_CMD_END_OF_STRIP_MASK (1 << TA_CMD_END_OF_STRIP_SHIFT)

#define TA_CMD_DISP_LIST_SHIFT 24
#define TA_CMD_DISP_LIST_MASK (0x7 << TA_CMD_DISP_LIST_SHIFT)

/*
 * this has something to do with swapping out the ISP parameters
 * when modifier volumes are in use, I think
 */
#define TA_CMD_SHADOW_SHIFT 7
#define TA_CMD_SHADOW_MASK (1 << TA_CMD_SHADOW_SHIFT)

#define TA_CMD_TWO_VOLUMES_SHIFT 6
#define TA_CMD_TWO_VOLUMES_MASK (1 << TA_CMD_TWO_VOLUMES_SHIFT)

#define TA_CMD_COLOR_TYPE_SHIFT 4
#define TA_CMD_COLOR_TYPE_MASK (3 << TA_CMD_COLOR_TYPE_SHIFT)

#define TA_CMD_TEX_ENABLE_SHIFT 3
#define TA_CMD_TEX_ENABLE_MASK (1 << TA_CMD_TEX_ENABLE_SHIFT)

#define TA_CMD_OFFSET_COLOR_SHIFT 2
#define TA_CMD_OFFSET_COLOR_MASK (1 << TA_CMD_OFFSET_COLOR_SHIFT)

#define TA_CMD_GOURAD_SHADING_SHIFT 1
#define TA_CMD_GOURAD_SHADING_MASK (1 << TA_CMD_GOURAD_SHADING_SHIFT)

#define TA_CMD_16_BIT_TEX_COORD_SHIFT 0
#define TA_CMD_16_BIT_TEX_COORD_MASK (1 << TA_CMD_16_BIT_TEX_COORD_SHIFT)

enum ta_color_type {
    TA_COLOR_TYPE_PACKED,
    TA_COLOR_TYPE_FLOAT,
    TA_COLOR_TYPE_INTENSITY_MODE_1,
    TA_COLOR_TYPE_INTENSITY_MODE_2
};

#define TA_CMD_TYPE_END_OF_LIST 0x0
#define TA_CMD_TYPE_USER_CLIP   0x1
#define TA_CMD_TYPE_INPUT_LIST  0x2
// what is 3?
#define TA_CMD_TYPE_POLY_HDR    0x4
#define TA_CMD_TYPE_SPRITE_HDR  0x5
#define TA_CMD_TYPE_UNKNOWN     0x6  // I can't find any info on what this is
#define TA_CMD_TYPE_VERTEX      0x7

#define TA_COLOR_FMT_SHIFT 4
#define TA_COLOR_FMT_MASK (3 << TA_COLOR_FMT_SHIFT)

#define ISP_BACKGND_T_ADDR_SHIFT 1
#define ISP_BACKGND_T_ADDR_MASK (0x7ffffc << ISP_BACKGND_T_ADDR_SHIFT)

#define ISP_BACKGND_T_SKIP_SHIFT 24
#define ISP_BACKGND_T_SKIP_MASK (7 << ISP_BACKGND_T_SKIP_SHIFT)

#define TSP_WORD_SRC_ALPHA_FACTOR_SHIFT 29
#define TSP_WORD_SRC_ALPHA_FACTOR_MASK (7 << TSP_WORD_SRC_ALPHA_FACTOR_SHIFT)

#define TSP_WORD_DST_ALPHA_FACTOR_SHIFT 26
#define TSP_WORD_DST_ALPHA_FACTOR_MASK (7 << TSP_WORD_DST_ALPHA_FACTOR_SHIFT)

#define DEPTH_FUNC_SHIFT 29
#define DEPTH_FUNC_MASK (7 << DEPTH_FUNC_SHIFT)

#define DEPTH_WRITE_DISABLE_SHIFT 26
#define DEPTH_WRITE_DISABLE_MASK (1 << DEPTH_WRITE_DISABLE_SHIFT)

static uint8_t ta_fifo[PVR2_CMD_MAX_LEN];

static unsigned ta_fifo_byte_count = 0;

enum vert_type {
    VERT_NO_TEX_PACKED_COLOR,
    VERT_NO_TEX_FLOAT_COLOR,
    VERT_NO_TEX_INTENSITY,
    VERT_TEX_PACKED_COLOR,
    VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD,
    VERT_TEX_FLOATING_COLOR,
    VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD,
    VERT_TEX_INTENSITY,
    VERT_TEX_INTENSITY_16_BIT_TEX_COORD,
    VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES,
    VERT_NO_TEX_INTENSITY_TWO_VOLUMES,
    VERT_TEX_PACKED_COLOR_TWO_VOLUMES,
    VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD,
    VERT_TEX_INTENSITY_TWO_VOLUMES,
    VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD,

    N_VERT_TYPES
};

static DEF_ERROR_INT_ATTR(ta_fifo_cmd);
static DEF_ERROR_INT_ATTR(pvr2_global_param);

enum global_param {
    GLOBAL_PARAM_POLY = 4,
    GLOBAL_PARAM_SPRITE = 5
};

struct poly_hdr {
    enum display_list_type list;

    bool tex_enable;
    uint32_t tex_addr;
    unsigned tex_width_shift, tex_height_shift;
    bool tex_twiddle;
    bool tex_vq_compression;
    int tex_fmt;
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;

    unsigned ta_color_fmt;
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    bool shadow;
    bool two_volumes_mode;
    enum ta_color_type color_type;
    bool offset_color_enable;
    bool gourad_shading_enable;
    bool tex_coord_16_bit_enable;

    float poly_color_rgba[4];
};

static struct poly_state {
    enum global_param global_param;

    // used to store the previous two verts when we're rendering a triangle strip
    float strip_vert1[GEO_BUF_VERT_LEN];
    float strip_vert2[GEO_BUF_VERT_LEN];
    unsigned strip_len; // number of verts in the current triangle strip

    unsigned ta_color_fmt;

    bool tex_enable;

    // index into the texture cache
    unsigned tex_idx;

    // which display list is currently open
    enum display_list_type current_list;

    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    bool shadow;
    bool two_volumes_mode;
    enum ta_color_type color_type;
    bool offset_color_enable;
    bool gourad_shading_enable;
    bool tex_coord_16_bit_enable;

    enum tex_inst tex_inst;
    enum tex_filter tex_filter;

    // number of 4-byte quads per vertex
    unsigned vert_len;

    float poly_color_rgba[4];

    enum vert_type vert_type;
} poly_state = {
    .current_list = DISPLAY_LIST_NONE,
    .vert_len = 8
};

char const *display_list_names[DISPLAY_LIST_COUNT] = {
    "Opaque",
    "Opaque Modifier Volume",
    "Transparent",
    "Transparent Modifier Volume",
    "Punch-through Polygon"
};

// lengths of each type of vert, in terms of 32-bit integers
static const unsigned vert_lengths[N_VERT_TYPES] = {
    [VERT_NO_TEX_PACKED_COLOR] = 8,
    [VERT_NO_TEX_FLOAT_COLOR] = 8,
    [VERT_NO_TEX_INTENSITY] = 8,
    [VERT_TEX_PACKED_COLOR] = 8,
    [VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD] = 8,
    [VERT_TEX_FLOATING_COLOR] = 16,
    [VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD] = 16,
    [VERT_TEX_INTENSITY] = 8,
    [VERT_TEX_INTENSITY_16_BIT_TEX_COORD] = 8,
    [VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES] = 8,
    [VERT_NO_TEX_INTENSITY_TWO_VOLUMES] = 8,
    [VERT_TEX_PACKED_COLOR_TWO_VOLUMES] = 16,
    [VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD] = 16,
    [VERT_TEX_INTENSITY_TWO_VOLUMES] = 16,
    [VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD] = 16
};

bool list_submitted[DISPLAY_LIST_COUNT];

static void input_poly_fifo(uint8_t byte);

// this function gets called every time a full packet is received by the TA
static void on_packet_received(void);
static void on_polyhdr_received(void);
static void on_vertex_received(void);
static void on_sprite_received(void);
static void on_end_of_list_received(void);
static void on_user_clip_received(void);

static void finish_poly_group(struct geo_buf *geo,
                              enum display_list_type disp_list);
static void next_poly_group(struct geo_buf *geo,
                            enum display_list_type disp_list);

static void decode_poly_hdr(struct poly_hdr *hdr);

// call this whenever a packet has been processed
static void ta_fifo_finish_packet(void);

static enum vert_type classify_vert(void);

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

    if (!(ta_fifo_byte_count % 32)) {
        on_packet_received();
    }
}

static void on_packet_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch(cmd_tp) {
    case TA_CMD_TYPE_VERTEX:
        if (poly_state.global_param == GLOBAL_PARAM_POLY) {
            on_vertex_received();
        } else if (poly_state.global_param == GLOBAL_PARAM_SPRITE) {
            on_sprite_received();
        } else {
            error_set_feature("some unknown vertex type");
            error_set_pvr2_global_param(poly_state.global_param);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case TA_CMD_TYPE_POLY_HDR:
    case TA_CMD_TYPE_SPRITE_HDR:
        on_polyhdr_received();
        break;
    case TA_CMD_TYPE_END_OF_LIST:
        on_end_of_list_received();
        break;
    case TA_CMD_TYPE_USER_CLIP:
        on_user_clip_received();
        break;
    case TA_CMD_TYPE_INPUT_LIST:
        // I only semi-understand what this is
        fprintf(stderr, "WARNING: TA_CMD_TYPE_INPUT_LIST received on pvr2 ta "
                "fifo!\n");
        ta_fifo_finish_packet();
        break;
    case TA_CMD_TYPE_UNKNOWN:
        fprintf(stderr, "WARNING: TA_CMD_TYPE_UNKNOWN received on pvr2 ta "
                "fifo!\n");
        ta_fifo_finish_packet();
        break;
    default:
        printf("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
        error_set_feature("PVR2 command type");
        error_set_ta_fifo_cmd(cmd_tp);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void decode_poly_hdr(struct poly_hdr *hdr) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;

    hdr->list =
        (enum display_list_type)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                                 TA_CMD_DISP_LIST_SHIFT);
    hdr->tex_enable = (bool)(ta_fifo32[0] & TA_CMD_TEX_ENABLE_MASK);
    hdr->ta_color_fmt = (ta_fifo32[0] & TA_COLOR_FMT_MASK) >>
        TA_COLOR_FMT_SHIFT;
    if (hdr->tex_enable) {
        hdr->tex_fmt = (ta_fifo32[3] & TEX_CTRL_PIX_FMT_MASK) >>
            TEX_CTRL_PIX_FMT_SHIFT;
        hdr->tex_width_shift = 3 +
            ((ta_fifo32[2] & TSP_TEX_WIDTH_MASK) >> TSP_TEX_WIDTH_SHIFT);
        hdr->tex_height_shift = 3 +
            ((ta_fifo32[2] & TSP_TEX_HEIGHT_MASK) >> TSP_TEX_HEIGHT_SHIFT);
        hdr->tex_inst = (ta_fifo32[2] & TSP_TEX_INST_MASK) >>
            TSP_TEX_INST_SHIFT;
        hdr->tex_twiddle = !(bool)(TEX_CTRL_NOT_TWIDDLED_MASK & ta_fifo32[3]);
        hdr->tex_vq_compression = (bool)(TEX_CTRL_VQ_MASK & ta_fifo32[3]);
        hdr->tex_addr = ((ta_fifo32[3] & TEX_CTRL_TEX_ADDR_MASK) >>
                         TEX_CTRL_TEX_ADDR_SHIFT) << 3;
        hdr->tex_filter = (ta_fifo32[2] & TSP_TEX_INST_FILTER_MASK) >>
            TSP_TEX_INST_FILTER_SHIFT;
    }

    hdr->src_blend_factor =
        (ta_fifo32[2] & TSP_WORD_SRC_ALPHA_FACTOR_MASK) >>
        TSP_WORD_SRC_ALPHA_FACTOR_SHIFT;
    hdr->dst_blend_factor =
        (ta_fifo32[2] & TSP_WORD_DST_ALPHA_FACTOR_MASK) >>
        TSP_WORD_DST_ALPHA_FACTOR_SHIFT;

    hdr->enable_depth_writes =
        !((ta_fifo32[0] & DEPTH_WRITE_DISABLE_MASK) >>
          DEPTH_WRITE_DISABLE_SHIFT);
    hdr->depth_func =
        (ta_fifo32[0] & DEPTH_FUNC_MASK) >> DEPTH_FUNC_SHIFT;

    hdr->shadow = (bool)(ta_fifo32[0] & TA_CMD_SHADOW_MASK);
    hdr->two_volumes_mode = (bool)(ta_fifo32[0] & TA_CMD_TWO_VOLUMES_MASK);
    hdr->color_type =
        (enum ta_color_type)((ta_fifo32[0] & TA_CMD_COLOR_TYPE_MASK) >>
                             TA_CMD_COLOR_TYPE_SHIFT);
    hdr->offset_color_enable = (bool)(ta_fifo32[0] & TA_CMD_OFFSET_COLOR_MASK);
    hdr->gourad_shading_enable =
        (bool)(ta_fifo32[0] & TA_CMD_GOURAD_SHADING_MASK);
    hdr->tex_coord_16_bit_enable =
        (bool)(ta_fifo32[0] & TA_CMD_16_BIT_TEX_COORD_MASK);

    if (hdr->color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        if (hdr->offset_color_enable) {
            memcpy(hdr->poly_color_rgba, ta_fifo32 + 9, 3 * sizeof(float));
            memcpy(hdr->poly_color_rgba + 3, ta_fifo32 + 8, sizeof(float));
        } else {
            memcpy(hdr->poly_color_rgba, ta_fifo32 + 5, 3 * sizeof(float));
            memcpy(hdr->poly_color_rgba + 3, ta_fifo32 + 4, sizeof(float));
        }
    }
}

static void on_polyhdr_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    enum display_list_type list =
        (enum display_list_type)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                                 TA_CMD_DISP_LIST_SHIFT);
    struct poly_hdr hdr;

    decode_poly_hdr(&hdr);

    /*
     * XXX It seems that intensity mode 1 is 64 bits, but mode 2 is only 32.
     * This is most likely because the point of intensity mode 2 is to reuse
     * the face color from the previous intensity mode 1 polygon.  I'm not 100%
     * clear on what the format of an intensity mode 2 header is, and I'm also
     * not 100% clear on whether or not it has its own offset header.  That
     * said, I am confident that intensity mode 2 is 32 bits.
     */
    if (hdr.color_type != TA_COLOR_TYPE_INTENSITY_MODE_2 &&
        hdr.offset_color_enable && ta_fifo_byte_count != 64) {
        // need 64 bytes not, 32.
        return;
    }

    if ((poly_state.current_list == DISPLAY_LIST_NONE) &&
        list_submitted[list]) {
        printf("WARNING: unable to open list %s because it is already "
               "closed\n", display_list_names[list]);
        goto the_end;
    }

    if ((poly_state.current_list != DISPLAY_LIST_NONE) &&
        (poly_state.current_list != list)) {
        printf("WARNING: attempting to input poly header for list %s without "
               "first closing %s\n", display_list_names[list],
               display_list_names[poly_state.current_list]);
        goto the_end;
    }

    /*
     * next_poly_group will finish the current poly_group (if there is one),
     * and that will reference the poly_state.  Ergo, next_poly_group must be
     * called BEFORE any poly_state changes are made.
     */
    struct geo_buf *geo = geo_buf_get_prod();

    if (poly_state.current_list != DISPLAY_LIST_NONE &&
        poly_state.current_list != list) {
        // finish the last poly group of the current list

#ifdef INVARIANTS
        if (poly_state.current_list < 0 || poly_state.current_list >= DISPLAY_LIST_COUNT) {
            fprintf(stderr, "ERROR: poly_state.current_list is 0x%08x\n",
                   (unsigned)poly_state.current_list);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
#endif

        if (geo->lists[poly_state.current_list].n_groups >= 1)
            finish_poly_group(geo, poly_state.current_list);
    }

    if ((poly_state.current_list != list) &&
        !list_submitted[list]) {

        printf("Opening display list %s\n", display_list_names[list]);
        poly_state.current_list = list;
        list_submitted[list] = true;
    }

    next_poly_group(geo, poly_state.current_list);

    // reset triangle strips
    poly_state.strip_len = 0;

    poly_state.ta_color_fmt = hdr.ta_color_fmt;

    if (hdr.tex_enable) {
        poly_state.tex_enable = true;
        printf("texture enabled\n");

        printf("the texture format is %d\n", hdr.tex_fmt);
        printf("The texture address ix 0x%08x\n", hdr.tex_addr);

        if (hdr.tex_twiddle)
            printf("not twiddled\n");
        else
            printf("twiddled\n");

        struct pvr2_tex *ent =
            pvr2_tex_cache_find(hdr.tex_addr,
                                hdr.tex_width_shift,
                                hdr.tex_height_shift,
                                hdr.tex_fmt, hdr.tex_twiddle,
                                hdr.tex_vq_compression);

        printf("texture dimensions are (%u, %u)\n",
               1 << hdr.tex_width_shift,
               1 << hdr.tex_height_shift);
        if (ent) {
            printf("Texture 0x%08x found in cache\n",
                   hdr.tex_addr);
        } else {
            printf("Adding 0x%08x to texture cache...\n",
                   hdr.tex_addr);
            ent = pvr2_tex_cache_add(hdr.tex_addr,
                                     hdr.tex_width_shift,
                                     hdr.tex_height_shift,
                                     hdr.tex_fmt,
                                     hdr.tex_twiddle,
                                     hdr.tex_vq_compression);
        }

        if (!ent) {
            fprintf(stderr, "WARNING: failed to add texture 0x%08x to "
                    "the texture cache\n", hdr.tex_addr);
            poly_state.tex_enable = false;
        } else {
            poly_state.tex_idx = pvr2_tex_cache_get_idx(ent);
        }
    } else {
        printf("textures are NOT enabled\n");
        poly_state.tex_enable = false;
    }
    poly_state.src_blend_factor = hdr.src_blend_factor;
    poly_state.dst_blend_factor = hdr.dst_blend_factor;

    poly_state.enable_depth_writes = hdr.enable_depth_writes;
    poly_state.depth_func = hdr.depth_func;

    poly_state.shadow = hdr.shadow;
    poly_state.two_volumes_mode = hdr.two_volumes_mode;
    poly_state.color_type = hdr.color_type;
    poly_state.offset_color_enable = hdr.offset_color_enable;
    poly_state.gourad_shading_enable = hdr.gourad_shading_enable;
    poly_state.tex_coord_16_bit_enable = hdr.tex_coord_16_bit_enable;

    poly_state.vert_type = classify_vert();
    poly_state.vert_len = vert_lengths[poly_state.vert_type];

    poly_state.tex_inst = hdr.tex_inst;
    poly_state.tex_filter = hdr.tex_filter;

    if (hdr.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        memcpy(poly_state.poly_color_rgba, hdr.poly_color_rgba,
               sizeof(poly_state.poly_color_rgba));
    }

    poly_state.global_param =
        (enum global_param)((ta_fifo32[0] & TA_CMD_TYPE_MASK) >>
                            TA_CMD_TYPE_SHIFT);

    printf("POLY HEADER PACKET!\n");

the_end:
    ta_fifo_finish_packet();
}

// unpack a sprite's texture coordinates into two floats
static void unpack_uv16(float *u_coord, float *v_coord, void const *input) {
    uint32_t val = *(uint32_t*)input;
    uint32_t u_val = val & 0xffff0000;
    uint32_t v_val = val << 16;

    memcpy(u_coord, &u_val, sizeof(*u_coord));
    memcpy(v_coord, &v_val, sizeof(*v_coord));
}

static void on_sprite_received(void) {
    struct geo_buf *geo = geo_buf_get_prod();

    /*
     * if the vertex is not long enough, return and make input_poly_fifo call
     * us again later when there is more data.  Practically, this means that we
     * are expecting 64 bytes, but we only have 32 bytes so far.
     */
    if (ta_fifo_byte_count != 64)
        return;

    if (poly_state.current_list < 0) {
        printf("ERROR: unable to render sprite because no display lists are "
               "open\n");
        ta_fifo_finish_packet();
        return;
    }

    struct display_list *list = geo->lists + poly_state.current_list;

    if (list->n_groups <= 0) {
        printf("ERROR: unable to render sprite because I'm still waiting to "
               "see a polygon header\n");
        ta_fifo_finish_packet();
        return;
    }

    struct poly_group *group = list->groups + (list->n_groups - 1);

    if (group->n_verts + 6 >= GEO_BUF_VERT_COUNT) {
        fprintf(stderr, "ERROR (while rendering a sprite): PVR2's "
                "GEO_BUF_VERT_COUNT has been reached!\n");
        return;
    }

    float const *ta_fifo_float = (float const*)ta_fifo;

    /*
     * four quadrilateral vertices.  the z-coordinate of p4 is determined
     * automatically by the PVR2 so it is not possible to specify a non-coplanar
     * set of vertices.
     */
    float p1[3] = { ta_fifo_float[1], ta_fifo_float[2], 1.0 / ta_fifo_float[3] };
    float p2[3] = { ta_fifo_float[4], ta_fifo_float[5], 1.0 / ta_fifo_float[6] };
    float p3[3] = { ta_fifo_float[7], ta_fifo_float[8], 1.0 / ta_fifo_float[9] };
    float p4[3] = { ta_fifo_float[10], ta_fifo_float[11] };

    /*
     * unpack the texture coordinates.  The third vertex's coordinate is the
     * scond vertex's coordinate plus the two side-vectors.  We do this
     * unconditionally even if textures are disabled.  If textures are disabled
     * then the output of this texture-coordinate algorithm is undefined but it
     * does not matter because the rendering code won't be using it anyways.
     */
    float uv[4][2];

    unpack_uv16(uv[0], uv[0] + 1, ta_fifo_float + 13);
    unpack_uv16(uv[1], uv[1] + 1, ta_fifo_float + 14);
    unpack_uv16(uv[2], uv[2] + 1, ta_fifo_float + 15);

    float uv_vec[2][2] = {
        { uv[0][0] - uv[1][0], uv[0][1] - uv[1][1] },
        { uv[2][0] - uv[1][0], uv[2][1] - uv[1][1] }
    };

    uv[3][0] = uv[1][0] + uv_vec[0][0] + uv_vec[1][0];
    uv[3][1] = uv[1][1] + uv_vec[0][1] + uv_vec[1][1];

    /*
     * any three non-colinear points will define a 2-dimensional hyperplane in
     * 3-dimensionl space.  The hyperplane consists of all points where the
     * following relationship is true:
     *
     * dot(n, p) + d == 0
     *
     * where n is a vector orthogonal to the hyperplane, d is the translation
     * from the origin to the hyperplane along n, and p is any point on the
     * plane.
     *
     * n is usually a normalized vector, but for our purposes that is not
     * necessary because d will scale accordingly.
     *
     * If the magnitude of n is zero, then all three points are colinear (or
     * coincidental) and they do not define a single hyperplane because there
     * are inifinite hyperplanes which contain all three points.  In this case
     * the quadrilateral is considered degenerate and should not be rendered.
     *
     * Because the three existing vertices are coplanar, the fourth vertex's
     * z-coordinate can be determined based on the hyperplane defined by the
     * other three points.
     *
     * dot(n, p) + d == 0
     * n.x * p.x + n.y * p.y + n.z * p.z + d == 0
     * n.z * p.z = -(d + n.x * p.x + n.y * p.y)
     * p.z = -(d + n.x * p.x + n.y * p.y) / n.z
     *
     * In the case where n.z is 0, the hyperplane is oriented orthogonally with
     * respect to the observer.  The only dimension on which the quadrilateral
     * is visible is the one which is infinitely thin, so it should not be
     * rendered.
     */

    // side-vectors
    float v1[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
    float v2[3] = { p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2] };

    // hyperplane normal
    float norm[3] = {
        v1[1] * v2[2] - v1[2] * v2[1],
        v1[2] * v2[0] - v1[0] * v2[2],
        v1[0] * v2[1] - v1[1] * v2[0]
    };

    /*
     * return early if the quad is degenerate or it is oriented orthoganally to
     * the viewer.
     *
     * TODO: consider using a floating-point tolerance instead of comparing to
     * zero directly.
     */
    if ((norm[2] == 0.0f) ||
        (norm[0] * norm[0] + norm[1] * norm[1] + norm[2] * norm[2] == 0.0f))
        return;

    // hyperplane translation
    float dist = -norm[0] * p1[0] - norm[1] * p1[1] - norm[2] * p1[2];

    p4[2] = -1.0f * (dist + norm[0] * p4[0] + norm[1] * p4[1]) / norm[2];

    float *vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p1[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p1[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p1[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[0][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[0][1];
    group->n_verts++;

    vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p2[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p2[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p2[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[1][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[1][1];
    group->n_verts++;

    vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p3[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p3[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p3[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[2][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[2][1];
    group->n_verts++;

    vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p1[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p1[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p1[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[0][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[0][1];
    group->n_verts++;

    vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p3[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p3[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p3[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[2][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[2][1];
    group->n_verts++;

    vert_ptr = group->verts + GEO_BUF_VERT_LEN * group->n_verts;
    memset(vert_ptr, 0, GEO_BUF_VERT_LEN * sizeof(float));
    vert_ptr[GEO_BUF_POS_OFFSET + 0] = p4[0];
    vert_ptr[GEO_BUF_POS_OFFSET + 1] = p4[1];
    vert_ptr[GEO_BUF_POS_OFFSET + 2] = p4[2];
    vert_ptr[GEO_BUF_COLOR_OFFSET + 0] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 1] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 2] = 1.0f;
    vert_ptr[GEO_BUF_COLOR_OFFSET + 3] = 1.0f;
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 0] = uv[3][0];
    vert_ptr[GEO_BUF_TEX_COORD_OFFSET + 1] = uv[3][1];
    group->n_verts++;

    if (p1[2] < geo->clip_min)
        geo->clip_min = p1[2];
    if (p1[2] > geo->clip_max)
        geo->clip_max = p1[2];

    if (p2[2] < geo->clip_min)
        geo->clip_min = p2[2];
    if (p2[2] > geo->clip_max)
        geo->clip_max = p2[2];

    if (p3[2] < geo->clip_min)
        geo->clip_min = p3[2];
    if (p3[2] > geo->clip_max)
        geo->clip_max = p3[2];

    if (p4[2] < geo->clip_min)
        geo->clip_min = p4[2];
    if (p4[2] > geo->clip_max)
        geo->clip_max = p4[2];

    ta_fifo_finish_packet();
}

static void on_vertex_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    float const *ta_fifo_float = (float const*)ta_fifo;

    /*
     * if the vertex is not long enough, return and make input_poly_fifo call
     * us again later when there is more data.  Practically, this means that we
     * are expecting 64 bytes, but we only have 32 bytes so far.
     */
    if (ta_fifo_byte_count != (poly_state.vert_len * 4))
        return;

#ifdef PVR2_LOG_VERBOSE
    printf("vertex received!\n");
#endif
    struct geo_buf *geo = geo_buf_get_prod();

    if (poly_state.current_list < 0) {
        printf("ERROR: unable to render vertex because no display lists are "
               "open\n");
        ta_fifo_finish_packet();
        return;
    }

    struct display_list *list = geo->lists + poly_state.current_list;

    if (list->n_groups <= 0) {
        printf("ERROR: unable to render vertex because I'm still waiting to "
               "see a polygon header\n");
        ta_fifo_finish_packet();
        return;
    }

    struct poly_group *group = list->groups + (list->n_groups - 1);

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
        if (group->n_verts < GEO_BUF_VERT_COUNT) {
            memcpy(group->verts + GEO_BUF_VERT_LEN * group->n_verts,
                   poly_state.strip_vert1, sizeof(poly_state.strip_vert1));
            group->n_verts++;
        }

        if (group->n_verts < GEO_BUF_VERT_COUNT) {
            memcpy(group->verts + GEO_BUF_VERT_LEN * group->n_verts,
                   poly_state.strip_vert2, sizeof(poly_state.strip_vert2));
            group->n_verts++;
        }
    }

    if (group->n_verts < GEO_BUF_VERT_COUNT) {
        // first update the clipping planes in the geo_buf
        /*
         * TODO: there are FPU instructions on x86 that can do this without
         * branching
         */
        float z_recip = 1.0 / ta_fifo_float[3];
        if (z_recip < geo->clip_min)
            geo->clip_min = z_recip;
        if (z_recip > geo->clip_max)
            geo->clip_max = z_recip;

        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_POS_OFFSET + 0] =
            ta_fifo_float[1];
        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_POS_OFFSET + 1] =
            ta_fifo_float[2];
        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_POS_OFFSET + 2] =
            z_recip;

        if (poly_state.tex_enable) {
            unsigned dst_uv_offset =
                GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_TEX_COORD_OFFSET;
            group->verts[dst_uv_offset + 0] = ta_fifo_float[4];
            group->verts[dst_uv_offset + 1] = ta_fifo_float[5];
        }

        float color_r, color_g, color_b, color_a;
        float intensity;

        switch (poly_state.ta_color_fmt) {
        case TA_COLOR_TYPE_PACKED:
            color_a = (float)((ta_fifo32[6] & 0xff000000) >> 24) / 255.0f;
            color_r = (float)((ta_fifo32[6] & 0x00ff0000) >> 16) / 255.0f;
            color_g = (float)((ta_fifo32[6] & 0x0000ff00) >> 8) / 255.0f;
            color_b = (float)((ta_fifo32[6] & 0x000000ff) >> 0) / 255.0f;
            break;
        case TA_COLOR_TYPE_FLOAT:
            memcpy(&color_a, ta_fifo32 + 4, sizeof(color_a));
            memcpy(&color_r, ta_fifo32 + 5, sizeof(color_r));
            memcpy(&color_g, ta_fifo32 + 6, sizeof(color_g));
            memcpy(&color_b, ta_fifo32 + 7, sizeof(color_b));
            break;
        case TA_COLOR_TYPE_INTENSITY_MODE_1:
        case TA_COLOR_TYPE_INTENSITY_MODE_2:
            color_a = poly_state.poly_color_rgba[3];

            memcpy(&intensity, ta_fifo32 + 6, sizeof(float));
            color_r = intensity * poly_state.poly_color_rgba[0];
            color_g = intensity * poly_state.poly_color_rgba[1];
            color_b = intensity * poly_state.poly_color_rgba[2];
            break;
        default:
            color_r = color_g = color_b = color_a = 1.0f;
            fprintf(stderr, "WARNING: unknown TA color format %u\n", poly_state.ta_color_fmt);
        }

        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_COLOR_OFFSET + 0] =
            color_r;
        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_COLOR_OFFSET + 1] =
            color_g;
        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_COLOR_OFFSET + 2] =
            color_b;
        group->verts[GEO_BUF_VERT_LEN * group->n_verts + GEO_BUF_COLOR_OFFSET + 3] =
            color_a;

        if (ta_fifo32[0] & TA_CMD_END_OF_STRIP_MASK) {
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
                   group->verts + GEO_BUF_VERT_LEN * group->n_verts,
                   sizeof(poly_state.strip_vert2));
            poly_state.strip_len++;
        }

        group->n_verts++;
    } else {
        fprintf(stderr, "WARNING: dropped vertices: geo_buf contains %u "
                "verts\n", group->n_verts);
#ifdef INVARIANTS
        abort();
#endif
    }

    ta_fifo_finish_packet();
}

static void on_end_of_list_received(void) {
    printf("END-OF-LIST PACKET!\n");

    finish_poly_group(geo_buf_get_prod(), poly_state.current_list);

    if (poly_state.current_list != DISPLAY_LIST_NONE) {
        printf("Display list \"%s\" closed\n",
               display_list_names[poly_state.current_list]);
    } else {
        printf("Unable to close the current display list because no display "
               "list has been opened\n");
        goto the_end;
    }

    // TODO: In a real dreamcast this probably would not happen instantly
    switch (poly_state.current_list) {
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
        /*
         * this can never actually happen because this
         * functionshould have returned early above
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    poly_state.current_list = DISPLAY_LIST_NONE;

the_end:
    ta_fifo_finish_packet();
}

static void on_user_clip_received(void) {
    printf("PVR2 WARNING: UNIMPLEMENTED USER TILE CLIP PACKET RECEIVED!\n");

    // TODO: implement tile clipping

    ta_fifo_finish_packet();
}

void pvr2_ta_startrender(void) {
    printf("STARTRENDER requested!\n");

    struct geo_buf *geo = geo_buf_get_prod();

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
    geo->bgcolor[0] = bg_color_r;
    geo->bgcolor[1] = bg_color_g;
    geo->bgcolor[2] = bg_color_b;
    geo->bgcolor[3] = bg_color_a;

    uint32_t backgnd_depth_as_int = get_isp_backgnd_d();
    memcpy(&geo->bgdepth, &backgnd_depth_as_int, sizeof(float));

    geo->screen_width = width;
    geo->screen_height = height;

    // set the blend enable flag for translucent-only
    geo->lists[DISPLAY_LIST_OPAQUE].blend_enable = false;
    geo->lists[DISPLAY_LIST_OPAQUE_MOD].blend_enable = false;
    geo->lists[DISPLAY_LIST_TRANS].blend_enable = true;
    geo->lists[DISPLAY_LIST_TRANS_MOD].blend_enable = false;
    geo->lists[DISPLAY_LIST_PUNCH_THROUGH].blend_enable = false;

    pvr2_tex_cache_xmit(geo);

    finish_poly_group(geo, poly_state.current_list);

    framebuffer_set_current_host(geo->frame_stamp);
    geo_buf_produce();
    gfx_thread_render_geo_buf();

    memset(list_submitted, 0, sizeof(list_submitted));
    poly_state.current_list = DISPLAY_LIST_NONE;

    // TODO: This irq definitely should not be triggered immediately
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}

void pvr2_ta_reinit(void) {
    memset(list_submitted, 0, sizeof(list_submitted));
}

static void finish_poly_group(struct geo_buf *geo,
                              enum display_list_type disp_list) {
    if (disp_list < 0) {
        printf("%s - no lists are open\n", __func__);
        return;
    }

    struct display_list *list = geo->lists + disp_list;

    if (list->n_groups <= 0) {
        printf("%s - still waiting for a polygon header to be opened!\n",
               __func__);
        return;
    }

    struct poly_group *group = list->groups + (list->n_groups - 1);

    if (poly_state.tex_enable) {
        printf("tex_enable should be true\n");
        group->tex_enable = true;
        group->tex_idx = poly_state.tex_idx;
    } else {
        printf("tex_enable should be false\n");
        group->tex_enable = false;
    }

    group->src_blend_factor = poly_state.src_blend_factor;
    group->dst_blend_factor = poly_state.dst_blend_factor;

    group->enable_depth_writes = poly_state.enable_depth_writes;
    group->depth_func = poly_state.depth_func;

    group->tex_inst = poly_state.tex_inst;
    group->tex_filter = poly_state.tex_filter;

    /*
     * this check is a little silly, but I get segfaults sometimes when
     * indexing into src_blend_factors and dst_blend_factors and I don't know
     * why.
     *
     * TODO: this was (hopefully) fixed in commit
     * 92059fe4f1714b914cec75fd2f91e676127d3097 but I am keeping the INVARIANTS
     * test here just in case.  It should be safe to delete after a couple of
     * months have gone by without this INVARIANTS test ever failing.
     */
    if ((group->src_blend_factor < 0) ||
        (group->dst_blend_factor < 0) ||
        (group->src_blend_factor >= PVR2_BLEND_FACTOR_COUNT) ||
        (group->dst_blend_factor >= PVR2_BLEND_FACTOR_COUNT)) {
        error_set_src_blend_factor(group->src_blend_factor);
        error_set_dst_blend_factor(group->dst_blend_factor);
        error_set_display_list_index((unsigned)disp_list);
        error_set_geo_buf_group_index(group - list->groups);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static void next_poly_group(struct geo_buf *geo,
                            enum display_list_type disp_list) {
    struct display_list *list = geo->lists + disp_list;

    if (disp_list < 0) {
        printf("%s - no lists are open\n", __func__);
        return;
    }

    if (list->n_groups >= 1) {
        finish_poly_group(geo, disp_list);
        list->groups = (struct poly_group*)realloc(list->groups,
                                                  sizeof(struct poly_group) *
                                                  ++list->n_groups);
    } else {
        list->n_groups = 1;
        list->groups = (struct poly_group*)malloc(sizeof(struct poly_group));
    }

    struct poly_group *new_group = list->groups + (list->n_groups - 1);
    new_group->n_verts = 0;
    new_group->tex_enable = false;
}

static enum vert_type classify_vert(void) {
    if (poly_state.tex_enable) {
        if (poly_state.two_volumes_mode) {
            if (poly_state.tex_coord_16_bit_enable) {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD;
            } else {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_TWO_VOLUMES;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_TWO_VOLUMES;
            }
        } else {
            if (poly_state.tex_coord_16_bit_enable) {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD;
                if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                    return VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_16_BIT_TEX_COORD;
            } else {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR;
                if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                    return VERT_TEX_FLOATING_COLOR;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY;
            }
        }
    } else {
        if (poly_state.two_volumes_mode) {
            if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                return VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES;
            if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                return VERT_NO_TEX_INTENSITY_TWO_VOLUMES;
        } else {
            if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                return VERT_NO_TEX_PACKED_COLOR;
            if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                return VERT_NO_TEX_FLOAT_COLOR;
            if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                return VERT_NO_TEX_INTENSITY;
        }
    }

    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void ta_fifo_finish_packet(void) {
    ta_fifo_byte_count = 0;
}
