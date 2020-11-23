/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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
#include <stdbool.h>

#include "dc_sched.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"
#include "gfx/gfx.h"
#include "washdc/gfx/gfx_il.h"

#include "pvr2_core.h"

struct pvr2;

// this needs to be left-shifted by 3 to get the actual address
#define TEX_CTRL_TEX_ADDR_SHIFT 0
#define TEX_CTRL_TEX_ADDR_MASK (0x1fffff << TEX_CTRL_TEX_ADDR_SHIFT)

#define TEX_CTRL_PALETTE_START_SHIFT 21
#define TEX_CTRL_PALETTE_START_MASK (0x3f << TEX_CTRL_PALETTE_START_SHIFT)

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

enum pvr2_pkt_tp {
    PVR2_PKT_HDR,
    PVR2_PKT_VTX,
    PVR2_PKT_END_OF_LIST,
    PVR2_PKT_INPUT_LIST,
    PVR2_PKT_USER_CLIP
};

struct pvr2_pkt_vtx {
    float pos[3];
    float base_color[4];
    float offs_color[4];
    float uv[2];

    bool end_of_strip;
};

struct pvr2_pkt_quad {
    /*
     * four vertices consisting of 3-component poistions
     *and 2-component texture coordinates
     */
    float vert_pos[4][3];
    float vert_recip_z[4];
    unsigned tex_coords_packed[3];
    bool degenerate;
};

struct pvr2_pkt_hdr {
    uint32_t param[4];

    enum pvr2_hdr_tp tp;

    unsigned vtx_len;

    float poly_base_color_rgba[4];
    float poly_offs_color_rgba[4];

    float sprite_base_color_rgba[4];
    float sprite_offs_color_rgba[4];
};

#define TA_CMD_TEX_ENABLE_SHIFT 3
#define TA_CMD_TEX_ENABLE_MASK (1 << TA_CMD_TEX_ENABLE_SHIFT)
static inline bool pvr2_hdr_tex_enable(struct pvr2_pkt_hdr const *hdr) {
    return hdr->param[0] & TA_CMD_TEX_ENABLE_MASK;
}

#define TA_CMD_COLOR_TYPE_SHIFT 4
#define TA_CMD_COLOR_TYPE_MASK (3 << TA_CMD_COLOR_TYPE_SHIFT)
static inline enum ta_color_type
pvr2_hdr_color_fmt(struct pvr2_pkt_hdr const *hdr) {
    return (enum ta_color_type)((hdr->param[0] & TA_CMD_COLOR_TYPE_MASK) >>
                                TA_CMD_COLOR_TYPE_SHIFT);
}

#define TA_CMD_OFFSET_COLOR_SHIFT 2
#define TA_CMD_OFFSET_COLOR_MASK (1 << TA_CMD_OFFSET_COLOR_SHIFT)
static inline bool
pvr2_hdr_offset_color_enable(struct pvr2_pkt_hdr const *hdr) {
    /*
     * When textures are disabled, offset colors are implicitly disabled even
     * if the offset_color_enable bit was set.
     */
    if (pvr2_hdr_tex_enable(hdr) &&
        pvr2_hdr_color_fmt(hdr) != TA_COLOR_TYPE_PACKED &&
        pvr2_hdr_color_fmt(hdr) != TA_COLOR_TYPE_FLOAT)
        return hdr->param[0] & TA_CMD_OFFSET_COLOR_MASK;
    else
        return false;
}

#define TA_CMD_POLY_TYPE_SHIFT 24
#define TA_CMD_POLY_TYPE_MASK (0x7 << TA_CMD_POLY_TYPE_SHIFT)
static inline enum pvr2_poly_type
pvr2_hdr_poly_type(struct pvr2_pkt_hdr const *hdr) {
    return (enum pvr2_poly_type)((hdr->param[0] & TA_CMD_POLY_TYPE_MASK) >>
                                 TA_CMD_POLY_TYPE_SHIFT);
}

