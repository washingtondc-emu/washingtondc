/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020, 2022 snickerbockers
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

/*
 * TODO: This is a terribly inaccurate, HLE-like implementation of the PowerVR2
 * that doesn't correclty emulate the interactions between TA and ISP and it
 * doesn't really even do tile rendering like it should.
 */

#include <math.h>
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

#define TA_CMD_END_OF_STRIP_SHIFT 28
#define TA_CMD_END_OF_STRIP_MASK (1 << TA_CMD_END_OF_STRIP_SHIFT)

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

static void close_tri_strip(struct pvr2 *pvr2);

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
    if (addr >= 0x11000000 && addr <= 0x11ffffe0) {
        /*
         * workaround for annoying bullshit in Sonic Adventure
         *
         * During E-102 Gamma's boss fight against E-101 Beta, the game will
         * read from 0x1129411a, clear bit 15 and then write that value back.
         * It will then read from 0x1129411a again, clear bit 0 and then write
         * that value back.  It only does this a couple times.
         *
         * The address written to is one that corresponds to texture DMA
         * transfers.  this is the only time I have ever seen a program write
         * to this address range directly instead of writing to it via DMA
         * transfer.
         *
         * Possible explanations are:
         *     * a bug in WashingtonDC causes the address the game accesses to
         *       be incorrect
         *     * there is a legitimate bug in the game
         *     * reading from and writing to this address has some sort of
         *       low-level effect on the TA FIFO (such as forcing it to finish
         *       processing any unprocessed data?)
         *
         * see issue #92 on github.
         *
         * The function where this happens is at PC=0x8c091b8a.
         */
        LOG_ERROR("%s - WRITE %04X to %08X (DIRECT TEXTURE)\n",
                  __func__, (unsigned)val, (unsigned)addr);
    } else {
        error_set_value(val);
        error_set_address(addr);
        error_set_feature("trying to write a 16-bit value to the PVR2 TA FIFO");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
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

static char const *
pvr2_depth_func_name(int func) {
    switch (func) {
    case PVR2_DEPTH_NEVER:
        return "NEVER";
    case PVR2_DEPTH_LESS:
        return "LESS";
    case PVR2_DEPTH_EQUAL:
        return "EQUAL";
    case PVR2_DEPTH_LEQUAL:
        return "LEQUAL";
    case PVR2_DEPTH_GREATER:
        return "GREATER";
    case PVR2_DEPTH_NOTEQUAL:
        return "NOTEQUAL";
    case PVR2_DEPTH_GEQUAL:
        return "GEQUAL";
    case PVR2_DEPTH_ALWAYS:
        return "ALWAYS";
    default:
        return "ERROR/UNKNOWN";
    }
}

static void dump_pkt_hdr(struct pvr2_pkt_hdr const *hdr) {
#define HDR_BOOL(hdr, mem) PVR2_TRACE("\t"#mem": %s\n", hdr->mem ? "true" : "false")
#define HDR_BOOL_FUNC(hdr, mem, func) PVR2_TRACE("\t"#mem": %s\n", func(hdr) ? "true" : "false")
#define HDR_INT(hdr, mem) PVR2_TRACE("\t"#mem": %d\n", (int)hdr->mem)
#define HDR_INT_FUNC(hdr, mem, func) PVR2_TRACE("\t"#mem": %d\n", (int)func(hdr))
#define HDR_HEX(hdr, mem) PVR2_TRACE("\t"#mem": 0x%08x\n", (int)hdr->mem)
#define HDR_HEX_FUNC(hdr, mem, func) PVR2_TRACE("\t"#mem": 0x%08x\n", (int)func(hdr))
    PVR2_TRACE("packet header:\n");
    PVR2_TRACE("\ttype: %s\n", hdr->tp == PVR2_HDR_TRIANGLE_STRIP ?
               "triangle strip" : "quadrilateral");
    HDR_INT(hdr, vtx_len);
    PVR2_TRACE("\tpolygon type: %s\n",
               pvr2_poly_type_name(pvr2_hdr_poly_type(hdr)));
    HDR_BOOL_FUNC(hdr, tex_enable, pvr2_hdr_tex_enable);
    HDR_HEX_FUNC(hdr, tex_addr, pvr2_hdr_tex_addr);
    PVR2_TRACE("\ttexture dimensions: %ux%u\n",
               1 << pvr2_hdr_tex_width_shift(hdr),
               1 << pvr2_hdr_tex_height_shift(hdr));
    HDR_BOOL_FUNC(hdr, tex_twiddle, pvr2_hdr_tex_twiddle);
    HDR_BOOL_FUNC(hdr, stride_sel, pvr2_hdr_stride_sel);
    HDR_BOOL_FUNC(hdr, tex_vq_compression, pvr2_hdr_vq_compression);
    HDR_BOOL_FUNC(hdr, tex_mipmap, pvr2_hdr_tex_mipmap);
    HDR_INT_FUNC(hdr, pix_fmt, pvr2_hdr_pix_fmt);
    HDR_INT_FUNC(hdr, tex_inst, pvr2_hdr_tex_inst);
    HDR_INT_FUNC(hdr, tex_filter, pvr2_hdr_tex_filter);
    HDR_INT_FUNC(hdr, tex_wrap_mode[0], pvr2_hdr_tex_wrap_mode_s);
    HDR_INT_FUNC(hdr, tex_wrap_mode[1], pvr2_hdr_tex_wrap_mode_t);
    HDR_INT_FUNC(hdr, ta_color_fmt, pvr2_hdr_color_fmt);
    HDR_INT_FUNC(hdr, src_blend_factor, pvr2_hdr_src_blend_factor);
    HDR_INT_FUNC(hdr, dst_blend_factor, pvr2_hdr_dst_blend_factor);
    HDR_BOOL_FUNC(hdr, enable_depth_writes, pvr2_hdr_enable_depth_writes);
    PVR2_TRACE("\tdepth_func: %s\n", pvr2_depth_func_name(pvr2_hdr_depth_func(hdr)));
    HDR_BOOL_FUNC(hdr, two_volumes_mode, pvr2_hdr_two_volumes_mode);
    HDR_BOOL_FUNC(hdr, offset_color_enable, pvr2_hdr_offset_color_enable);
    HDR_BOOL_FUNC(hdr, gourad_shading_enable, pvr2_hdr_gourad_shading);
    HDR_BOOL_FUNC(hdr, tex_coord_16_bit_enable, pvr2_hdr_tex_coord_16_bit);
}
#endif

static void on_pkt_hdr_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_pkt_hdr const *hdr = &pkt->dat.hdr;
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;

#ifdef PVR2_LOG_VERBOSE
    dump_pkt_hdr(hdr);
#endif

    if (pvr2_hdr_two_volumes_mode(hdr))
        LOG_DBG("Unimplemented two-volumes mode polygon!\n");

    enum pvr2_poly_type poly_type = pvr2_hdr_poly_type(hdr);
    if (ta->fifo_state.cur_poly_type != poly_type) {
        if (get_poly_type_state(ta, poly_type) ==
            PVR2_POLY_TYPE_STATE_SUBMITTED) {
            /*
             * TODO: I want to make this an ERROR_UNIMPLEMENTED, but enough
             * games do it that I have to accept that somehow it works out on
             * real hardware.
             */
            LOG_ERROR("PVR2: re-opening polython type %s after it was already "
                      "submitted?\n", pvr2_poly_type_name(poly_type));
        }

        if (ta->fifo_state.cur_poly_type == PVR2_POLY_TYPE_NONE) {
            PVR2_TRACE("Opening polygon group \"%s\"\n",
                       pvr2_poly_type_name(poly_type));
            set_poly_type_state(ta, poly_type,
                                PVR2_POLY_TYPE_STATE_IN_PROGRESS);
            ta->fifo_state.cur_poly_type = poly_type;
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
                   pvr2_poly_type_name(poly_type));

        next_poly_group(pvr2, ta->fifo_state.cur_poly_type);
    }

    /*
     * XXX this happens before the texture caching code because we need to be
     * able to disable textures if the cache is full, but hdr is const.
     */
    ta->fifo_state.vtx_len = hdr->vtx_len;
    ta->fifo_state.tex_enable = pvr2_hdr_tex_enable(hdr);
    ta->fifo_state.geo_tp = hdr->tp;
    ta->fifo_state.tex_coord_16_bit_enable = pvr2_hdr_tex_coord_16_bit(hdr);
    ta->fifo_state.two_volumes_mode = pvr2_hdr_two_volumes_mode(hdr);
    ta->fifo_state.ta_color_fmt = pvr2_hdr_color_fmt(hdr);
    ta->fifo_state.offset_color_enable = pvr2_hdr_offset_color_enable(hdr);
    ta->fifo_state.src_blend_factor = pvr2_hdr_src_blend_factor(hdr);
    ta->fifo_state.dst_blend_factor = pvr2_hdr_dst_blend_factor(hdr);
    ta->fifo_state.tex_wrap_mode[0] = pvr2_hdr_tex_wrap_mode_s(hdr);
    ta->fifo_state.tex_wrap_mode[1] = pvr2_hdr_tex_wrap_mode_t(hdr);
    ta->fifo_state.enable_depth_writes = pvr2_hdr_enable_depth_writes(hdr);
    ta->fifo_state.depth_func = pvr2_hdr_depth_func(hdr);
    ta->fifo_state.tex_inst = pvr2_hdr_tex_inst(hdr);
    ta->fifo_state.tex_filter = pvr2_hdr_tex_filter(hdr);

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

    cmd_hdr->tex_width_shift = pvr2_hdr_tex_width_shift(hdr);
    cmd_hdr->tex_height_shift = pvr2_hdr_tex_height_shift(hdr);
    cmd_hdr->stride_sel = pvr2_hdr_stride_sel(hdr);
    cmd_hdr->tex_twiddle = pvr2_hdr_tex_twiddle(hdr);
    cmd_hdr->pix_fmt = pvr2_hdr_pix_fmt(hdr);
    cmd_hdr->tex_addr = pvr2_hdr_tex_addr(hdr);
    cmd_hdr->tex_palette_start = pvr2_hdr_tex_palette_start(hdr);
    cmd_hdr->tex_vq_compression = pvr2_hdr_vq_compression(hdr);
    cmd_hdr->tex_mipmap = pvr2_hdr_tex_mipmap(hdr);
    cmd_hdr->user_clip_mode = pvr2_hdr_user_clip_mode(hdr);
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

    finish_poly_group(pvr2, ta->fifo_state.cur_poly_type);
    set_poly_type_state(ta, ta->fifo_state.cur_poly_type,
                        PVR2_POLY_TYPE_STATE_SUBMITTED);
    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;

    // queue up in a display list
    struct pvr2_display_list *cur_list = core->disp_lists + ta->cur_list_idx;
    if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT || !cur_list->valid)
        RAISE_ERROR(ERROR_INTEGRITY);

    ta->fifo_state.cur_poly_type = PVR2_POLY_TYPE_NONE;
}

