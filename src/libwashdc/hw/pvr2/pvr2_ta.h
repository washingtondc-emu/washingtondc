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
    float base_color[4];
    float offs_color[4];
    float uv[2];
    float pos[3];

    bool end_of_strip;
};

struct pvr2_pkt_quad {
    /*
     * four vertices consisting of 3-component poistions
     *and 2-component texture coordinates
     */
    float vert_pos[4][3];
    unsigned tex_coords_packed[3];
    bool degenerate;
};

struct pvr2_pkt_hdr {
    enum pvr2_hdr_tp tp;

    unsigned vtx_len;

    enum pvr2_poly_type poly_type;

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