#define TEX_CTRL_PIX_FMT_SHIFT 27
#define TEX_CTRL_PIX_FMT_MASK (7 << TEX_CTRL_PIX_FMT_SHIFT)
static inline enum TexCtrlPixFmt
pvr2_hdr_pix_fmt(struct pvr2_pkt_hdr const *hdr) {
    return (enum TexCtrlPixFmt)((hdr->param[3] & TEX_CTRL_PIX_FMT_MASK) >>
                                TEX_CTRL_PIX_FMT_SHIFT);
}

#define TA_CMD_GOURAD_SHADING_SHIFT 1
#define TA_CMD_GOURAD_SHADING_MASK (1 << TA_CMD_GOURAD_SHADING_SHIFT)
static inline bool
pvr2_hdr_gourad_shading(struct pvr2_pkt_hdr const *hdr) {
    return hdr->param[0] & TA_CMD_GOURAD_SHADING_MASK;
}

/*
 * this has something to do with swapping out the ISP parameters
 * when modifier volumes are in use, I think
 */
#define TA_CMD_SHADOW_SHIFT 7
#define TA_CMD_SHADOW_MASK (1 << TA_CMD_SHADOW_SHIFT)
static inline bool
pvr2_hdr_shadow(struct pvr2_pkt_hdr const *hdr) {
    return hdr->param[0] & TA_CMD_SHADOW_MASK;
}

#define TEX_CTRL_VQ_SHIFT 30
#define TEX_CTRL_VQ_MASK (1 << TEX_CTRL_VQ_SHIFT)
static inline bool
pvr2_hdr_vq_compression(struct pvr2_pkt_hdr const *hdr) {
    return TEX_CTRL_VQ_MASK & hdr->param[3];
}

#define TSP_TEX_WIDTH_SHIFT 3
#define TSP_TEX_WIDTH_MASK (7 << TSP_TEX_WIDTH_SHIFT)
static inline unsigned
pvr2_hdr_tex_width_shift(struct pvr2_pkt_hdr const *hdr) {
    return 3 + ((hdr->param[2] & TSP_TEX_WIDTH_MASK) >> TSP_TEX_WIDTH_SHIFT);
}

#define TSP_TEX_HEIGHT_SHIFT 0
#define TSP_TEX_HEIGHT_MASK (7 << TSP_TEX_HEIGHT_SHIFT)
static inline unsigned
pvr2_hdr_tex_height_shift(struct pvr2_pkt_hdr const *hdr) {
    return 3 + ((hdr->param[2] & TSP_TEX_HEIGHT_MASK) >> TSP_TEX_HEIGHT_SHIFT);
}

#define TSP_TEX_INST_SHIFT 6
#define TSP_TEX_INST_MASK (3 << TSP_TEX_INST_SHIFT)
static inline enum tex_inst
pvr2_hdr_tex_inst(struct pvr2_pkt_hdr const *hdr) {
    return (hdr->param[2] & TSP_TEX_INST_MASK) >>
        TSP_TEX_INST_SHIFT;
}

#define TSP_TEX_INST_FILTER_SHIFT 13
#define TSP_TEX_INST_FILTER_MASK (3 << TSP_TEX_INST_FILTER_SHIFT)
static inline enum tex_filter
pvr2_hdr_tex_filter(struct pvr2_pkt_hdr const *hdr) {
    return (hdr->param[2] & TSP_TEX_INST_FILTER_MASK) >>
        TSP_TEX_INST_FILTER_SHIFT;
}

#define TSP_TEX_FLIP_SHIFT 17
#define TSP_TEX_FLIP_MASK (3 << TSP_TEX_FLIP_SHIFT)

#define TSP_TEX_CLAMP_SHIFT 15
#define TSP_TEX_CLAMP_MASK (3 << TSP_TEX_CLAMP_SHIFT)

