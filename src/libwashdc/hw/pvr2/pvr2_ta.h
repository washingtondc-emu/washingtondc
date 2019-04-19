/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef PVR2_TA_H_
#define PVR2_TA_H_

#include <stddef.h>
#include <stdint.h>

#include "dc_sched.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"
#include "gfx/gfx.h"
#include "gfx/gfx_il.h"

struct pvr2;

// texture control word
#define TEX_CTRL_MIP_MAPPED_SHIFT 31
#define TEX_CTRL_MIP_MAPPED_MASK (1 << TEX_CTRL_MIP_MAPPED_SHIFT)

#define TEX_CTRL_VQ_SHIFT 30
#define TEX_CTRL_VQ_MASK (1 << TEX_CTRL_VQ_SHIFT)

#define TEX_CTRL_PIX_FMT_SHIFT 27
#define TEX_CTRL_PIX_FMT_MASK (7 << TEX_CTRL_PIX_FMT_SHIFT)

#define TEX_CTRL_NOT_TWIDDLED_SHIFT 26
#define TEX_CTRL_NOT_TWIDDLED_MASK (1 << TEX_CTRL_NOT_TWIDDLED_SHIFT)

#define TEX_CTRL_STRIDE_SEL_SHIFT 25
#define TEX_CTRL_STRIDE_SEL_MASK (1 << TEX_CTRL_STRIDE_SEL_SHIFT)

// this needs to be left-shifted by 3 to get the actual address
#define TEX_CTRL_TEX_ADDR_SHIFT 0
#define TEX_CTRL_TEX_ADDR_MASK (0x1fffff << TEX_CTRL_TEX_ADDR_SHIFT)

#define TEX_CTRL_PALETTE_START_SHIFT 21
#define TEX_CTRL_PALETTE_START_MASK (0x3f << TEX_CTRL_PALETTE_START_SHIFT)

#define TSP_TEX_FLIP_SHIFT 17
#define TSP_TEX_FLIP_MASK (3 << TSP_TEX_FLIP_SHIFT)

#define TSP_TEX_CLAMP_SHIFT 15
#define TSP_TEX_CLAMP_MASK (3 << TSP_TEX_CLAMP_SHIFT)

#define TSP_TEX_INST_FILTER_SHIFT 13
#define TSP_TEX_INST_FILTER_MASK (3 << TSP_TEX_INST_FILTER_SHIFT)

#define TSP_TEX_INST_SHIFT 6
#define TSP_TEX_INST_MASK (3 << TSP_TEX_INST_SHIFT)

#define TSP_TEX_WIDTH_SHIFT 3
#define TSP_TEX_WIDTH_MASK (7 << TSP_TEX_WIDTH_SHIFT)

#define TSP_TEX_HEIGHT_SHIFT 0
#define TSP_TEX_HEIGHT_MASK (7 << TSP_TEX_HEIGHT_SHIFT)

/*
 * pixel formats for the texture control word.
 *
 * PAL here means "palette", not the European video standard.
 *
 * Also TEX_CTRL_PIX_FMT_INVALID is treated as TEX_CTRL_PIX_FMT_ARGB_1555 even
 * though it's still invalid.
 */
enum TexCtrlPixFmt {
    TEX_CTRL_PIX_FMT_ARGB_1555,
    TEX_CTRL_PIX_FMT_RGB_565,
    TEX_CTRL_PIX_FMT_ARGB_4444,
    TEX_CTRL_PIX_FMT_YUV_422,
    TEX_CTRL_PIX_FMT_BUMP_MAP,
    TEX_CTRL_PIX_FMT_4_BPP_PAL,
    TEX_CTRL_PIX_FMT_8_BPP_PAL,
    TEX_CTRL_PIX_FMT_INVALID,

    TEX_CTRL_PIX_FMT_COUNT // obviously this is not a real pixel format
};

