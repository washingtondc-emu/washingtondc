/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/*
 * TODO: This is a terribly inaccurate, HLE-like implementation of the PowerVR2
 * that doesn't correclty emulate the interactions between TA and ISP and it
 * doesn't really even do tile rendering like it should.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "washdc/error.h"
#include "gfx/gfx.h"
#include "hw/sys/holly_intc.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"
#include "log.h"
#include "dc_sched.h"
#include "dreamcast.h"
#include "washdc/gfx/gfx_il.h"
#include "pvr2.h"
#include "pvr2_reg.h"
#include "intmath.h"

#include "pvr2_ta.h"

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_END_OF_STRIP_SHIFT 28
#define TA_CMD_END_OF_STRIP_MASK (1 << TA_CMD_END_OF_STRIP_SHIFT)

#define TA_CMD_POLY_TYPE_SHIFT 24
#define TA_CMD_POLY_TYPE_MASK (0x7 << TA_CMD_POLY_TYPE_SHIFT)

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

#define TSP_WORD_SRC_ALPHA_FACTOR_SHIFT 29
#define TSP_WORD_SRC_ALPHA_FACTOR_MASK (7 << TSP_WORD_SRC_ALPHA_FACTOR_SHIFT)

#define TSP_WORD_DST_ALPHA_FACTOR_SHIFT 26
#define TSP_WORD_DST_ALPHA_FACTOR_MASK (7 << TSP_WORD_DST_ALPHA_FACTOR_SHIFT)

#define DEPTH_FUNC_SHIFT 29
#define DEPTH_FUNC_MASK (7 << DEPTH_FUNC_SHIFT)

#define DEPTH_WRITE_DISABLE_SHIFT 26
#define DEPTH_WRITE_DISABLE_MASK (1 << DEPTH_WRITE_DISABLE_SHIFT)

static DEF_ERROR_INT_ATTR(poly_type_index);
static DEF_ERROR_INT_ATTR(geo_buf_group_index);
static DEF_ERROR_INT_ATTR(ta_fifo_cmd)
static DEF_ERROR_INT_ATTR(pvr2_global_param)
static DEF_ERROR_INT_ATTR(ta_fifo_word_count)
static DEF_ERROR_U32_ATTR(ta_fifo_word_0)
static DEF_ERROR_U32_ATTR(ta_fifo_word_1)
static DEF_ERROR_U32_ATTR(ta_fifo_word_2)
static DEF_ERROR_U32_ATTR(ta_fifo_word_3)
static DEF_ERROR_U32_ATTR(ta_fifo_word_4)
static DEF_ERROR_U32_ATTR(ta_fifo_word_5)
static DEF_ERROR_U32_ATTR(ta_fifo_word_6)
static DEF_ERROR_U32_ATTR(ta_fifo_word_7)
static DEF_ERROR_U32_ATTR(ta_fifo_word_8)
static DEF_ERROR_U32_ATTR(ta_fifo_word_9)
static DEF_ERROR_U32_ATTR(ta_fifo_word_a)
static DEF_ERROR_U32_ATTR(ta_fifo_word_b)
static DEF_ERROR_U32_ATTR(ta_fifo_word_c)
static DEF_ERROR_U32_ATTR(ta_fifo_word_d)
static DEF_ERROR_U32_ATTR(ta_fifo_word_e)
static DEF_ERROR_U32_ATTR(ta_fifo_word_f)

static char const *pvr2_poly_type_name(enum pvr2_poly_type tp) {
    switch (tp) {
    case PVR2_POLY_TYPE_OPAQUE:
        return "Opaque";
    case PVR2_POLY_TYPE_OPAQUE_MOD:
        return "Opaque Modifier Volume";
    case PVR2_POLY_TYPE_TRANS:
        return "Transparent";
    case PVR2_POLY_TYPE_TRANS_MOD:
        return "Transparent Modifier Volume";
    case PVR2_POLY_TYPE_PUNCH_THROUGH:
        return "Punch-through Polygon";
    case PVR2_POLY_TYPE_5:
        return "Unknown Polygon Type 5";
    case PVR2_POLY_TYPE_6:
        return "Unknown Polygon Type 6";
    case PVR2_POLY_TYPE_7:
        return "Unknown Polygon Type 7";
    default:
        return "ERROR - INVALID POLYGON TYPE INDEX";
    }
}

static void handle_packet(struct pvr2 *pvr2);
static void dump_fifo(struct pvr2 *pvr2);

static void
finish_poly_group(struct pvr2 *pvr2, enum pvr2_poly_type poly_type);
static void
next_poly_group(struct pvr2 *pvr2, enum pvr2_poly_type poly_type);

static int decode_poly_hdr(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_end_of_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_vtx(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_quad(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_input_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_user_clip(struct pvr2 *pvr2, struct pvr2_pkt *pkt);

// call this whenever a packet has been processed
static void ta_fifo_finish_packet(struct pvr2_ta *ta);

static void unpack_uv16(float *u_coord, float *v_coord, void const *input);
static void unpack_rgba_8888(uint32_t const *ta_fifo32, float *rgba, uint32_t input);

/*
 * the delay between when a list is rendered and when the list-complete
 * interrupt happens.
 *
 * TODO: This value has no basis in reality.  I need to run some tests on real
 * hardware and come up with a good heuristic.
 *
 * If this value is too low, it will trigger race conditions in certain games
 * which can cause them to miss interrupts.
 *
 */
#define PVR2_LIST_COMPLETE_INT_DELAY (SCHED_FREQUENCY / 1024)

static void pvr2_op_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_op_mod_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_trans_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_trans_mod_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_pt_complete_int_event_handler(struct SchedEvent *event);