static inline enum tex_wrap_mode
pvr2_hdr_tex_wrap_mode_s(struct pvr2_pkt_hdr const *hdr) {
    if (hdr->param[2] & (2 << TSP_TEX_CLAMP_SHIFT))
        return TEX_WRAP_CLAMP;
    else if (hdr->param[2] & (2 << TSP_TEX_FLIP_SHIFT))
        return TEX_WRAP_FLIP;
    else
        return TEX_WRAP_REPEAT;
}

static inline enum tex_wrap_mode
pvr2_hdr_tex_wrap_mode_t(struct pvr2_pkt_hdr const *hdr) {
    if (hdr->param[2] & (1 << TSP_TEX_CLAMP_SHIFT))
        return TEX_WRAP_CLAMP;
    else if (hdr->param[2] & (1 << TSP_TEX_FLIP_SHIFT))
        return TEX_WRAP_FLIP;
    else
        return TEX_WRAP_REPEAT;
}

#define TSP_WORD_SRC_ALPHA_FACTOR_SHIFT 29
#define TSP_WORD_SRC_ALPHA_FACTOR_MASK (7 << TSP_WORD_SRC_ALPHA_FACTOR_SHIFT)
static inline enum Pvr2BlendFactor
pvr2_hdr_src_blend_factor(struct pvr2_pkt_hdr const *hdr) {
    return (hdr->param[2] & TSP_WORD_SRC_ALPHA_FACTOR_MASK) >>
        TSP_WORD_SRC_ALPHA_FACTOR_SHIFT;
}

#define TSP_WORD_DST_ALPHA_FACTOR_SHIFT 26
#define TSP_WORD_DST_ALPHA_FACTOR_MASK (7 << TSP_WORD_DST_ALPHA_FACTOR_SHIFT)
static inline enum Pvr2BlendFactor
pvr2_hdr_dst_blend_factor(struct pvr2_pkt_hdr const *hdr) {
    return (hdr->param[2] & TSP_WORD_DST_ALPHA_FACTOR_MASK) >>
        TSP_WORD_DST_ALPHA_FACTOR_SHIFT;
}

#define DEPTH_WRITE_DISABLE_SHIFT 26
#define DEPTH_WRITE_DISABLE_MASK (1 << DEPTH_WRITE_DISABLE_SHIFT)
static inline bool
pvr2_hdr_enable_depth_writes(struct pvr2_pkt_hdr const *hdr) {
    return !((hdr->param[1] & DEPTH_WRITE_DISABLE_MASK) >>
             DEPTH_WRITE_DISABLE_SHIFT);
}

#define DEPTH_FUNC_SHIFT 29
#define DEPTH_FUNC_MASK (7 << DEPTH_FUNC_SHIFT)
static inline enum Pvr2DepthFunc
pvr2_hdr_depth_func(struct pvr2_pkt_hdr const *hdr) {
    return (hdr->param[0] & DEPTH_FUNC_MASK) >> DEPTH_FUNC_SHIFT;
}

static inline uint32_t
pvr2_hdr_tex_addr(struct pvr2_pkt_hdr const *hdr) {
    return ((hdr->param[3] & TEX_CTRL_TEX_ADDR_MASK) >>
            TEX_CTRL_TEX_ADDR_SHIFT) << 3;
}

#define TA_CMD_TWO_VOLUMES_SHIFT 6
#define TA_CMD_TWO_VOLUMES_MASK (1 << TA_CMD_TWO_VOLUMES_SHIFT)
static inline bool pvr2_hdr_two_volumes_mode(struct pvr2_pkt_hdr const *hdr) {
    return hdr->param[0] & TA_CMD_TWO_VOLUMES_MASK;
}

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_TYPE_END_OF_LIST 0x0
#define TA_CMD_TYPE_USER_CLIP   0x1
#define TA_CMD_TYPE_INPUT_LIST  0x2
// what is 3?
#define TA_CMD_TYPE_POLY_HDR    0x4
#define TA_CMD_TYPE_SPRITE_HDR  0x5
#define TA_CMD_TYPE_UNKNOWN     0x6  // I can't find any info on what this is
#define TA_CMD_TYPE_VERTEX      0x7