#define PVR2_TEX_MAX_W 1024
#define PVR2_TEX_MAX_H 1024
#define PVR2_TEX_MAX_BYTES (PVR2_TEX_MAX_W * PVR2_TEX_MAX_H * 4)

float pvr2_ta_fifo_poly_read_float(addr32_t addr, void *ctxt);
void pvr2_ta_fifo_poly_write_float(addr32_t addr, float val, void *ctxt);
double pvr2_ta_fifo_poly_read_double(addr32_t addr, void *ctxt);
void pvr2_ta_fifo_poly_write_double(addr32_t addr, double val, void *ctxt);
uint32_t pvr2_ta_fifo_poly_read_32(addr32_t addr, void *ctxt);
void pvr2_ta_fifo_poly_write_32(addr32_t addr, uint32_t val, void *ctxt);
uint16_t pvr2_ta_fifo_poly_read_16(addr32_t addr, void *ctxt);
void pvr2_ta_fifo_poly_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint8_t pvr2_ta_fifo_poly_read_8(addr32_t addr, void *ctxt);
void pvr2_ta_fifo_poly_write_8(addr32_t addr, uint8_t val, void *ctxt);

extern struct memory_interface pvr2_ta_fifo_intf;

void pvr2_ta_startrender(struct pvr2 *pvr2);

/*
 * This gets called when the TA gets reset by a register write.  It is not
 * related to pvr2_ta_init/pvr2_ta_cleanup.
 */
void pvr2_ta_reinit(struct pvr2 *pvr2);

void pvr2_ta_init(struct pvr2 *pvr2);
void pvr2_ta_cleanup(struct pvr2 *pvr2);

unsigned get_cur_frame_stamp(struct pvr2 *pvr2);

/*
 * There are five display lists:
 *
 * Opaque
 * Punch-through polygon
 * Opaque/punch-through modifier volume
 * Translucent
 * Translucent modifier volume
 *
 * They are rendered by the opengl backend in that order.
 */
enum display_list_type {
    DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE = DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,
    DISPLAY_LIST_LAST = DISPLAY_LIST_PUNCH_THROUGH,

    // These three list types are invalid, but I do see DISPLAY_LIST_7 sometimes
    DISPLAY_LIST_5,
    DISPLAY_LIST_6,
    DISPLAY_LIST_7,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

enum ta_color_type {
    TA_COLOR_TYPE_PACKED,
    TA_COLOR_TYPE_FLOAT,
    TA_COLOR_TYPE_INTENSITY_MODE_1,
    TA_COLOR_TYPE_INTENSITY_MODE_2
};

enum pvr2_pkt_tp {
    PVR2_PKT_HDR,
    PVR2_PKT_VTX,
    PVR2_PKT_END_OF_LIST,
    PVR2_PKT_INPUT_LIST,
    PVR2_PKT_USER_CLIP
};

struct pvr2_pkt_vtx {
    float base_color[4];
    float offs_color[4];
    float uv[2];
    float pos[3];

    bool end_of_strip;
};

enum pvr2_hdr_tp {
    PVR2_HDR_TRIANGLE_STRIP,
    PVR2_HDR_QUAD
};

struct pvr2_pkt_hdr {
    enum pvr2_hdr_tp tp;

    unsigned vtx_len;

    enum display_list_type list;

    bool tex_enable;
    uint32_t tex_addr;

    /*
     * this is the upper 2-bits (for 8BPP) or 6 bits (for 4BPP) of every
     * palette address referenced by this texture.  It needs to be shifted left
     * by 2 or 6 bits and ORed with pixel values to get palette addresses.
     *
     * this field only holds meaning if tex_fmt is TEX_CTRL_PIX_FMT_4_BPP_PAL
     * or TEX_CTRL_PIX_FMT_8_BPP_PAL; otherwise it is meaningless.
     */
    unsigned tex_palette_start;