static enum pvr2_poly_type_state get_poly_type_state(struct pvr2_ta *ta,
                                                     enum pvr2_poly_type tp) {
    if (tp >= PVR2_POLY_TYPE_FIRST &&
        tp < PVR2_POLY_TYPE_COUNT) {
        return ta->fifo_state.poly_type_state[tp];
    } else {
        error_set_poly_type_index((int)tp);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static void set_poly_type_state(struct pvr2_ta *ta,
                                enum pvr2_poly_type tp,
                                enum pvr2_poly_type_state state) {
    if (tp >= PVR2_POLY_TYPE_FIRST &&
        tp < PVR2_POLY_TYPE_COUNT) {
        ta->fifo_state.poly_type_state[tp] = state;
    } else {
        error_set_poly_type_index((int)tp);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

void pvr2_ta_init(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;

    ta->pvr2_op_complete_int_event.handler =
        pvr2_op_complete_int_event_handler;
    ta->pvr2_op_mod_complete_int_event.handler =
        pvr2_op_mod_complete_int_event_handler;
    ta->pvr2_trans_complete_int_event.handler =
        pvr2_trans_complete_int_event_handler;
    ta->pvr2_trans_mod_complete_int_event.handler =
        pvr2_trans_mod_complete_int_event_handler;
    ta->pvr2_pt_complete_int_event.handler =
        pvr2_pt_complete_int_event_handler;

    ta->pvr2_op_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_op_mod_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_trans_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_trans_mod_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_pt_complete_int_event.arg_ptr = pvr2;

    ta->cur_list_idx = 0;
}

void pvr2_ta_cleanup(struct pvr2 *pvr2) {
}

uint32_t pvr2_ta_fifo_poly_read_32(addr32_t addr, void *ctxt) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: trying to read 4 bytes from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    PVR2_TRACE("writing 4 bytes to TA polygon FIFO: 0x%08x\n", (unsigned)val);
    pvr2_tafifo_input(pvr2, val);
}

uint16_t pvr2_ta_fifo_poly_read_16(addr32_t addr, void *ctxt) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: trying to read 2 bytes from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}


uint8_t pvr2_ta_fifo_poly_read_8(addr32_t addr, void *ctxt) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: trying to read 1 byte from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

float pvr2_ta_fifo_poly_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = pvr2_ta_fifo_poly_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_ta_fifo_poly_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_ta_fifo_poly_write_32(addr, tmp, ctxt);
}

double pvr2_ta_fifo_poly_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_ta_fifo_poly_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

#ifdef PVR2_LOG_VERBOSE
static void dump_pkt_hdr(struct pvr2_pkt_hdr const *hdr) {
#define HDR_BOOL(hdr, mem) PVR2_TRACE("\t"#mem": %s\n", hdr->mem ? "true" : "false")
#define HDR_INT(hdr, mem) PVR2_TRACE("\t"#mem": %d\n", (int)hdr->mem)
#define HDR_HEX(hdr, mem) PVR2_TRACE("\t"#mem": 0x%08x\n", (int)hdr->mem)
    PVR2_TRACE("packet header:\n");
    PVR2_TRACE("\ttype: %s\n", hdr->tp == PVR2_HDR_TRIANGLE_STRIP ?
               "triangle strip" : "quadrilateral");
    HDR_INT(hdr, vtx_len);
    PVR2_TRACE("\tpolygon type: %s\n", pvr2_poly_type_name(hdr->poly_type));
    HDR_BOOL(hdr, tex_enable);
    HDR_HEX(hdr, tex_addr);
    PVR2_TRACE("\ttexture dimensions: %ux%u\n",
               1 << hdr->tex_width_shift, 1 << hdr->tex_height_shift);
    HDR_BOOL(hdr, tex_twiddle);
    HDR_BOOL(hdr, stride_sel);
    HDR_BOOL(hdr, tex_vq_compression);
    HDR_BOOL(hdr, tex_mipmap);
    HDR_INT(hdr, pix_fmt);
    HDR_INT(hdr, tex_inst);
    HDR_INT(hdr, tex_filter);
    HDR_INT(hdr, tex_wrap_mode[0]);
    HDR_INT(hdr, tex_wrap_mode[1]);
    HDR_INT(hdr, ta_color_fmt);
    HDR_INT(hdr, src_blend_factor);
    HDR_INT(hdr, dst_blend_factor);
    HDR_BOOL(hdr, enable_depth_writes);
    HDR_INT(hdr, depth_func);
    HDR_BOOL(hdr, two_volumes_mode);
    HDR_BOOL(hdr, offset_color_enable);
    HDR_BOOL(hdr, gourad_shading_enable);
    HDR_BOOL(hdr, tex_coord_16_bit_enable);
}
#endif

static void on_pkt_hdr_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_pkt_hdr const *hdr = &pkt->dat.hdr;
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;

#ifdef PVR2_LOG_VERBOSE
    dump_pkt_hdr(hdr);
#endif

    if (hdr->two_volumes_mode)
        LOG_DBG("Unimplemented two-volumes mode polygon!\n");

    if (ta->fifo_state.cur_poly_type != hdr->poly_type) {
        if (get_poly_type_state(ta, hdr->poly_type) ==
            PVR2_POLY_TYPE_STATE_SUBMITTED) {
            /*
             * TODO: I want to make this an ERROR_UNIMPLEMENTED, but enough
             * games do it that I have to accept that somehow it works out on
             * real hardware.
             */
            LOG_ERROR("PVR2: re-opening polython type %s after it was already "
                      "submitted?\n", pvr2_poly_type_name(hdr->poly_type));
        }

        if (ta->fifo_state.cur_poly_type == PVR2_POLY_TYPE_NONE) {
            PVR2_TRACE("Opening polygon group \"%s\"\n",
                       pvr2_poly_type_name(hdr->poly_type));
            set_poly_type_state(ta, hdr->poly_type,
                                PVR2_POLY_TYPE_STATE_IN_PROGRESS);
            ta->fifo_state.cur_poly_type = hdr->poly_type;
            ta->fifo_state.open_group = true;
        } else {
            PVR2_TRACE("software did not close polygon group %d\n",
                      (int)ta->fifo_state.cur_poly_type);
            PVR2_TRACE("Beginning polygon group within group \"%s\"\n",
                       pvr2_poly_type_name(ta->fifo_state.cur_poly_type));

            next_poly_group(pvr2, ta->fifo_state.cur_poly_type);
        }
    } else {
        PVR2_TRACE("Beginning polygon group within group \"%s\"\n",
                   pvr2_poly_type_name(hdr->poly_type));

        next_poly_group(pvr2, ta->fifo_state.cur_poly_type);
    }

    /*
     * XXX this happens before the texture caching code because we need to be
     * able to disable textures if the cache is full, but hdr is const.
     */
    ta->fifo_state.vtx_len = hdr->vtx_len;
    ta->fifo_state.tex_enable = hdr->tex_enable;
    ta->fifo_state.geo_tp = hdr->tp;
    ta->fifo_state.tex_coord_16_bit_enable = hdr->tex_coord_16_bit_enable;
    ta->fifo_state.two_volumes_mode = hdr->two_volumes_mode;
    ta->fifo_state.ta_color_fmt = hdr->ta_color_fmt;
    ta->fifo_state.offset_color_enable = hdr->offset_color_enable;
    ta->fifo_state.src_blend_factor = hdr->src_blend_factor;
    ta->fifo_state.dst_blend_factor = hdr->dst_blend_factor;
    ta->fifo_state.tex_wrap_mode[0] = hdr->tex_wrap_mode[0];
    ta->fifo_state.tex_wrap_mode[1] = hdr->tex_wrap_mode[1];
    ta->fifo_state.enable_depth_writes = hdr->enable_depth_writes;
    ta->fifo_state.depth_func = hdr->depth_func;
    ta->fifo_state.tex_inst = hdr->tex_inst;
    ta->fifo_state.tex_filter = hdr->tex_filter;

    // queue up in a display list
    struct pvr2_display_list *cur_list = core->disp_lists + ta->cur_list_idx;
    if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT || !cur_list->valid)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    struct pvr2_display_list_command *cmd =
        pvr2_list_alloc_new_cmd(cur_list, ta->fifo_state.cur_poly_type);
    if (!cmd) {
        LOG_ERROR("%s unable to allocate display list entry!\n", __func__);
        return;
    }

    cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_HEADER;
    struct pvr2_display_list_command_header *cmd_hdr = &cmd->hdr;
    cmd_hdr->geo_tp = ta->fifo_state.geo_tp;
    cmd_hdr->tex_enable = ta->fifo_state.tex_enable;
    cmd_hdr->tex_wrap_mode[0] = ta->fifo_state.tex_wrap_mode[0];
    cmd_hdr->tex_wrap_mode[1] = ta->fifo_state.tex_wrap_mode[1];
    cmd_hdr->tex_inst = ta->fifo_state.tex_inst;
    cmd_hdr->tex_filter = ta->fifo_state.tex_filter;
    cmd_hdr->src_blend_factor = ta->fifo_state.src_blend_factor;
    cmd_hdr->dst_blend_factor = ta->fifo_state.dst_blend_factor;
    cmd_hdr->enable_depth_writes = ta->fifo_state.enable_depth_writes;
    cmd_hdr->depth_func = ta->fifo_state.depth_func;

    cmd_hdr->tex_width_shift = hdr->tex_width_shift;
    cmd_hdr->tex_height_shift = hdr->tex_height_shift;
    cmd_hdr->stride_sel = hdr->stride_sel;
    cmd_hdr->tex_twiddle = hdr->tex_twiddle;
    cmd_hdr->pix_fmt = hdr->pix_fmt;
    cmd_hdr->tex_addr = hdr->tex_addr;
    cmd_hdr->tex_palette_start = hdr->tex_palette_start;
    cmd_hdr->tex_vq_compression = hdr->tex_vq_compression;
    cmd_hdr->tex_mipmap = hdr->tex_mipmap;
}