#define TA_CMD_16_BIT_TEX_COORD_SHIFT 0
#define TA_CMD_16_BIT_TEX_COORD_MASK (1 << TA_CMD_16_BIT_TEX_COORD_SHIFT)

static inline bool pvr2_hdr_tex_coord_16_bit(struct pvr2_pkt_hdr const *hdr) {
    if (((hdr->param[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT) ==
        TA_CMD_TYPE_SPRITE_HDR) {
        return true; // force this on
    } else {
        return hdr->param[0] & TA_CMD_16_BIT_TEX_COORD_MASK;
    }
}

#define TEX_CTRL_NOT_TWIDDLED_SHIFT 26
#define TEX_CTRL_NOT_TWIDDLED_MASK (1 << TEX_CTRL_NOT_TWIDDLED_SHIFT)
static inline bool pvr2_hdr_tex_twiddle(struct pvr2_pkt_hdr const *hdr) {
    if (pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
        pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        return true;
    } else {
        return !(TEX_CTRL_NOT_TWIDDLED_MASK & hdr->param[3]);
    }
}

#define TEX_CTRL_STRIDE_SEL_SHIFT 25
#define TEX_CTRL_STRIDE_SEL_MASK (1 << TEX_CTRL_STRIDE_SEL_SHIFT)
static inline bool pvr2_hdr_stride_sel(struct pvr2_pkt_hdr const *hdr) {
    if (pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
        pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        return false;
    } else {
        if (!pvr2_hdr_tex_twiddle(hdr))
            return (bool)(TEX_CTRL_STRIDE_SEL_MASK & hdr->param[3]);
        else
            return false;
    }
}

#define TEX_CTRL_MIP_MAPPED_SHIFT 31
#define TEX_CTRL_MIP_MAPPED_MASK (1 << TEX_CTRL_MIP_MAPPED_SHIFT)
static inline bool pvr2_hdr_tex_mipmap(struct pvr2_pkt_hdr const *hdr) {
    if (pvr2_hdr_stride_sel(hdr))
        return false;
    else
        return TEX_CTRL_MIP_MAPPED_MASK & hdr->param[3];
}

/*
 * this is the upper 2-bits (for 8BPP) or 6 bits (for 4BPP) of every
 * palette address referenced by this texture.  It needs to be shifted left
 * by 2 or 6 bits and ORed with pixel values to get palette addresses.
 *
 * this field only holds meaning if tex_fmt is TEX_CTRL_PIX_FMT_4_BPP_PAL
 * or TEX_CTRL_PIX_FMT_8_BPP_PAL; otherwise it is meaningless.
 */
static inline unsigned
pvr2_hdr_tex_palette_start(struct pvr2_pkt_hdr const *hdr) {
    if (pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
        pvr2_hdr_pix_fmt(hdr) == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        return (hdr->param[3] & TEX_CTRL_PALETTE_START_MASK) >>
            TEX_CTRL_PALETTE_START_SHIFT;
    } else {
        return 0xdeadbeef;
    }
}

static inline unsigned
pvr2_hdr_user_clip_mode(struct pvr2_pkt_hdr const *hdr) {
    switch ((hdr->param[0] >> 16) & 3) {
    case 0:
    default:
        return PVR2_USER_CLIP_DISABLE;
    case 1:
        return PVR2_USER_CLIP_RESERVED;
    case 2:
        return PVR2_USER_CLIP_INSIDE;
    case 3:
        return PVR2_USER_CLIP_OUTSIDE;
    }
}

struct pvr2_pkt_user_clip {
    /*
     * these are in terms of tiles, so the actual coordinates are these
     * multiplied by 32.
     */
    unsigned xmin, ymin, xmax, ymax;
};

union pvr2_pkt_inner {
    struct pvr2_pkt_vtx vtx;
    struct pvr2_pkt_quad quad;
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

#define PVR2_CMD_MAX_LEN 64

enum pvr2_poly_type_state {

    // the given polygon type has not been opened
    PVR2_POLY_TYPE_STATE_NOT_OPENED,

    // the given polygon type is currently open for submission
    PVR2_POLY_TYPE_STATE_IN_PROGRESS,

    // the given polygon type was opened, but a continuation was requested.
    // it is temporarily closed but the data from before the continuation is
    // still valid and will be submitted.
    PVR2_POLY_TYPE_STATE_CONTINUATION,

    /*
     * the given polygon type has been opened and closed.  It cannot be
     * re-opened until the next soft reset.
     */
    PVR2_POLY_TYPE_STATE_SUBMITTED
};

/*
 * holds state which is preserved between TA FIFO packets.  Only things which
 * are set by FIFO packets should go in here.  State which (on real hardware)
 * would be updated by processing the display lists generated by the FIFO
 * packets in a STARTRENDER command does not belong here.
 */
struct pvr2_fifo_state {
    /**************************************************************************
     *
     * FIFO Buffer
     *
     * contains data which has been input to the TAFIFO but has not been
     * processed because we don't have a complete packet yet.
     *
     *************************************************************************/
    uint32_t ta_fifo32[PVR2_CMD_MAX_LEN];
    unsigned ta_fifo_word_count;

    /**************************************************************************
     *
     * coloring/blending parameters
     *
     *************************************************************************/
    enum ta_color_type ta_color_fmt;
    bool offset_color_enable;
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

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

    bool two_volumes_mode;

    /**************************************************************************
     *
     * texturing parameters
     *
     *************************************************************************/
    bool tex_enable;

    bool tex_coord_16_bit_enable;

    enum tex_wrap_mode tex_wrap_mode[2];
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;

    /**************************************************************************
     *
     * primitive parameters
     *
     *************************************************************************/
    // whether each polygon group is open/closed/etc
    enum pvr2_poly_type_state poly_type_state[PVR2_POLY_TYPE_COUNT];

    // whether or not there even is currently an open polygon group
    bool open_group;

    // the currently opened polygon group.  Only valid if open_group is true
    enum pvr2_poly_type cur_poly_type;

    // if there's an open group, this is the length of the vertex packets
    unsigned vtx_len;

    // current geometry type (either triangle strips or quads)
    enum pvr2_hdr_tp geo_tp;


    /**************************************************************************
     *
     * depth-buffering parameters
     *
     *************************************************************************/
    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;
};

struct pvr2_ta {
    struct pvr2_fifo_state fifo_state;

    // if a list is open, this is the index
    unsigned cur_list_idx;

    struct SchedEvent pvr2_op_complete_int_event,
        pvr2_op_mod_complete_int_event,
        pvr2_trans_complete_int_event,
        pvr2_trans_mod_complete_int_event,
        pvr2_pt_complete_int_event;
    bool pvr2_op_complete_int_event_scheduled,
        pvr2_op_mod_complete_int_event_scheduled,
        pvr2_trans_complete_int_event_scheduled,
        pvr2_trans_mod_complete_int_event_scheduled,
        pvr2_pt_complete_int_event_scheduled;
};

unsigned pvr2_ta_fifo_rem_bytes(void);

struct pvr2_ta_param_dims {
    /*
     * vtx_len and hdr_len will be either 8 or 16.
     * is_vert will tell you whether the current packet's length
     * is determined by vtx_len (if true) or hdr_len (if false).
     *
     * not that if is_vert is false, vtx_len will still be valid since packet
     * headers determine the length of vertex parameters.  if is_vert is true,
     * hdr_len will not be valid since it is irrelevant.
     */
    int vtx_len : 8;
    int hdr_len : 8;
    bool is_vert : 1;
};

struct pvr2_ta_param_dims pvr2_ta_get_param_dims(unsigned control_word);

/*
 * input polygon data to the TAFIFO, one 32-bit int at a time.  This is only
 * the polygon part of the TAFIFO, this doesn't apply to texture memory or YUV
 * conversion.
 */
void pvr2_tafifo_input(struct pvr2 *pvr2, uint32_t dword);

void pvr2_ta_list_continue(struct pvr2 *pvr2);

#endif