static float *
alloc_disp_list_verts(struct pvr2_display_list *listp, unsigned n_verts) {
    if (listp->n_verts + n_verts > PVR2_DISPLAY_LIST_MAX_VERTS) {
        LOG_ERROR("PVR2 CORE display list vertex buffer overflow\n");
        return NULL;
    }

    float *outp = listp->vert_array + GFX_VERT_LEN * listp->n_verts;
    listp->n_verts += n_verts;
    return outp;
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
    float *verts_out = alloc_disp_list_verts(cur_list, 4);

    if (!verts_out)
        return;

    close_tri_strip(pvr2);
    struct pvr2_display_list_command *cmd =
        pvr2_list_alloc_new_cmd(cur_list, ta->fifo_state.cur_poly_type);
    if (!cmd) {
        LOG_ERROR("%s unable to allocate display list entry!\n", __func__);
        return;
    }
    cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_QUAD;
    cmd->quad.first_vtx = cur_list->n_verts - 4;

    float *vp[4] = {
        verts_out,
        verts_out + GFX_VERT_LEN,
        verts_out + GFX_VERT_LEN * 2,
        verts_out + GFX_VERT_LEN * 3
    };
    vp[0][GFX_VERT_POS_OFFSET + 0] = quad->vert_pos[1][0];
    vp[0][GFX_VERT_POS_OFFSET + 1] = quad->vert_pos[1][1];
    vp[0][GFX_VERT_POS_OFFSET + 2] = quad->vert_pos[1][2];
    vp[0][GFX_VERT_POS_OFFSET + 3] = 1.0f;
    vp[1][GFX_VERT_POS_OFFSET + 0] = quad->vert_pos[0][0];
    vp[1][GFX_VERT_POS_OFFSET + 1] = quad->vert_pos[0][1];
    vp[1][GFX_VERT_POS_OFFSET + 2] = quad->vert_pos[0][2];
    vp[1][GFX_VERT_POS_OFFSET + 3] = 1.0f;
    vp[2][GFX_VERT_POS_OFFSET + 0] = quad->vert_pos[2][0];
    vp[2][GFX_VERT_POS_OFFSET + 1] = quad->vert_pos[2][1];
    vp[2][GFX_VERT_POS_OFFSET + 2] = quad->vert_pos[2][2];
    vp[2][GFX_VERT_POS_OFFSET + 3] = 1.0f;
    vp[3][GFX_VERT_POS_OFFSET + 0] = quad->vert_pos[3][0];
    vp[3][GFX_VERT_POS_OFFSET + 1] = quad->vert_pos[3][1];
    vp[3][GFX_VERT_POS_OFFSET + 2] = quad->vert_pos[3][2];
    vp[3][GFX_VERT_POS_OFFSET + 3] = 1.0f;

    memcpy(vp[0] + GFX_VERT_BASE_COLOR_OFFSET,
           ta->fifo_state.sprite_base_color_rgba, 4 * sizeof(float));
    memcpy(vp[1] + GFX_VERT_BASE_COLOR_OFFSET,
           ta->fifo_state.sprite_base_color_rgba, 4 * sizeof(float));
    memcpy(vp[2] + GFX_VERT_BASE_COLOR_OFFSET,
           ta->fifo_state.sprite_base_color_rgba, 4 * sizeof(float));
    memcpy(vp[3] + GFX_VERT_BASE_COLOR_OFFSET,
           ta->fifo_state.sprite_base_color_rgba, 4 * sizeof(float));

    memcpy(vp[0] + GFX_VERT_OFFS_COLOR_OFFSET,
           ta->fifo_state.sprite_offs_color_rgba, 4 * sizeof(float));
    memcpy(vp[1] + GFX_VERT_OFFS_COLOR_OFFSET,
           ta->fifo_state.sprite_offs_color_rgba, 4 * sizeof(float));
    memcpy(vp[2] + GFX_VERT_OFFS_COLOR_OFFSET,
           ta->fifo_state.sprite_offs_color_rgba, 4 * sizeof(float));
    memcpy(vp[3] + GFX_VERT_OFFS_COLOR_OFFSET,
           ta->fifo_state.sprite_offs_color_rgba, 4 * sizeof(float));

    /*
     * unpack the texture coordinates.  The third vertex's coordinate is the
     * scond vertex's coordinate plus the two side-vectors.  We do this
     * unconditionally even if textures are disabled.  If textures are disabled
     * then the output of this texture-coordinate algorithm is undefined but it
     * does not matter because the rendering code won't be using it anyways.
     */
    unpack_uv16(vp[1] + GFX_VERT_TEX_COORD_OFFSET,
                vp[1] + GFX_VERT_TEX_COORD_OFFSET + 1,
                quad->tex_coords_packed);
    unpack_uv16(vp[0] + GFX_VERT_TEX_COORD_OFFSET,
                vp[0] + GFX_VERT_TEX_COORD_OFFSET + 1,
                quad->tex_coords_packed + 1);
    unpack_uv16(vp[2] + GFX_VERT_TEX_COORD_OFFSET,
                vp[2] + GFX_VERT_TEX_COORD_OFFSET + 1,
                quad->tex_coords_packed + 2);
    float uv_vec[2][2] = {
        { vp[1][GFX_VERT_TEX_COORD_OFFSET] - vp[0][GFX_VERT_TEX_COORD_OFFSET],
          vp[1][GFX_VERT_TEX_COORD_OFFSET + 1] - vp[0][GFX_VERT_TEX_COORD_OFFSET + 1] },
        { vp[2][GFX_VERT_TEX_COORD_OFFSET] - vp[0][GFX_VERT_TEX_COORD_OFFSET],
          vp[2][GFX_VERT_TEX_COORD_OFFSET + 1] - vp[0][GFX_VERT_TEX_COORD_OFFSET + 1] }
    };
    vp[3][GFX_VERT_TEX_COORD_OFFSET] =
        vp[0][GFX_VERT_TEX_COORD_OFFSET] + uv_vec[0][0] + uv_vec[1][0];
    vp[3][GFX_VERT_TEX_COORD_OFFSET + 1] =
        vp[0][GFX_VERT_TEX_COORD_OFFSET + 1] + uv_vec[0][1] + uv_vec[1][1];

    // update display list depth clipping
    if (!isinf(quad->vert_pos[0][2]) &&
        !isnan(quad->vert_pos[0][2]) &&
        fabsf(quad->vert_pos[0][2]) < 1024 * 1024) {
        if (quad->vert_pos[0][2] < cur_list->clip_min)
            cur_list->clip_min = quad->vert_pos[0][2];
        if (quad->vert_pos[0][2] > cur_list->clip_max)
            cur_list->clip_max = quad->vert_pos[0][2];
    }
    if (!isinf(quad->vert_pos[1][2]) &&
        !isnan(quad->vert_pos[1][2]) &&
        fabsf(quad->vert_pos[1][2]) < 1024 * 1024) {
        if (quad->vert_pos[1][2] < cur_list->clip_min)
            cur_list->clip_min = quad->vert_pos[1][2];
        if (quad->vert_pos[1][2] > cur_list->clip_max)
            cur_list->clip_max = quad->vert_pos[1][2];
    }
    if (!isinf(quad->vert_pos[2][2]) &&
        !isnan(quad->vert_pos[2][2]) &&
        fabsf(quad->vert_pos[2][2]) < 1024 * 1024) {
        if (quad->vert_pos[2][2] < cur_list->clip_min)
            cur_list->clip_min = quad->vert_pos[2][2];
        if (quad->vert_pos[2][2] > cur_list->clip_max)
            cur_list->clip_max = quad->vert_pos[2][2];
    }
    if (!isinf(quad->vert_pos[3][2]) &&
        !isnan(quad->vert_pos[3][2]) &&
        fabsf(quad->vert_pos[3][2]) < 1024 * 1024) {
        if (quad->vert_pos[3][2] < cur_list->clip_min)
            cur_list->clip_min = quad->vert_pos[3][2];
        if (quad->vert_pos[3][2] > cur_list->clip_max)
            cur_list->clip_max = quad->vert_pos[3][2];
    }
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

        /*
         * update the clipping planes.
         *
         * some games will submit vertices with infinite or near-infinite 1/z
         * values.  This represents a vertex which is very close to the projection
         * surface, with an approximate distance of 0.
         *
         * This causes the linear interpolation between clip_min and clip_max to
         * push everything else to the far-plane (1/z = clip_min), so we exclude it
         * from the clip_min and clip_max calculations.  The gfxgl implementation
         * will have enabled GL_DEPTH_CLAMP, so the polygon will still get
         * rasterized but this may in theory cause z-fighting at the near-plane.  In
         * practice I've never seen this cause any z-fighting; I think the infinite
         * 1/z polygons are all extreme outliers.
         *
         * This hack is unfortunate but it will always be necessary as long as
         * graphics APIs force us to map our depth values from an unbounded range to
         * a limited range as both OpenGL and Direct X do.  soft_gfx actually does
         * not have this problem at all since it is able to ignore clip_min and
         * clip_max and use the raw 1/z values for its depth testing.
         *
         * Note that the cutoff value of 1024*1024 below is abritrary and can be
         * changed.
         *
         * SoulCalibur and Sonic Adventure 2 both do this.
         *
         * TODO: should take the range into account as well as the absolute value.
         * eg current implementation would break if the game submitted polygons
         * with 1/z values between 1024*1024 and 1024*1024+1, but that wouldn't
         * actually be a situation with an unreasonably large depth range so we'd
         * ideally want to let that through.
         */
        float depth = vtx->pos[2];
        if (!isinf(depth) && !isnan(depth) && fabsf(depth) < 1024 * 1024) {
            if (depth < cur_list->clip_min)
                cur_list->clip_min = depth;
            if (depth > cur_list->clip_max)
                cur_list->clip_max = depth;
        }

        float *vtx_out = alloc_disp_list_verts(cur_list, 1);

        if (!vtx_out)
            return;

        memcpy(vtx_out + GFX_VERT_POS_OFFSET, vtx->pos, sizeof(float) * 3);
        vtx_out[GFX_VERT_POS_OFFSET + 3] = 1.0f;
        memcpy(vtx_out + GFX_VERT_BASE_COLOR_OFFSET, vtx->base_color, sizeof(float) * 4);
        memcpy(vtx_out + GFX_VERT_OFFS_COLOR_OFFSET, vtx->offs_color, sizeof(float) * 4);
        memcpy(vtx_out + GFX_VERT_TEX_COORD_OFFSET, vtx->uv, sizeof(float) * 2);

#ifdef PVR2_LOG_VERBOSE
        LOG_DBG("\tposition: (%f, %f, %f)\n",
                vtx_out[GFX_VERT_POS_OFFSET],
                vtx_out[GFX_VERT_POS_OFFSET + 1],
                vtx_out[GFX_VERT_POS_OFFSET + 2]);
        LOG_DBG("\tbase color: (%f, %f, %f, %f)\n",
                vtx_out[GFX_VERT_BASE_COLOR_OFFSET],
                vtx_out[GFX_VERT_BASE_COLOR_OFFSET + 1],
                vtx_out[GFX_VERT_BASE_COLOR_OFFSET + 2],
                vtx_out[GFX_VERT_BASE_COLOR_OFFSET + 3]);
        LOG_DBG("\toffset color: (%f, %f, %f, %f)\n",
                vtx_out[GFX_VERT_OFFS_COLOR_OFFSET],
                vtx_out[GFX_VERT_OFFS_COLOR_OFFSET + 1],
                vtx_out[GFX_VERT_OFFS_COLOR_OFFSET + 2],
                vtx_out[GFX_VERT_OFFS_COLOR_OFFSET + 3]);
        LOG_DBG("\ttex_coord: (%f, %f)\n",
                vtx_out[GFX_VERT_TEX_COORD_OFFSET],
                vtx_out[GFX_VERT_TEX_COORD_OFFSET + 1]);
#endif

        if (!ta->fifo_state.open_tri_strip) {
            ta->fifo_state.cur_tri_strip_start = cur_list->n_verts - 1;
            ta->fifo_state.cur_tri_strip_len = 0;
            ta->fifo_state.open_tri_strip = true;
        }
        ta->fifo_state.cur_tri_strip_len++;

        if (vtx->end_of_strip)
            close_tri_strip(pvr2);
    }
}