static void
on_pkt_end_of_list_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;

    PVR2_TRACE("END-OF-LIST PACKET!\n");

    if (ta->fifo_state.cur_poly_type == PVR2_POLY_TYPE_NONE) {
        LOG_WARN("attempt to close poly group when no group is open!\n");
        /*
         * SEGA Bass Fishing does this.  At bootup, before the loading icon, it
         * appears to think it's submitting 64-bit vertices, but they're
         * actually 32-bit (control word is 0x82000000).  Because of this, the
         * vertex packets get cut in half and the second halves are interpreted
         * as end-of-list packets because they begin with 0.
         *
         * Intended behavior of the developers may have been to have been to
         * gradually darken the screen because one of the dwords in the
         * second-half of each packet increases by 0x01010101 with each
         * successive packet (meaning it is intended to be 32-bit packed RGBA
         * color).  This behavior does not manifest on real hardware, so my
         * conclusion is that the devs must have fucked up.
         */
        return;
    }

    struct dc_clock *clk = pvr2->clk;
    dc_cycle_stamp_t int_when =
        clock_cycle_stamp(clk) + PVR2_LIST_COMPLETE_INT_DELAY;

    switch (ta->fifo_state.cur_poly_type) {
    case PVR2_POLY_TYPE_OPAQUE:
        if (!ta->pvr2_op_complete_int_event_scheduled) {
            ta->pvr2_op_complete_int_event_scheduled = true;
            ta->pvr2_op_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_op_complete_int_event);
        }
        break;
    case PVR2_POLY_TYPE_OPAQUE_MOD:
        if (!ta->pvr2_op_mod_complete_int_event_scheduled) {
            ta->pvr2_op_mod_complete_int_event_scheduled = true;
            ta->pvr2_op_mod_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_op_mod_complete_int_event);
        }
        break;
    case PVR2_POLY_TYPE_TRANS:
        if (!ta->pvr2_trans_complete_int_event_scheduled) {
            ta->pvr2_trans_complete_int_event_scheduled = true;
            ta->pvr2_trans_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_trans_complete_int_event);
        }
        break;
    case PVR2_POLY_TYPE_TRANS_MOD:
        if (!ta->pvr2_trans_mod_complete_int_event_scheduled) {
            ta->pvr2_trans_mod_complete_int_event_scheduled = true;
            ta->pvr2_trans_mod_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_trans_mod_complete_int_event);
        }
        break;
    case PVR2_POLY_TYPE_PUNCH_THROUGH:
        if (!ta->pvr2_pt_complete_int_event_scheduled) {
            ta->pvr2_pt_complete_int_event_scheduled = true;
            ta->pvr2_pt_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_pt_complete_int_event);
        }
        break;
    default:
        /*
         * this can never actually happen because this
         * functionshould have returned early above
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (get_poly_type_state(ta, ta->fifo_state.cur_poly_type) !=
        PVR2_POLY_TYPE_STATE_IN_PROGRESS) {
        error_set_feature("closing a polygon group that isn't open");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    enum pvr2_poly_type poly_type = ta->fifo_state.cur_poly_type;
    finish_poly_group(pvr2, ta->fifo_state.cur_poly_type);
    set_poly_type_state(ta, ta->fifo_state.cur_poly_type,
                        PVR2_POLY_TYPE_STATE_SUBMITTED);
    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;

    // queue up in a display list
    struct pvr2_display_list *cur_list = core->disp_lists + ta->cur_list_idx;
    if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT || !cur_list->valid)
        RAISE_ERROR(ERROR_INTEGRITY);
    struct pvr2_display_list_command *cmd =
        pvr2_list_alloc_new_cmd(cur_list, poly_type);
    if (cmd) {
        cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_END_OF_GROUP;
        cmd->end_of_group.poly_type = poly_type;
    } else {
        LOG_ERROR("%s unable to allocate display list entry!\n", __func__);
    }

    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;
}

static void
on_quad_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_pkt_quad const *quad = &pkt->dat.quad;

    if (quad->degenerate)
        return;

    // queue up in a display list
    struct pvr2_display_list *cur_list = core->disp_lists + ta->cur_list_idx;
    if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT || !cur_list->valid)
        RAISE_ERROR(ERROR_INTEGRITY);
    struct pvr2_display_list_command *cmd =
        pvr2_list_alloc_new_cmd(cur_list, ta->fifo_state.cur_poly_type);
    if (!cmd) {
        LOG_ERROR("%s unable to allocate display list entry!\n", __func__);
        return;
    }

    cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_QUAD;

    struct pvr2_display_list_quad *dl_quad = &cmd->quad;
    memcpy(dl_quad->vert_pos, quad->vert_pos, sizeof(dl_quad->vert_pos));
    memcpy(dl_quad->tex_coords_packed,
           quad->tex_coords_packed, sizeof(dl_quad->tex_coords_packed));
    memcpy(dl_quad->base_color,
           ta->fifo_state.sprite_base_color_rgba, sizeof(dl_quad->base_color));
    memcpy(dl_quad->offs_color,
           ta->fifo_state.sprite_offs_color_rgba, sizeof(dl_quad->offs_color));
    dl_quad->degenerate = quad->degenerate;
}

static void
on_pkt_vtx_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_pkt_vtx const *vtx = &pkt->dat.vtx;

#ifdef INVARIANTS
    if (ta->fifo_state.geo_tp != PVR2_HDR_TRIANGLE_STRIP)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    ta->fifo_state.open_group = true;

    if (ta->fifo_state.cur_poly_type >= PVR2_POLY_TYPE_FIRST &&
        ta->fifo_state.cur_poly_type <= PVR2_POLY_TYPE_LAST) {
        // queue up in a display list
        struct pvr2_display_list *cur_list = core->disp_lists + ta->cur_list_idx;
        if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT || !cur_list->valid)
            RAISE_ERROR(ERROR_INTEGRITY);
        struct pvr2_display_list_command *cmd =
            pvr2_list_alloc_new_cmd(cur_list, ta->fifo_state.cur_poly_type);
        if (!cmd) {
            LOG_ERROR("%s unable to allocate display list entry!\n", __func__);
            return;
        }

        cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_VERTEX;
        struct pvr2_display_list_vertex *dl_vtx = &cmd->vtx;
        memcpy(dl_vtx->pos, vtx->pos, sizeof(dl_vtx->pos));
        memcpy(dl_vtx->tex_coord, vtx->uv, sizeof(dl_vtx->tex_coord));
        memcpy(dl_vtx->base_color, vtx->base_color, sizeof(dl_vtx->base_color));
        memcpy(dl_vtx->offs_color, vtx->offs_color, sizeof(dl_vtx->offs_color));
        dl_vtx->end_of_strip = vtx->end_of_strip;
    }
}

static void
on_pkt_input_list_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    LOG_WARN("PVR2: unimplemented type 2 (input list) packet received\n");
}

static void on_pkt_user_clip_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    LOG_WARN("PVR2: unimplemented type 1 (user tile clip) packet received\n");
    PVR2_TRACE("\tmin: (%u, %u)\n\tmax: (%u, %u)\n",
               pkt->dat.user_clip.xmin * 32,
               pkt->dat.user_clip.ymin * 32,
               pkt->dat.user_clip.xmax * 32,
               pkt->dat.user_clip.ymax * 32);
}

static void handle_packet(struct pvr2 *pvr2) {
    struct pvr2_pkt pkt;
    uint32_t const *ta_fifo32 = (uint32_t const*)pvr2->ta.fifo_state.ta_fifo32;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;
    struct pvr2_ta *ta = &pvr2->ta;

    switch(cmd_tp) {
    case TA_CMD_TYPE_POLY_HDR:
    case TA_CMD_TYPE_SPRITE_HDR:
        if (decode_poly_hdr(pvr2, &pkt) == 0) {
            PVR2_TRACE("header packet received\n");
            on_pkt_hdr_received(pvr2, &pkt);
            ta_fifo_finish_packet(ta);
        }
        break;
    case TA_CMD_TYPE_END_OF_LIST:
        if (decode_end_of_list(pvr2, &pkt) == 0) {
            PVR2_TRACE("end-of-list packet received\n");
            on_pkt_end_of_list_received(pvr2, &pkt);
            ta_fifo_finish_packet(ta);
        }
        break;
    case TA_CMD_TYPE_VERTEX:
        if (ta->fifo_state.geo_tp == PVR2_HDR_TRIANGLE_STRIP) {
            if (decode_vtx(pvr2, &pkt) == 0) {
                PVR2_TRACE("vertex packet received\n");
                on_pkt_vtx_received(pvr2, &pkt);
                ta_fifo_finish_packet(ta);
            }
        } else {
            if (decode_quad(pvr2, &pkt) == 0) {
                PVR2_TRACE("quadrilateral vertex packet received\n");
                on_quad_received(pvr2, &pkt);
                ta_fifo_finish_packet(ta);
            }
        }
        break;
    case TA_CMD_TYPE_INPUT_LIST:
        if (decode_input_list(pvr2, &pkt) == 0) {
            PVR2_TRACE("input list packet received\n");
            on_pkt_input_list_received(pvr2, &pkt);
            ta_fifo_finish_packet(ta);
        }
        break;
    case TA_CMD_TYPE_USER_CLIP:
        if (decode_user_clip(pvr2, &pkt) == 0) {
            PVR2_TRACE("user clip packet received\n");
            on_pkt_user_clip_received(pvr2, &pkt);
            ta_fifo_finish_packet(ta);
        }
        break;
    default:
        LOG_ERROR("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
        dump_fifo(pvr2);
        error_set_feature("PVR2 command type");
        error_set_ta_fifo_cmd(cmd_tp);
        error_set_ta_fifo_word_count(pvr2->ta.fifo_state.ta_fifo_word_count);
        error_set_ta_fifo_word_0(ta_fifo32[0]);
        error_set_ta_fifo_word_1(ta_fifo32[1]);
        error_set_ta_fifo_word_2(ta_fifo32[2]);
        error_set_ta_fifo_word_3(ta_fifo32[3]);
        error_set_ta_fifo_word_4(ta_fifo32[4]);
        error_set_ta_fifo_word_5(ta_fifo32[5]);
        error_set_ta_fifo_word_6(ta_fifo32[6]);
        error_set_ta_fifo_word_7(ta_fifo32[7]);
        error_set_ta_fifo_word_8(ta_fifo32[8]);
        error_set_ta_fifo_word_9(ta_fifo32[9]);
        error_set_ta_fifo_word_a(ta_fifo32[10]);
        error_set_ta_fifo_word_b(ta_fifo32[11]);
        error_set_ta_fifo_word_c(ta_fifo32[12]);
        error_set_ta_fifo_word_d(ta_fifo32[13]);
        error_set_ta_fifo_word_e(ta_fifo32[14]);
        error_set_ta_fifo_word_f(ta_fifo32[15]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void pvr2_tafifo_input(struct pvr2 *pvr2, uint32_t dword) {
    struct pvr2_ta *ta = &pvr2->ta;
    ta->fifo_state.ta_fifo32[ta->fifo_state.ta_fifo_word_count++] = dword;

    if (!(ta->fifo_state.ta_fifo_word_count % 8))
        handle_packet(pvr2);
}

static void dump_fifo(struct pvr2 *pvr2) {
#ifdef ENABLE_LOG_DEBUG
    unsigned idx;
    uint32_t const *ta_fifo32 = pvr2->ta.fifo_state.ta_fifo32;
    LOG_DBG("Dumping FIFO: %u bytes\n", pvr2->ta.fifo_state.ta_fifo_word_count*4);
    for (idx = 0; idx < pvr2->ta.fifo_state.ta_fifo_word_count; idx++)
        LOG_DBG("\t0x%08x\n", (unsigned)ta_fifo32[idx]);
#endif
}

static int decode_end_of_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    pkt->tp = PVR2_PKT_END_OF_LIST;
    return 0;
}

static int decode_quad(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;

    if (ta->fifo_state.ta_fifo_word_count < ta->fifo_state.vtx_len) {
        return -1;
    } else if (ta->fifo_state.ta_fifo_word_count > ta->fifo_state.vtx_len) {
        LOG_ERROR("byte count is %u, vtx_len is %u\n",
                  ta->fifo_state.ta_fifo_word_count * 4, ta->fifo_state.vtx_len * 4);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    struct pvr2_pkt_quad *quad = &pkt->dat.quad;
    float ta_fifo_float[PVR2_CMD_MAX_LEN];
    memcpy(ta_fifo_float, ta->fifo_state.ta_fifo32, sizeof(ta_fifo_float));

    /*
     * four quadrilateral vertices.  the z-coordinate of p4 is determined
     * automatically by the PVR2 so it is not possible to specify a non-coplanar
     * set of vertices.
     */
    float *p1 = quad->vert_pos[0];
    p1[0] = ta_fifo_float[1];
    p1[1] = ta_fifo_float[2];
    p1[2] = 1.0 / ta_fifo_float[3];

    float *p2 = quad->vert_pos[1];
    p2[0] = ta_fifo_float[4];
    p2[1] = ta_fifo_float[5];
    p2[2] = 1.0 / ta_fifo_float[6];

    float *p3 = quad->vert_pos[2];
    p3[0] = ta_fifo_float[7];
    p3[1] = ta_fifo_float[8];
    p3[2] = 1.0 / ta_fifo_float[9];

    float *p4 = quad->vert_pos[3];
    p4[0] = ta_fifo_float[10];
    p4[1] = ta_fifo_float[11];
    // p4[2] will be determined later

    quad->tex_coords_packed[0] = ta->fifo_state.ta_fifo32[13];
    quad->tex_coords_packed[1] = ta->fifo_state.ta_fifo32[14];
    quad->tex_coords_packed[2] = ta->fifo_state.ta_fifo32[15];

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
        (norm[0] * norm[0] + norm[1] * norm[1] + norm[2] * norm[2] == 0.0f)) {
        // make it obvious it's degenerate
        quad->degenerate = true;
        return 0;
    } else {
        quad->degenerate = false;
    }

    // hyperplane translation
    float dist = -norm[0] * p1[0] - norm[1] * p1[1] - norm[2] * p1[2];

    p4[2] = -1.0f * (dist + norm[0] * p4[0] + norm[1] * p4[1]) / norm[2];

    return 0;
}