    unsigned tex_width_shift, tex_height_shift;
    bool tex_twiddle;
    bool stride_sel;
    bool tex_vq_compression;
    bool tex_mipmap;
    enum TexCtrlPixFmt pix_fmt;
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;
    enum tex_wrap_mode tex_wrap_mode[2];

    enum ta_color_type ta_color_fmt;
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    bool shadow;
    bool two_volumes_mode;
    /* enum ta_color_type color_type; */
    bool offset_color_enable;
    bool gourad_shading_enable;
    bool tex_coord_16_bit_enable;

    float poly_base_color_rgba[4];
    float poly_offs_color_rgba[4];

    float sprite_base_color_rgba[4];
    float sprite_offs_color_rgba[4];
};

struct pvr2_pkt_user_clip {
    /*
     * these are in terms of tiles, so the actual coordinates are these
     * multiplied by 32.
     */
    unsigned xmin, ymin, xmax, ymax;
};

union pvr2_pkt_inner {
    struct pvr2_pkt_vtx vtx;
    struct pvr2_pkt_hdr hdr;
    struct pvr2_pkt_user_clip user_clip;
};

struct pvr2_pkt {
    enum pvr2_pkt_tp tp;
    union pvr2_pkt_inner dat;
};

enum global_param {
    GLOBAL_PARAM_POLY = 4,
    GLOBAL_PARAM_SPRITE = 5
};

struct pvr2_ta_vert {
    float pos[3];
    float base_color[4];
    float offs_color[4];
    float tex_coord[2];
};

#define PVR2_CMD_MAX_LEN 64

struct gfx_il_inst_chain {
    struct gfx_il_inst cmd;
    struct gfx_il_inst_chain *next;
};

struct pvr2_ta {
    enum display_list_type cur_list;

    uint8_t ta_fifo[PVR2_CMD_MAX_LEN];
    unsigned ta_fifo_byte_count;

    bool list_submitted[DISPLAY_LIST_COUNT];

    struct pvr2_pkt_hdr hdr;

    /*
     * used to store the previous two verts when we're
     * rendering a triangle strip
     */
    struct pvr2_ta_vert strip_vert_1;
    struct pvr2_ta_vert strip_vert_2;
    unsigned strip_len; // number of verts in the current triangle strip

    float clip_min, clip_max;

    // index into the texture cache
    unsigned tex_idx;

    bool open_group;

    float *pvr2_ta_vert_buf;
    unsigned pvr2_ta_vert_buf_count;
    unsigned pvr2_ta_vert_cur_group;

    struct gfx_il_inst_chain *disp_list_begin[DISPLAY_LIST_COUNT];
    struct gfx_il_inst_chain *disp_list_end[DISPLAY_LIST_COUNT];

    struct gfx_il_inst_chain *gfx_il_inst_buf;
    unsigned gfx_il_inst_buf_count;

    // the 4-component color that gets sent to glClearColor
    float pvr2_bgcolor[4];

    unsigned next_frame_stamp;

    /*
     * the intensity mode base and offset colors.  These should be referenced
     * instead of the copies held in hdr because hdr's version of these gets
     * overwritten every time there's a new header, whereas these variables here
     * only get overwritten when there's a new INTENSITY_MODE_1 header packet.
     */
    float poly_base_color_rgba[4];
    float poly_offs_color_rgba[4];
    float sprite_base_color_rgba[4];
    float sprite_offs_color_rgba[4];

    struct SchedEvent pvr2_render_complete_int_event,
        pvr2_op_complete_int_event,
        pvr2_op_mod_complete_int_event,
        pvr2_trans_complete_int_event,
        pvr2_trans_mod_complete_int_event,
        pvr2_pt_complete_int_event;
    bool pvr2_render_complete_int_event_scheduled,
        pvr2_op_complete_int_event_scheduled,
        pvr2_op_mod_complete_int_event_scheduled,
        pvr2_trans_complete_int_event_scheduled,
        pvr2_trans_mod_complete_int_event_scheduled,
        pvr2_pt_complete_int_event_scheduled;
};

#endif