static void close_tri_strip(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_display_list_command *cmd = NULL;

    if (ta->fifo_state.open_tri_strip) {
        ta->fifo_state.open_tri_strip = false;
        if (ta->fifo_state.cur_poly_type >= PVR2_POLY_TYPE_FIRST &&
            ta->fifo_state.cur_poly_type <= PVR2_POLY_TYPE_LAST) {
            // queue up in a display list
            struct pvr2_display_list *cur_list =
                core->disp_lists + ta->cur_list_idx;
            if (ta->cur_list_idx >= PVR2_MAX_FRAMES_IN_FLIGHT ||
                !cur_list->valid) {
                RAISE_ERROR(ERROR_INTEGRITY);
            }
            cmd = pvr2_list_alloc_new_cmd(cur_list,
                                          ta->fifo_state.cur_poly_type);
            if (cmd) {
                cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_TRI_STRIP;
                struct pvr2_display_list_tri_strip *strip = &cmd->strip;
                strip->first_vtx = ta->fifo_state.cur_tri_strip_start;
                strip->vtx_count = ta->fifo_state.cur_tri_strip_len;
            } else {
                LOG_ERROR("%s unable to allocate display list entry!\n",
                          __func__);
            }
        }
    }
}

static void
on_pkt_input_list_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    LOG_WARN("PVR2: unimplemented type 2 (input list) packet received\n");
}