static int decode_vtx(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    uint32_t const *ta_fifo32 = (uint32_t const*)ta->fifo_state.ta_fifo32;

    if (ta->fifo_state.ta_fifo_word_count < ta->fifo_state.vtx_len)
        return -1;
    else if (ta->fifo_state.ta_fifo_word_count > ta->fifo_state.vtx_len) {
        LOG_ERROR("byte count is %u, vtx_len is %u\n",
                  ta->fifo_state.ta_fifo_word_count * 4, ta->fifo_state.vtx_len * 4);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pkt->tp = PVR2_PKT_VTX;
    struct pvr2_pkt_vtx *vtx = &pkt->dat.vtx;

    vtx->end_of_strip = (bool)(ta_fifo32[0] & TA_CMD_END_OF_STRIP_MASK);

    memcpy(vtx->pos, ta_fifo32 + 1, 3 * sizeof(float));

    if (ta->fifo_state.tex_enable) {
        if (ta->fifo_state.tex_coord_16_bit_enable)
            unpack_uv16(vtx->uv, vtx->uv + 1, ta_fifo32 + 4);
        else
            memcpy(vtx->uv, ta_fifo32 + 4, 2 * sizeof(float));
    }

    if (ta->fifo_state.two_volumes_mode) {
        switch (ta->fifo_state.ta_color_fmt) {
        case TA_COLOR_TYPE_PACKED:
            if (ta->fifo_state.tex_enable)
                unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[6]);
            else
                unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[4]);
            if (ta->fifo_state.offset_color_enable && ta->fifo_state.tex_enable) {
                unpack_rgba_8888(ta_fifo32, vtx->offs_color, ta_fifo32[7]);
            } else {
                vtx->offs_color[0] = 0.0f;
                vtx->offs_color[1] = 0.0f;
                vtx->offs_color[2] = 0.0f;
                vtx->offs_color[3] = 0.0f;
            }
            break;
        case TA_COLOR_TYPE_INTENSITY_MODE_1:
        case TA_COLOR_TYPE_INTENSITY_MODE_2:
            {
                float base_intensity, offs_intensity;
                if (ta->fifo_state.tex_enable) {
                    memcpy(&base_intensity, ta_fifo32 + 6, sizeof(float));
                    memcpy(&offs_intensity, ta_fifo32 + 7, sizeof(float));
                } else {
                    memcpy(&base_intensity, ta_fifo32 + 4, sizeof(float));
                    memcpy(&offs_intensity, ta_fifo32 + 5, sizeof(float));
                }
                vtx->base_color[0] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[0];
                vtx->base_color[1] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[1];
                vtx->base_color[2] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[2];
                vtx->base_color[3] = ta->fifo_state.poly_base_color_rgba[3];
                if (ta->fifo_state.offset_color_enable) {
                    vtx->offs_color[0] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[0];
                    vtx->offs_color[1] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[1];
                    vtx->offs_color[2] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[2];
                    vtx->offs_color[3] = ta->fifo_state.poly_offs_color_rgba[3];
                } else {
                    vtx->offs_color[0] = 0.0f;
                    vtx->offs_color[1] = 0.0f;
                    vtx->offs_color[2] = 0.0f;
                    vtx->offs_color[3] = 0.0f;
                }
            }
            break;
        case TA_COLOR_TYPE_FLOAT:
            // this is not supported, AFAIK
        default:
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
    } else {
        switch (ta->fifo_state.ta_color_fmt) {
        case TA_COLOR_TYPE_PACKED:
            unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[6]);
            if (ta->fifo_state.offset_color_enable) {
                unpack_rgba_8888(ta_fifo32, vtx->offs_color, ta_fifo32[7]);
            } else {
                vtx->offs_color[0] = 0.0f;
                vtx->offs_color[1] = 0.0f;
                vtx->offs_color[2] = 0.0f;
                vtx->offs_color[3] = 0.0f;
            }
            break;
        case TA_COLOR_TYPE_FLOAT:
            if (ta->fifo_state.tex_enable) {
                memcpy(vtx->base_color + 3, ta_fifo32 + 8, sizeof(float));
                memcpy(vtx->base_color + 0, ta_fifo32 + 9, sizeof(float));
                memcpy(vtx->base_color + 1, ta_fifo32 + 10, sizeof(float));
                memcpy(vtx->base_color + 2, ta_fifo32 + 11, sizeof(float));
                if (ta->fifo_state.offset_color_enable) {
                    memcpy(vtx->offs_color + 3, ta_fifo32 + 12,
                           sizeof(float));
                    memcpy(vtx->offs_color + 0, ta_fifo32 + 13,
                           sizeof(float));
                    memcpy(vtx->offs_color + 1, ta_fifo32 + 14,
                           sizeof(float));
                    memcpy(vtx->offs_color + 2, ta_fifo32 + 15,
                           sizeof(float));
                } else {
                    vtx->offs_color[0] = 0.0f;
                    vtx->offs_color[1] = 0.0f;
                    vtx->offs_color[2] = 0.0f;
                    vtx->offs_color[3] = 0.0f;
                }
            } else {
                memcpy(vtx->base_color + 3, ta_fifo32 + 4, sizeof(float));
                memcpy(vtx->base_color + 0, ta_fifo32 + 5, sizeof(float));
                memcpy(vtx->base_color + 1, ta_fifo32 + 6, sizeof(float));
                memcpy(vtx->base_color + 2, ta_fifo32 + 7, sizeof(float));

                vtx->offs_color[0] = 0.0f;
                vtx->offs_color[1] = 0.0f;
                vtx->offs_color[2] = 0.0f;
                vtx->offs_color[3] = 0.0f;
            }
            break;
        case TA_COLOR_TYPE_INTENSITY_MODE_1:
        case TA_COLOR_TYPE_INTENSITY_MODE_2:
            {
                float base_intensity, offs_intensity;
                memcpy(&base_intensity, ta_fifo32 + 6, sizeof(float));
                memcpy(&offs_intensity, ta_fifo32 + 7, sizeof(float));
                vtx->base_color[0] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[0];
                vtx->base_color[1] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[1];
                vtx->base_color[2] =
                    base_intensity * ta->fifo_state.poly_base_color_rgba[2];
                vtx->base_color[3] = ta->fifo_state.poly_base_color_rgba[3];
                if (ta->fifo_state.offset_color_enable) {
                    vtx->offs_color[0] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[0];
                    vtx->offs_color[1] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[1];
                    vtx->offs_color[2] =
                        offs_intensity * ta->fifo_state.poly_offs_color_rgba[2];
                    vtx->offs_color[3] = ta->fifo_state.poly_offs_color_rgba[3];
                } else {
                    vtx->offs_color[0] = 0.0f;
                    vtx->offs_color[1] = 0.0f;
                    vtx->offs_color[2] = 0.0f;
                    vtx->offs_color[3] = 0.0f;
                }
            }
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }
    }

    return 0;
}

static int decode_user_clip(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    uint32_t const *ta_fifo32 = (uint32_t const*)pvr2->ta.fifo_state.ta_fifo32;
    struct pvr2_pkt_user_clip *user_clip = &pkt->dat.user_clip;

    pkt->tp = PVR2_PKT_USER_CLIP;
    user_clip->xmin = ta_fifo32[4];
    user_clip->ymin = ta_fifo32[5];
    user_clip->xmax = ta_fifo32[6];
    user_clip->ymax = ta_fifo32[7];

    return 0;
}

struct pvr2_ta_param_dims pvr2_ta_get_param_dims(unsigned ctrl) {
    struct pvr2_ta_param_dims ret = {
        .hdr_len = -1,
        .vtx_len = -1
    };

    unsigned param_tp = (ctrl & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch (param_tp) {
    case TA_CMD_TYPE_POLY_HDR:
        {
            ret.is_vert = false;

            enum pvr2_poly_type poly_type =
                (enum pvr2_poly_type)((ctrl & TA_CMD_POLY_TYPE_MASK) >>
                                      TA_CMD_POLY_TYPE_SHIFT);

            if (poly_type == PVR2_POLY_TYPE_OPAQUE_MOD ||
                poly_type == PVR2_POLY_TYPE_TRANS_MOD) {
                ret.hdr_len = 8;
                ret.vtx_len = 16;
            } else {
                ret.hdr_len = 8;
                ret.vtx_len = 8;

                bool tex_enable = (bool)(ctrl & TA_CMD_TEX_ENABLE_MASK);
                bool two_volumes_mode = (bool)(ctrl & TA_CMD_TWO_VOLUMES_MASK);
                enum ta_color_type col_tp =
                    (enum ta_color_type)((ctrl & TA_CMD_COLOR_TYPE_MASK) >>
                                         TA_CMD_COLOR_TYPE_SHIFT);
                bool offset_color_enable =
                    (bool)(ctrl & TA_CMD_OFFSET_COLOR_MASK);

                if (col_tp == TA_COLOR_TYPE_INTENSITY_MODE_1 &&
                    (two_volumes_mode || (tex_enable && offset_color_enable))) {
                    ret.hdr_len = 16;
                }

                if (tex_enable) {
                    if ((!two_volumes_mode && col_tp == TA_COLOR_TYPE_FLOAT) ||
                        (two_volumes_mode && col_tp != TA_COLOR_TYPE_FLOAT))
                        ret.vtx_len = 16;
                }
            }
        }
        break;
    case TA_CMD_TYPE_SPRITE_HDR:
        ret.is_vert = false;
        ret.hdr_len = 8;
        ret.vtx_len = 16;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

#ifdef INVARIANTS
    // sanity checking
    if (ret.is_vert) {
        if (ret.vtx_len == -1 || ret.hdr_len != -1)
            RAISE_ERROR(ERROR_INTEGRITY);
    } else {
        if (ret.vtx_len == -1 || ret.hdr_len == -1)
            RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    return ret;
}

static int decode_poly_hdr(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    uint32_t const *ta_fifo32 = (uint32_t const*)ta->fifo_state.ta_fifo32;
    struct pvr2_pkt_hdr *hdr = &pkt->dat.hdr;

    unsigned param_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;
    enum pvr2_hdr_tp tp;

    struct pvr2_ta_param_dims dims = pvr2_ta_get_param_dims(ta_fifo32[0]);
    if (dims.is_vert)
        RAISE_ERROR(ERROR_INTEGRITY);
    unsigned hdr_len = dims.hdr_len;
    unsigned vtx_len = dims.vtx_len;

    // we need these to figure out whether the header is 32 bytes or 64 bytes.
    bool two_volumes_mode = (bool)(ta_fifo32[0] & TA_CMD_TWO_VOLUMES_MASK);
    enum ta_color_type col_tp =
        (enum ta_color_type)((ta_fifo32[0] & TA_CMD_COLOR_TYPE_MASK) >>
                             TA_CMD_COLOR_TYPE_SHIFT);
    bool tex_enable = (bool)(ta_fifo32[0] & TA_CMD_TEX_ENABLE_MASK);
    bool offset_color_enable = (bool)(ta_fifo32[0] & TA_CMD_OFFSET_COLOR_MASK);
    enum pvr2_poly_type poly_type =
        (enum pvr2_poly_type)((ta_fifo32[0] & TA_CMD_POLY_TYPE_MASK) >>
                              TA_CMD_POLY_TYPE_SHIFT);

    if (param_tp == TA_CMD_TYPE_POLY_HDR)
        tp = PVR2_HDR_TRIANGLE_STRIP;
    else if (param_tp == TA_CMD_TYPE_SPRITE_HDR)
        tp = PVR2_HDR_QUAD;
    else
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (ta->fifo_state.ta_fifo_word_count < hdr_len)
        return -1;
    else if (ta->fifo_state.ta_fifo_word_count > hdr_len)
        RAISE_ERROR(ERROR_INTEGRITY);

    pkt->tp = PVR2_PKT_HDR;
    hdr->tp = tp;
    hdr->vtx_len = vtx_len;

    hdr->poly_type = poly_type;

    hdr->two_volumes_mode = two_volumes_mode;
    hdr->tex_enable = tex_enable;
    hdr->ta_color_fmt = col_tp;

    /*
     * When textures are disabled, offset colors are implicitly disabled even
     * if the offset_color_enable bit was set.
     */
    if (hdr->tex_enable &&
        hdr->ta_color_fmt != TA_COLOR_TYPE_PACKED &&
        hdr->ta_color_fmt != TA_COLOR_TYPE_FLOAT) {
        hdr->offset_color_enable = offset_color_enable;
    } else {
        hdr->offset_color_enable = false;
    }

    hdr->tex_width_shift = 3 +
        ((ta_fifo32[2] & TSP_TEX_WIDTH_MASK) >> TSP_TEX_WIDTH_SHIFT);
    hdr->tex_height_shift = 3 +
        ((ta_fifo32[2] & TSP_TEX_HEIGHT_MASK) >> TSP_TEX_HEIGHT_SHIFT);
    hdr->tex_inst = (ta_fifo32[2] & TSP_TEX_INST_MASK) >>
        TSP_TEX_INST_SHIFT;
    hdr->pix_fmt =
        (enum TexCtrlPixFmt)((ta_fifo32[3] & TEX_CTRL_PIX_FMT_MASK) >>
                             TEX_CTRL_PIX_FMT_SHIFT);


    hdr->gourad_shading_enable =
        (bool)(ta_fifo32[0] & TA_CMD_GOURAD_SHADING_MASK);
    hdr->tex_coord_16_bit_enable =
        (bool)(ta_fifo32[0] & TA_CMD_16_BIT_TEX_COORD_MASK);

    if (hdr->pix_fmt != TEX_CTRL_PIX_FMT_4_BPP_PAL &&
        hdr->pix_fmt != TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        hdr->tex_twiddle = !(bool)(TEX_CTRL_NOT_TWIDDLED_MASK & ta_fifo32[3]);
        if (!hdr->tex_twiddle)
            hdr->stride_sel = (bool)(TEX_CTRL_STRIDE_SEL_MASK & ta_fifo32[3]);
        else
            hdr->stride_sel = false;
        hdr->tex_palette_start = 0xdeadbeef;
    } else {
        hdr->tex_twiddle = true;
        hdr->stride_sel = false;
        hdr->tex_palette_start = (ta_fifo32[3] & TEX_CTRL_PALETTE_START_MASK) >>
            TEX_CTRL_PALETTE_START_SHIFT;
    }

    if (hdr->stride_sel)
        hdr->tex_mipmap = false;
    else
        hdr->tex_mipmap = (bool)(TEX_CTRL_MIP_MAPPED_MASK & ta_fifo32[3]);

    hdr->tex_vq_compression = (bool)(TEX_CTRL_VQ_MASK & ta_fifo32[3]);

    hdr->tex_addr = ((ta_fifo32[3] & TEX_CTRL_TEX_ADDR_MASK) >>
                     TEX_CTRL_TEX_ADDR_SHIFT) << 3;
    hdr->tex_filter = (ta_fifo32[2] & TSP_TEX_INST_FILTER_MASK) >>
        TSP_TEX_INST_FILTER_SHIFT;

    hdr->src_blend_factor =
        (ta_fifo32[2] & TSP_WORD_SRC_ALPHA_FACTOR_MASK) >>
        TSP_WORD_SRC_ALPHA_FACTOR_SHIFT;
    hdr->dst_blend_factor =
        (ta_fifo32[2] & TSP_WORD_DST_ALPHA_FACTOR_MASK) >>
        TSP_WORD_DST_ALPHA_FACTOR_SHIFT;

    if (ta_fifo32[2] & (2 << TSP_TEX_CLAMP_SHIFT))
        hdr->tex_wrap_mode[0] = TEX_WRAP_CLAMP;
    else if (ta_fifo32[2] & (2 << TSP_TEX_FLIP_SHIFT))
        hdr->tex_wrap_mode[0] = TEX_WRAP_FLIP;
    else
        hdr->tex_wrap_mode[0] = TEX_WRAP_REPEAT;

    if (ta_fifo32[2] & (1 << TSP_TEX_CLAMP_SHIFT))
        hdr->tex_wrap_mode[1] = TEX_WRAP_CLAMP;
    else if (ta_fifo32[2] & (1 << TSP_TEX_FLIP_SHIFT))
        hdr->tex_wrap_mode[1] = TEX_WRAP_FLIP;
    else
        hdr->tex_wrap_mode[1] = TEX_WRAP_REPEAT;

    hdr->enable_depth_writes =
        !((ta_fifo32[1] & DEPTH_WRITE_DISABLE_MASK) >>
          DEPTH_WRITE_DISABLE_SHIFT);
    hdr->depth_func =
        (ta_fifo32[0] & DEPTH_FUNC_MASK) >> DEPTH_FUNC_SHIFT;

    hdr->shadow = (bool)(ta_fifo32[0] & TA_CMD_SHADOW_MASK);

    if (((ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT) ==
        TA_CMD_TYPE_SPRITE_HDR) {
        hdr->tex_coord_16_bit_enable = true; // force this on
    }

    // unpack the sprite color
    if (tp == PVR2_HDR_QUAD) {
        uint32_t base_color = ta_fifo32[4];
        uint32_t offset_color = ta_fifo32[5];
        unsigned base_r = (base_color & 0x00ff0000) >> 16;
        unsigned base_g = (base_color & 0x0000ff00) >> 8;
        unsigned base_b = base_color & 0x000000ff;
        unsigned base_a = (base_color & 0xff000000) >> 24;
        unsigned offset_r = (offset_color & 0x00ff0000) >> 16;
        unsigned offset_g = (offset_color & 0x0000ff00) >> 8;
        unsigned offset_b = offset_color & 0x000000ff;
        unsigned offset_a = (offset_color & 0xff000000) >> 24;

        hdr->sprite_base_color_rgba[0] = base_r / 255.0f;
        hdr->sprite_base_color_rgba[1] = base_g / 255.0f;
        hdr->sprite_base_color_rgba[2] = base_b / 255.0f;
        hdr->sprite_base_color_rgba[3] = base_a / 255.0f;

        if (hdr->offset_color_enable) {
            hdr->sprite_offs_color_rgba[0] = offset_r / 255.0f;
            hdr->sprite_offs_color_rgba[1] = offset_g / 255.0f;
            hdr->sprite_offs_color_rgba[2] = offset_b / 255.0f;
            hdr->sprite_offs_color_rgba[3] = offset_a / 255.0f;
        } else {
            memset(hdr->sprite_offs_color_rgba, 0,
                   sizeof(hdr->sprite_offs_color_rgba));
        }

        memcpy(ta->fifo_state.sprite_base_color_rgba, hdr->sprite_base_color_rgba,
               sizeof(ta->fifo_state.sprite_base_color_rgba));
        memcpy(ta->fifo_state.sprite_offs_color_rgba, hdr->sprite_offs_color_rgba,
               sizeof(ta->fifo_state.sprite_offs_color_rgba));
    }

    if (hdr->ta_color_fmt == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        if (hdr->offset_color_enable) {
            memcpy(hdr->poly_base_color_rgba, ta_fifo32 + 9, 3 * sizeof(float));
            memcpy(hdr->poly_base_color_rgba + 3, ta_fifo32 + 8, sizeof(float));
            memcpy(hdr->poly_offs_color_rgba, ta_fifo32 + 13, 3 * sizeof(float));
            memcpy(hdr->poly_offs_color_rgba + 3, ta_fifo32 + 12, sizeof(float));
        } else {
            memcpy(hdr->poly_base_color_rgba, ta_fifo32 + 5, 3 * sizeof(float));
            memcpy(hdr->poly_base_color_rgba + 3, ta_fifo32 + 4, sizeof(float));
            memset(hdr->poly_offs_color_rgba, 0, sizeof(float) * 4);
        }

        memcpy(ta->fifo_state.poly_base_color_rgba, hdr->poly_base_color_rgba,
               sizeof(ta->fifo_state.poly_base_color_rgba));
        memcpy(ta->fifo_state.poly_offs_color_rgba, hdr->poly_offs_color_rgba,
               sizeof(ta->fifo_state.poly_base_color_rgba));
    }

    return 0;
}

static int decode_input_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    pkt->tp = PVR2_PKT_INPUT_LIST;
    return 0;
}

static void unpack_rgba_8888(uint32_t const *ta_fifo32, float *rgba, uint32_t input) {
    float alpha = (float)((ta_fifo32[6] & 0xff000000) >> 24) / 255.0f;
    float red = (float)((ta_fifo32[6] & 0x00ff0000) >> 16) / 255.0f;
    float green = (float)((ta_fifo32[6] & 0x0000ff00) >> 8) / 255.0f;
    float blue = (float)((ta_fifo32[6] & 0x000000ff) >> 0) / 255.0f;

    rgba[0] = red;
    rgba[1] = green;
    rgba[2] = blue;
    rgba[3] = alpha;
}

static void
pvr2_op_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_op_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE);
}

static void
pvr2_op_mod_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_op_mod_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE);
}