static void on_pkt_user_clip_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_core *core = &pvr2->core;

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

        cmd->tp = PVR2_DISPLAY_LIST_COMMAND_TP_USER_CLIP;
        struct pvr2_display_list_user_clip *user_clip = &cmd->user_clip;
        user_clip->x_min = pkt->dat.user_clip.xmin;
        user_clip->y_min = pkt->dat.user_clip.ymin;
        user_clip->x_max = pkt->dat.user_clip.xmax;
        user_clip->y_max = pkt->dat.user_clip.ymax;
    }
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

    p1[2] = ta_fifo_float[3];
    p2[2] = ta_fifo_float[6];
    p3[2] = ta_fifo_float[9];
    p4[2] = norm[2] / (-1.0f * (dist + norm[0] * p4[0] + norm[1] * p4[1]));

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

    memcpy(hdr->param, ta_fifo32, sizeof(hdr->param));

    unsigned param_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;
    enum pvr2_hdr_tp tp;

    struct pvr2_ta_param_dims dims = pvr2_ta_get_param_dims(ta_fifo32[0]);
    if (dims.is_vert)
        RAISE_ERROR(ERROR_INTEGRITY);
    unsigned hdr_len = dims.hdr_len;
    unsigned vtx_len = dims.vtx_len;

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

        if (pvr2_hdr_offset_color_enable(hdr)) {
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

    if (pvr2_hdr_color_fmt(hdr) == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        if (pvr2_hdr_offset_color_enable(hdr)) {
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
    close_tri_strip(pvr2);
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