static void
pvr2_trans_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_trans_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE);
}

static void
pvr2_trans_mod_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_trans_mod_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE);
}

static void
pvr2_pt_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_pt_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE);
}

void pvr2_ta_reinit(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;

    int poly_type;
    for (poly_type = 0; poly_type < PVR2_POLY_TYPE_COUNT; poly_type++)
        set_poly_type_state(ta, poly_type, PVR2_POLY_TYPE_STATE_NOT_OPENED);
    ta->fifo_state.open_group = false;
    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;

    pvr2_display_list_key key = pvr2->reg_backing[PVR2_TA_VERTBUF_POS];
    struct pvr2_display_list *cur_list = NULL;
    unsigned disp_list_idx;

    PVR2_TRACE("PVR2 TA initializing new list for key %08X\n", (unsigned)key);

    // first see if there are any display lists with a matching key
    for (disp_list_idx = 0; disp_list_idx < PVR2_MAX_FRAMES_IN_FLIGHT;
         disp_list_idx++)
        if (core->disp_lists[disp_list_idx].key == key &&
            core->disp_lists[disp_list_idx].valid) {
            cur_list = core->disp_lists + disp_list_idx;
            break;
        }

    /*
     * next see if any display lists are invalid.  Else, take the
     * least-recently used list.
     */
    unsigned oldest_age = UINT_MAX;
    if (!cur_list) {
        for (disp_list_idx = 0; disp_list_idx < PVR2_MAX_FRAMES_IN_FLIGHT;
             disp_list_idx++) {
            if (!core->disp_lists[disp_list_idx].valid) {
                cur_list = core->disp_lists + disp_list_idx;
                break;
            } else if (core->disp_lists[disp_list_idx].age_counter <= oldest_age) {
                oldest_age = core->disp_lists[disp_list_idx].age_counter;
                cur_list = core->disp_lists + disp_list_idx;
            }
        }
    }

    // initialize the display list
    pvr2_display_list_init(cur_list);
    cur_list->valid = true;
    cur_list->key = key;

    pvr2_inc_age_counter(pvr2);
    cur_list->age_counter = core->disp_list_counter;

    ta->cur_list_idx = cur_list - core->disp_lists;
}

static void next_poly_group(struct pvr2 *pvr2, enum pvr2_poly_type poly_type) {
    struct pvr2_ta *ta = &pvr2->ta;
    PVR2_TRACE("%s(%s)\n", __func__, pvr2_poly_type_name(poly_type));

    if (poly_type < 0) {
        LOG_WARN("%s - no polygon groups are open\n", __func__);
        return;
    }

    if (ta->fifo_state.open_group)
        finish_poly_group(pvr2, poly_type);
    ta->fifo_state.open_group = true;
}

static void finish_poly_group(struct pvr2 *pvr2, enum pvr2_poly_type poly_type) {
    struct pvr2_ta *ta = &pvr2->ta;
    PVR2_TRACE("%s(%s)\n", __func__, pvr2_poly_type_name(poly_type));
    ta->fifo_state.open_group = false;
}

static void ta_fifo_finish_packet(struct pvr2_ta *ta) {
    ta->fifo_state.ta_fifo_word_count = 0;
}

void pvr2_ta_list_continue(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;

    if (ta->fifo_state.cur_poly_type == PVR2_POLY_TYPE_NONE) {
        /*
         * TODO: quite a lot of games will submit a list continuation
         * immediately after closing a list.  Is the continuation only
         * supposed to be used immediately after closing a list?
         */
        LOG_ERROR("continuing when nothing is open?\n");
        return;
    }
    PVR2_TRACE("TAFIFO list continuation requested for %s\n",
               pvr2_poly_type_name(ta->fifo_state.cur_poly_type));

    if (get_poly_type_state(ta, ta->fifo_state.cur_poly_type) !=
        PVR2_POLY_TYPE_STATE_IN_PROGRESS) {
        error_set_feature("requesting continuation of a polygon "
                          "type which is not open");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    set_poly_type_state(ta, ta->fifo_state.cur_poly_type,
                        PVR2_POLY_TYPE_STATE_CONTINUATION);
    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;
    ta->fifo_state.open_group = false;
}

struct memory_interface pvr2_ta_fifo_intf = {
    .readdouble = pvr2_ta_fifo_poly_read_double,
    .readfloat = pvr2_ta_fifo_poly_read_float,
    .read32 = pvr2_ta_fifo_poly_read_32,
    .read16 = pvr2_ta_fifo_poly_read_16,
    .read8 = pvr2_ta_fifo_poly_read_8,

    .writedouble = pvr2_ta_fifo_poly_write_double,
    .writefloat = pvr2_ta_fifo_poly_write_float,
    .write32 = pvr2_ta_fifo_poly_write_32,
    .write16 = pvr2_ta_fifo_poly_write_16,
    .write8 = pvr2_ta_fifo_poly_write_8
};

unsigned pvr2_ta_fifo_rem_bytes(void) {
    // hardcode this to 256 bytes.  The TFREM register over in sys_block.c
    // calls this
    return 256;
}
