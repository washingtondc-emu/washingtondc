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
#include "gfx/gfx_il.h"
#include "pvr2.h"
#include "pvr2_reg.h"

#include "pvr2_ta.h"

#define PVR2_TRACE(msg, ...)                                            \
    do {                                                                \
        LOG_DBG("PVR2: ");                                              \
        LOG_DBG(msg, ##__VA_ARGS__);                                    \
    } while (0)

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

static DEF_ERROR_INT_ATTR(src_blend_factor);
static DEF_ERROR_INT_ATTR(dst_blend_factor);
static DEF_ERROR_INT_ATTR(display_list_index);
static DEF_ERROR_INT_ATTR(geo_buf_group_index);
static DEF_ERROR_INT_ATTR(ta_fifo_cmd)
static DEF_ERROR_INT_ATTR(pvr2_global_param)
static DEF_ERROR_INT_ATTR(ta_fifo_byte_count)
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

char const *display_list_names[DISPLAY_LIST_COUNT] = {
    "Opaque",
    "Opaque Modifier Volume",
    "Transparent",
    "Transparent Modifier Volume",
    "Punch-through Polygon",
    "Unknown Display list 5",
    "Unknown Display list 6",
    "Unknown Display list 7"
};

static void input_poly_fifo(struct pvr2 *pvr2, uint8_t byte);

// this function gets called every time a full packet is received by the TA
static int decode_packet(struct pvr2 *pvr2, struct pvr2_pkt *pkt);

static void handle_packet(struct pvr2 *pvr2, struct pvr2_pkt const *pkt);

static void render_frame_init(struct pvr2 *pvr2);

static void
finish_poly_group(struct pvr2 *pvr2, enum display_list_type disp_list);
static void
next_poly_group(struct pvr2 *pvr2, enum display_list_type disp_list);

static int decode_poly_hdr(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_end_of_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_vtx(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_input_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt);
static int decode_user_clip(struct pvr2 *pvr2, struct pvr2_pkt *pkt);

// call this whenever a packet has been processed
static void ta_fifo_finish_packet(struct pvr2_ta *ta);

static void unpack_uv16(float *u_coord, float *v_coord, void const *input);
static void unpack_rgba_8888(uint32_t const *ta_fifo32, float *rgba, uint32_t input);

/*
 * the delay between when the STARTRENDER command is received and when the
 * RENDER_COMPLETE interrupt gets raised.
 *
 * TODO: This value has no basis in reality.  I need to run some tests on real
 * hardware and come up with a good heuristic.
 *
 * If this value is too low, it will trigger race conditions in certain games
 * which can cause them to miss interrupts.
 */
#define PVR2_RENDER_COMPLETE_INT_DELAY (SCHED_FREQUENCY / 1024)

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event);

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

#define PVR2_TA_VERT_BUF_LEN (1024 * 1024)

#define PVR2_GFX_IL_INST_BUF_LEN (1024 * 256)

void pvr2_ta_init(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;

    ta->pvr2_render_complete_int_event.handler =
        pvr2_render_complete_int_event_handler;
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

    ta->pvr2_render_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_op_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_op_mod_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_trans_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_trans_mod_complete_int_event.arg_ptr = pvr2;
    ta->pvr2_pt_complete_int_event.arg_ptr = pvr2;

    pvr2->ta.pvr2_ta_vert_buf = (float*)malloc(PVR2_TA_VERT_BUF_LEN *
                                               sizeof(float) * GFX_VERT_LEN);
    if (!pvr2->ta.pvr2_ta_vert_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    ta->gfx_il_inst_buf = (struct gfx_il_inst_chain*)malloc(PVR2_GFX_IL_INST_BUF_LEN *
                                                        sizeof(struct gfx_il_inst_chain));
    if (!ta->gfx_il_inst_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    pvr2->ta.pvr2_ta_vert_buf_count = 0;
    pvr2->ta.pvr2_ta_vert_cur_group = 0;

    render_frame_init(pvr2);
}

void pvr2_ta_cleanup(struct pvr2 *pvr2) {
    free(pvr2->ta.gfx_il_inst_buf);
    free(pvr2->ta.pvr2_ta_vert_buf);
    pvr2->ta.pvr2_ta_vert_buf = NULL;
    pvr2->ta.pvr2_ta_vert_buf_count = 0;
    pvr2->ta.pvr2_ta_vert_cur_group = 0;
}

static inline void pvr2_ta_push_vert(struct pvr2 *pvr2, struct pvr2_ta_vert vert) {
    struct pvr2_ta *ta = &pvr2->ta;
    if (ta->pvr2_ta_vert_buf_count >= PVR2_TA_VERT_BUF_LEN) {
        LOG_WARN("PVR2 TA vertex buffer overflow\n");
        return;
    }

    float *outp = ta->pvr2_ta_vert_buf +
        GFX_VERT_LEN * ta->pvr2_ta_vert_buf_count++;
    PVR2_TRACE("vert_buf_count is now %u\n", ta->pvr2_ta_vert_buf_count);
    outp[GFX_VERT_POS_OFFSET + 0] = vert.pos[0];
    outp[GFX_VERT_POS_OFFSET + 1] = vert.pos[1];
    outp[GFX_VERT_POS_OFFSET + 2] = vert.pos[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 0] = vert.base_color[0];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 1] = vert.base_color[1];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 2] = vert.base_color[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 3] = vert.base_color[3];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 0] = vert.offs_color[0];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 1] = vert.offs_color[1];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 2] = vert.offs_color[2];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 3] = vert.offs_color[3];
    outp[GFX_VERT_TEX_COORD_OFFSET + 0] = vert.tex_coord[0];
    outp[GFX_VERT_TEX_COORD_OFFSET + 1] = vert.tex_coord[1];
}

static inline void
pvr2_ta_push_gfx_il(struct pvr2 *pvr2, struct gfx_il_inst inst) {
    struct pvr2_ta *ta = &pvr2->ta;

    if (ta->gfx_il_inst_buf_count >= PVR2_GFX_IL_INST_BUF_LEN)
        RAISE_ERROR(ERROR_OVERFLOW);

    struct gfx_il_inst_chain *ent =
        ta->gfx_il_inst_buf + ta->gfx_il_inst_buf_count++;
    ent->next = NULL;
    ent->cmd = inst;
    if (!ta->disp_list_begin[ta->cur_list])
        ta->disp_list_begin[ta->cur_list] = ent;
    if (ta->disp_list_end[ta->cur_list])
        ta->disp_list_end[ta->cur_list]->next = ent;
    ta->disp_list_end[ta->cur_list] = ent;
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

    uint8_t bytes[4] = {
        val & 0xff,
        (val >> 8) & 0xff,
        (val >> 16) & 0xff,
        (val >> 24) & 0xff
    };
    input_poly_fifo(pvr2, bytes[0]);
    input_poly_fifo(pvr2, bytes[1]);
    input_poly_fifo(pvr2, bytes[2]);
    input_poly_fifo(pvr2, bytes[3]);
}

uint16_t pvr2_ta_fifo_poly_read_16(addr32_t addr, void *ctxt) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: trying to read 2 bytes from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: writing 2 bytes to TA polygon FIFO: 0x%04x\n",
            (unsigned)val);
#endif
    uint8_t bytes[2] = {
        val & 0xff,
        (val >> 8) & 0xff,
    };
    input_poly_fifo(pvr2, bytes[0]);
    input_poly_fifo(pvr2, bytes[1]);
}


uint8_t pvr2_ta_fifo_poly_read_8(addr32_t addr, void *ctxt) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: trying to read 1 byte from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: writing 1 byte to TA polygon FIFO: 0x%02x\n",
            (unsigned)val);
#endif
    input_poly_fifo(pvr2, val);
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
    PVR2_TRACE("\tpolygon list: %s\n", display_list_names[hdr->list]);
    HDR_BOOL(hdr, tex_enable);
    HDR_HEX(hdr, tex_addr);
    PVR2_TRACE("\ttexture dimensions: %ux%u\n", 1 << hdr->tex_width_shift, 1 << hdr->tex_height_shift);
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

    ta->strip_len = 0;

#ifdef PVR2_LOG_VERBOSE
    dump_pkt_hdr(hdr);
#endif

    if (hdr->two_volumes_mode)
        LOG_WARN("Unimplemented two-volumes mode polygon!\n");

    if (ta->cur_list != hdr->list) {
        if (ta->cur_list == DISPLAY_LIST_NONE) {
            PVR2_TRACE("Opening display list \"%s\"\n", display_list_names[hdr->list]);
            ta->cur_list = hdr->list;
            ta->open_group = true;
        } else {
            PVR2_TRACE("software did not close list %d\n",
                      (int)ta->cur_list);
            PVR2_TRACE("Beginning polygon group within list \"%s\"\n",
                       display_list_names[ta->cur_list]);

            next_poly_group(pvr2, ta->cur_list);
        }

        if (ta->list_submitted[ta->cur_list]) {
            LOG_ERROR("Already submitted list %d\n", (int)hdr->list);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        /* pvr2_ta_vert_cur_group = pvr2_ta_vert_buf_count; */
    } else {
        PVR2_TRACE("Beginning polygon group within list \"%s\"\n",
                   display_list_names[hdr->list]);

        next_poly_group(pvr2, ta->cur_list);
    }

    ta->strip_len = 0;

    /*
     * XXX this happens before the texture caching code because we need to be
     * able to disable textures if the cache is full, but hdr is const.
     */
    memcpy(&ta->hdr, hdr, sizeof(ta->hdr));

    if (hdr->tex_enable) {
        PVR2_TRACE("texture enabled\n");

        PVR2_TRACE("the texture format is %d\n", hdr->pix_fmt);
        PVR2_TRACE("The texture address ix 0x%08x\n", hdr->tex_addr);

        if (hdr->tex_twiddle)
            PVR2_TRACE("not twiddled\n");
        else
            PVR2_TRACE("twiddled\n");

        struct pvr2_tex *ent =
            pvr2_tex_cache_find(pvr2, hdr->tex_addr, hdr->tex_palette_start,
                                hdr->tex_width_shift,
                                hdr->tex_height_shift,
                                hdr->pix_fmt, hdr->tex_twiddle,
                                hdr->tex_vq_compression,
                                hdr->tex_mipmap,
                                hdr->stride_sel);

        PVR2_TRACE("texture dimensions are (%u, %u)\n",
               1 << hdr->tex_width_shift,
               1 << hdr->tex_height_shift);
        if (ent) {
            PVR2_TRACE("Texture 0x%08x found in cache\n",
                    hdr->tex_addr);
        } else {
            PVR2_TRACE("Adding 0x%08x to texture cache...\n",
                    hdr->tex_addr);
            ent = pvr2_tex_cache_add(pvr2,
                                     hdr->tex_addr, hdr->tex_palette_start,
                                     hdr->tex_width_shift,
                                     hdr->tex_height_shift,
                                     hdr->pix_fmt,
                                     hdr->tex_twiddle,
                                     hdr->tex_vq_compression,
                                     hdr->tex_mipmap,
                                     hdr->stride_sel);
        }

        if (!ent) {
            LOG_WARN("WARNING: failed to add texture 0x%08x to "
                    "the texture cache\n", hdr->tex_addr);
            ta->hdr.tex_enable = false;
        } else {
            ta->tex_idx = pvr2_tex_cache_get_idx(pvr2, ent);
        }
    } else {
        PVR2_TRACE("textures are NOT enabled\n");
        /* poly_state.tex_enable = false; */
    }
}

static void
on_pkt_end_of_list_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    PVR2_TRACE("END-OF-LIST PACKET!\n");

    if (ta->cur_list == DISPLAY_LIST_NONE) {
        LOG_WARN("attempt to close list when no list is open!\n");
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

    switch (ta->cur_list) {
    case DISPLAY_LIST_OPAQUE:
        if (!ta->pvr2_op_complete_int_event_scheduled) {
            ta->pvr2_op_complete_int_event_scheduled = true;
            ta->pvr2_op_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_op_complete_int_event);
        }
        break;
    case DISPLAY_LIST_OPAQUE_MOD:
        if (!ta->pvr2_op_mod_complete_int_event_scheduled) {
            ta->pvr2_op_mod_complete_int_event_scheduled = true;
            ta->pvr2_op_mod_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_op_mod_complete_int_event);
        }
        break;
    case DISPLAY_LIST_TRANS:
        if (!ta->pvr2_trans_complete_int_event_scheduled) {
            ta->pvr2_trans_complete_int_event_scheduled = true;
            ta->pvr2_trans_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_trans_complete_int_event);
        }
        break;
    case DISPLAY_LIST_TRANS_MOD:
        if (!ta->pvr2_trans_mod_complete_int_event_scheduled) {
            ta->pvr2_trans_mod_complete_int_event_scheduled = true;
            ta->pvr2_trans_mod_complete_int_event.when = int_when;
            sched_event(clk, &ta->pvr2_trans_mod_complete_int_event);
        }
        break;
    case DISPLAY_LIST_PUNCH_THROUGH:
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

    finish_poly_group(pvr2, ta->cur_list);

    ta->list_submitted[ta->cur_list] = true;
    ta->cur_list = DISPLAY_LIST_NONE;
}

static void
on_quad_received(struct pvr2 *pvr2, struct pvr2_pkt_vtx const *vtx) {
    struct pvr2_ta *ta = &pvr2->ta;
    float const *ta_fifo_float = (float const*)ta->ta_fifo;

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
        (norm[0] * norm[0] + norm[1] * norm[1] + norm[2] * norm[2] == 0.0f)) {
        ta_fifo_finish_packet(ta);
        return;
    }

    // hyperplane translation
    float dist = -norm[0] * p1[0] - norm[1] * p1[1] - norm[2] * p1[2];

    p4[2] = -1.0f * (dist + norm[0] * p4[0] + norm[1] * p4[1]) / norm[2];

    float const base_col[] = {
        ta->sprite_base_color_rgba[0],
        ta->sprite_base_color_rgba[1],
        ta->sprite_base_color_rgba[2],
        ta->sprite_base_color_rgba[3]
    };

    float const offs_col[] = {
        ta->sprite_offs_color_rgba[0],
        ta->sprite_offs_color_rgba[1],
        ta->sprite_offs_color_rgba[2],
        ta->sprite_offs_color_rgba[3]
    };

    struct pvr2_ta_vert vert1 = {
        .pos = { p1[0], p1[1], p1[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[0][0], uv[0][1] }
    };

    struct pvr2_ta_vert vert2 = {
        .pos = { p2[0], p2[1], p2[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[1][0], uv[1][1] }
    };

    struct pvr2_ta_vert vert3 = {
        .pos = { p3[0], p3[1], p3[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[2][0], uv[2][1] }
    };

    struct pvr2_ta_vert vert4 = {
        .pos = { p4[0], p4[1], p4[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[3][0], uv[3][1] }
    };

    pvr2_ta_push_vert(pvr2, vert1);
    pvr2_ta_push_vert(pvr2, vert2);
    pvr2_ta_push_vert(pvr2, vert3);

    pvr2_ta_push_vert(pvr2, vert1);
    pvr2_ta_push_vert(pvr2, vert3);
    pvr2_ta_push_vert(pvr2, vert4);

    if (p1[2] < ta->clip_min)
        ta->clip_min = p1[2];
    if (p1[2] > ta->clip_max)
        ta->clip_max = p1[2];

    if (p2[2] < ta->clip_min)
        ta->clip_min = p2[2];
    if (p2[2] > ta->clip_max)
        ta->clip_max = p2[2];

    if (p3[2] < ta->clip_min)
        ta->clip_min = p3[2];
    if (p3[2] > ta->clip_max)
        ta->clip_max = p3[2];

    if (p4[2] < ta->clip_min)
        ta->clip_min = p4[2];
    if (p4[2] > ta->clip_max)
        ta->clip_max = p4[2];
}

static void
on_pkt_vtx_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    struct pvr2_pkt_vtx const *vtx = &pkt->dat.vtx;

    if (ta->hdr.tp == PVR2_HDR_QUAD) {
        on_quad_received(pvr2, vtx);
        return;
    }

    ta->open_group = true;

    /*
     * un-strip triangle strips by duplicating the previous two vertices.
     *
     * TODO: obviously it would be best to preserve the triangle strips and
     * send them to OpenGL via GL_TRIANGLE_STRIP in the rendering backend, but
     * then I need to come up with some way to signal the renderer to stop and
     * re-start strips.  It might also be possible to stitch separate strips
     * together with degenerate triangles...
     */
    if (ta->strip_len >= 3) {
        pvr2_ta_push_vert(pvr2, ta->strip_vert_1);
        pvr2_ta_push_vert(pvr2, ta->strip_vert_2);
    }

    // first update the clipping planes
    /*
     * TODO: there are FPU instructions on x86 that can do this without
     * branching
     */
    float z_recip = 1.0 / vtx->pos[2];
    if (z_recip < ta->clip_min)
        ta->clip_min = z_recip;
    if (z_recip > ta->clip_max)
        ta->clip_max = z_recip;

    struct pvr2_ta_vert vert;

    vert.pos[0] = vtx->pos[0];
    vert.pos[1] = vtx->pos[1];
    vert.pos[2] = z_recip;

    PVR2_TRACE("(%f, %f, %f)\n", vert.pos[0], vert.pos[1], vert.pos[2]);

    memcpy(vert.tex_coord, vtx->uv, sizeof(vert.tex_coord));
    memcpy(vert.base_color, vtx->base_color, sizeof(vtx->base_color));
    memcpy(vert.offs_color, vtx->offs_color, sizeof(vtx->offs_color));

    pvr2_ta_push_vert(pvr2, vert);

    if (vtx->end_of_strip) {
        /*
         * TODO: handle degenerate cases where the user sends an
         * end-of-strip on the first or second vertex
         */
        ta->strip_len = 0;
    } else {
        /*
         * shift the new vert into strip_vert2 and
         * shift strip_vert2 into strip_vert1
         */
        ta->strip_vert_1 = ta->strip_vert_2;
        ta->strip_vert_2 = vert;
        ta->strip_len++;
    }
}

static void
on_pkt_input_list_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    LOG_WARN("PVR2: unimplemented type 2 (input list) packet received\n");
}

static void on_pkt_user_clip_received(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    LOG_WARN("PVR2: unimplemented type 1 (user tile clip) packet received\n");
    PVR2_TRACE("\tmin: (%u, %u)\n\tax: (%u, %u)\n",
               pkt->dat.user_clip.xmin * 32,
               pkt->dat.user_clip.ymin * 32,
               pkt->dat.user_clip.xmax * 32,
               pkt->dat.user_clip.ymax * 32);
}

static void handle_packet(struct pvr2 *pvr2, struct pvr2_pkt const *pkt) {
    switch (pkt->tp) {
    case PVR2_PKT_HDR:
        PVR2_TRACE("header packet received\n");
        on_pkt_hdr_received(pvr2, pkt);
        break;
    case PVR2_PKT_END_OF_LIST:
        PVR2_TRACE("end-of-list packet received\n");
        on_pkt_end_of_list_received(pvr2, pkt);
        break;
    case PVR2_PKT_VTX:
        PVR2_TRACE("vertex packet received\n");
        on_pkt_vtx_received(pvr2, pkt);
        break;
    case PVR2_PKT_INPUT_LIST:
        PVR2_TRACE("input list packet received\n");
        on_pkt_input_list_received(pvr2, pkt);
        break;
    case PVR2_PKT_USER_CLIP:
        PVR2_TRACE("user clip packet received\n");
        on_pkt_user_clip_received(pvr2, pkt);
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void input_poly_fifo(struct pvr2 *pvr2, uint8_t byte) {
    struct pvr2_ta *ta = &pvr2->ta;
    ta->ta_fifo[ta->ta_fifo_byte_count++] = byte;

    if (!(ta->ta_fifo_byte_count % 32)) {
        struct pvr2_pkt pkt;
        if (decode_packet(pvr2, &pkt) == 0) {
            handle_packet(pvr2, &pkt);
            ta_fifo_finish_packet(ta);
        }
    }
}

static void dump_fifo(struct pvr2 *pvr2) {
#ifdef ENABLE_LOG_DEBUG
    unsigned idx;
    uint32_t const *ta_fifo32 = (uint32_t const*)pvr2->ta.ta_fifo;
    LOG_DBG("Dumping FIFO: %u bytes\n", pvr2->ta.ta_fifo_byte_count);
    for (idx = 0; idx < pvr2->ta.ta_fifo_byte_count / 4; idx++)
        LOG_DBG("\t0x%08x\n", (unsigned)ta_fifo32[idx]);
#endif
}

static int decode_packet(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    uint32_t const *ta_fifo32 = (uint32_t const*)pvr2->ta.ta_fifo;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch(cmd_tp) {
    case TA_CMD_TYPE_POLY_HDR:
    case TA_CMD_TYPE_SPRITE_HDR:
        return decode_poly_hdr(pvr2, pkt);
    case TA_CMD_TYPE_END_OF_LIST:
        return decode_end_of_list(pvr2, pkt);
    case TA_CMD_TYPE_VERTEX:
        return decode_vtx(pvr2, pkt);
    case TA_CMD_TYPE_INPUT_LIST:
        return decode_input_list(pvr2, pkt);
    case TA_CMD_TYPE_USER_CLIP:
        return decode_user_clip(pvr2, pkt);
    default:
        LOG_ERROR("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
        dump_fifo(pvr2);
        error_set_feature("PVR2 command type");
        error_set_ta_fifo_cmd(cmd_tp);
        /* error_set_display_list_index(poly_state.current_list); */
        error_set_ta_fifo_byte_count(pvr2->ta.ta_fifo_byte_count);
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

static int decode_end_of_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    pkt->tp = PVR2_PKT_END_OF_LIST;
    return 0;
}

static int decode_vtx(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    uint32_t const *ta_fifo32 = (uint32_t const*)ta->ta_fifo;

    if (ta->ta_fifo_byte_count < ta->hdr.vtx_len)
        return -1;
    else if (ta->ta_fifo_byte_count > ta->hdr.vtx_len) {
        LOG_ERROR("byte count is %u, vtx_len is %u\n",
                  ta->ta_fifo_byte_count, ta->hdr.vtx_len);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pkt->tp = PVR2_PKT_VTX;
    struct pvr2_pkt_vtx *vtx = &pkt->dat.vtx;

    vtx->end_of_strip = (bool)(ta_fifo32[0] & TA_CMD_END_OF_STRIP_MASK);

    memcpy(vtx->pos, ta_fifo32 + 1, 3 * sizeof(float));

    if (ta->hdr.tex_enable) {
        if (ta->hdr.tex_coord_16_bit_enable)
            unpack_uv16(vtx->uv, vtx->uv + 1, ta_fifo32 + 4);
        else
            memcpy(vtx->uv, ta_fifo32 + 4, 2 * sizeof(float));
    }

    if (ta->hdr.two_volumes_mode) {
        switch (ta->hdr.ta_color_fmt) {
        case TA_COLOR_TYPE_PACKED:
            if (ta->hdr.tex_enable)
                unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[6]);
            else
                unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[4]);
            if (ta->hdr.offset_color_enable && ta->hdr.tex_enable) {
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
                if (ta->hdr.tex_enable) {
                    memcpy(&base_intensity, ta_fifo32 + 6, sizeof(float));
                    memcpy(&offs_intensity, ta_fifo32 + 7, sizeof(float));
                } else {
                    memcpy(&base_intensity, ta_fifo32 + 4, sizeof(float));
                    memcpy(&offs_intensity, ta_fifo32 + 5, sizeof(float));
                }
                vtx->base_color[0] =
                    base_intensity * ta->poly_base_color_rgba[0];
                vtx->base_color[1] =
                    base_intensity * ta->poly_base_color_rgba[1];
                vtx->base_color[2] =
                    base_intensity * ta->poly_base_color_rgba[2];
                vtx->base_color[3] = ta->poly_base_color_rgba[3];
                if (ta->hdr.offset_color_enable) {
                    vtx->offs_color[0] =
                        offs_intensity * ta->poly_offs_color_rgba[0];
                    vtx->offs_color[1] =
                        offs_intensity * ta->poly_offs_color_rgba[1];
                    vtx->offs_color[2] =
                        offs_intensity * ta->poly_offs_color_rgba[2];
                    vtx->offs_color[3] = ta->poly_offs_color_rgba[3];
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
        switch (ta->hdr.ta_color_fmt) {
        case TA_COLOR_TYPE_PACKED:
            unpack_rgba_8888(ta_fifo32, vtx->base_color, ta_fifo32[6]);
            if (ta->hdr.offset_color_enable) {
                unpack_rgba_8888(ta_fifo32, vtx->offs_color, ta_fifo32[7]);
            } else {
                vtx->offs_color[0] = 0.0f;
                vtx->offs_color[1] = 0.0f;
                vtx->offs_color[2] = 0.0f;
                vtx->offs_color[3] = 0.0f;
            }
            break;
        case TA_COLOR_TYPE_FLOAT:
            if (ta->hdr.tex_enable) {
                memcpy(vtx->base_color + 3, ta_fifo32 + 8, sizeof(float));
                memcpy(vtx->base_color + 0, ta_fifo32 + 9, sizeof(float));
                memcpy(vtx->base_color + 1, ta_fifo32 + 10, sizeof(float));
                memcpy(vtx->base_color + 2, ta_fifo32 + 11, sizeof(float));
                if (ta->hdr.offset_color_enable) {
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
                    base_intensity * ta->poly_base_color_rgba[0];
                vtx->base_color[1] =
                    base_intensity * ta->poly_base_color_rgba[1];
                vtx->base_color[2] =
                    base_intensity * ta->poly_base_color_rgba[2];
                vtx->base_color[3] = ta->poly_base_color_rgba[3];
                if (ta->hdr.offset_color_enable) {
                    vtx->offs_color[0] =
                        offs_intensity * ta->poly_offs_color_rgba[0];
                    vtx->offs_color[1] =
                        offs_intensity * ta->poly_offs_color_rgba[1];
                    vtx->offs_color[2] =
                        offs_intensity * ta->poly_offs_color_rgba[2];
                    vtx->offs_color[3] = ta->poly_offs_color_rgba[3];
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
    uint32_t const *ta_fifo32 = (uint32_t const*)pvr2->ta.ta_fifo;
    struct pvr2_pkt_user_clip *user_clip = &pkt->dat.user_clip;

    pkt->tp = PVR2_PKT_USER_CLIP;
    user_clip->xmin = ta_fifo32[4];
    user_clip->ymin = ta_fifo32[5];
    user_clip->xmax = ta_fifo32[6];
    user_clip->ymax = ta_fifo32[7];

    return 0;
}

static int decode_poly_hdr(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    struct pvr2_ta *ta = &pvr2->ta;
    uint32_t const *ta_fifo32 = (uint32_t const*)ta->ta_fifo;
    struct pvr2_pkt_hdr *hdr = &pkt->dat.hdr;

    unsigned param_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;
    enum pvr2_hdr_tp tp;

    unsigned hdr_len = 32;
    unsigned vtx_len = 32;

    // we need these to figure out whether the header is 32 bytes or 64 bytes.
    bool two_volumes_mode = (bool)(ta_fifo32[0] & TA_CMD_TWO_VOLUMES_MASK);
    enum ta_color_type col_tp =
        (enum ta_color_type)((ta_fifo32[0] & TA_CMD_COLOR_TYPE_MASK) >>
                             TA_CMD_COLOR_TYPE_SHIFT);
    bool tex_enable = (bool)(ta_fifo32[0] & TA_CMD_TEX_ENABLE_MASK);
    bool offset_color_enable = (bool)(ta_fifo32[0] & TA_CMD_OFFSET_COLOR_MASK);
    enum display_list_type disp_list =
        (enum display_list_type)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                                 TA_CMD_DISP_LIST_SHIFT);

    switch (param_tp) {
    case TA_CMD_TYPE_POLY_HDR:
        tp = PVR2_HDR_TRIANGLE_STRIP;
        if (disp_list == DISPLAY_LIST_OPAQUE_MOD ||
            disp_list == DISPLAY_LIST_TRANS_MOD) {
            hdr_len = 32;
            vtx_len = 64;
        } else {
            if (col_tp == TA_COLOR_TYPE_INTENSITY_MODE_1 &&
                (two_volumes_mode || (tex_enable && offset_color_enable))) {
                hdr_len = 64;
            }

            if (tex_enable) {
                if ((!two_volumes_mode && col_tp == TA_COLOR_TYPE_FLOAT) ||
                    (two_volumes_mode && col_tp != TA_COLOR_TYPE_FLOAT))
                    vtx_len = 64;
            }
        }

        break;
    case TA_CMD_TYPE_SPRITE_HDR:
        tp = PVR2_HDR_QUAD;
        vtx_len = 64;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (ta->ta_fifo_byte_count < hdr_len)
        return -1;
    else if (ta->ta_fifo_byte_count > hdr_len)
        RAISE_ERROR(ERROR_INTEGRITY);

    pkt->tp = PVR2_PKT_HDR;
    hdr->tp = tp;
    hdr->vtx_len = vtx_len;

    hdr->list = disp_list;

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

    hdr->tex_vq_compression = (bool)(TEX_CTRL_VQ_MASK & ta_fifo32[3]);
    hdr->tex_mipmap = (bool)(TEX_CTRL_MIP_MAPPED_MASK & ta_fifo32[3]);
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
        !((ta_fifo32[0] & DEPTH_WRITE_DISABLE_MASK) >>
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

        memcpy(ta->sprite_base_color_rgba, hdr->sprite_base_color_rgba,
               sizeof(ta->sprite_base_color_rgba));
        memcpy(ta->sprite_offs_color_rgba, hdr->sprite_offs_color_rgba,
               sizeof(ta->sprite_offs_color_rgba));
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

        memcpy(ta->poly_base_color_rgba, hdr->poly_base_color_rgba,
               sizeof(ta->poly_base_color_rgba));
        memcpy(ta->poly_offs_color_rgba, hdr->poly_offs_color_rgba,
               sizeof(ta->poly_base_color_rgba));
    }

    return 0;
}

static int decode_input_list(struct pvr2 *pvr2, struct pvr2_pkt *pkt) {
    pkt->tp = PVR2_PKT_INPUT_LIST;
    return 0;
}

// unpack 16-bit texture coordinates into two floats
static void unpack_uv16(float *u_coord, float *v_coord, void const *input) {
    uint32_t val = *(uint32_t*)input;
    uint32_t u_val = val & 0xffff0000;
    uint32_t v_val = val << 16;

    memcpy(u_coord, &u_val, sizeof(*u_coord));
    memcpy(v_coord, &v_val, sizeof(*v_coord));
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

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_ta *ta = &((struct pvr2*)event->arg_ptr)->ta;
    ta->pvr2_render_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}

void pvr2_ta_startrender(struct pvr2 *pvr2) {
    PVR2_TRACE("STARTRENDER requested!\n");
    struct pvr2_ta *ta = &pvr2->ta;
    struct gfx_il_inst cmd;

    unsigned tile_w = get_glob_tile_clip_x(pvr2) << 5;
    unsigned tile_h = get_glob_tile_clip_y(pvr2) << 5;
    unsigned x_clip_min = get_fb_x_clip_min(pvr2);
    unsigned x_clip_max = get_fb_x_clip_max(pvr2);
    unsigned y_clip_min = get_fb_y_clip_min(pvr2);
    unsigned y_clip_max = get_fb_y_clip_max(pvr2);

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
    uint32_t backgnd_tag = get_isp_backgnd_t(pvr2);
    addr32_t backgnd_info_addr = (backgnd_tag & ISP_BACKGND_T_ADDR_MASK) >>
        ISP_BACKGND_T_ADDR_SHIFT;
    uint32_t backgnd_skip = ((ISP_BACKGND_T_SKIP_MASK & backgnd_tag) >>
                             ISP_BACKGND_T_SKIP_SHIFT) + 3;
    uint32_t const *backgnd_info = (uint32_t*)(pvr2->mem.tex32 + backgnd_info_addr);

    /* printf("background skip is %d\n", (int)backgnd_skip); */

    /* printf("ISP_BACKGND_D is %f\n", (double)frak); */
    /* printf("ISP_BACKGND_T is 0x%08x\n", (unsigned)backgnd_tag); */

    uint32_t bg_color_src = *(backgnd_info + 3 + 0 * backgnd_skip + 3);

    float bg_color_a = (float)((bg_color_src & 0xff000000) >> 24) / 255.0f;
    float bg_color_r = (float)((bg_color_src & 0x00ff0000) >> 16) / 255.0f;
    float bg_color_g = (float)((bg_color_src & 0x0000ff00) >> 8) / 255.0f;
    float bg_color_b = (float)((bg_color_src & 0x000000ff) >> 0) / 255.0f;
    ta->pvr2_bgcolor[0] = bg_color_r;
    ta->pvr2_bgcolor[1] = bg_color_g;
    ta->pvr2_bgcolor[2] = bg_color_b;
    ta->pvr2_bgcolor[3] = bg_color_a;

    /* uint32_t backgnd_depth_as_int = get_isp_backgnd_d(); */
    /* memcpy(&geo->bgdepth, &backgnd_depth_as_int, sizeof(float)); */

    pvr2_tex_cache_xmit(pvr2);

    if (ta->cur_list != DISPLAY_LIST_NONE)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    /* finish_poly_group(poly_state.current_list); */

    int tgt = framebuffer_set_render_target(pvr2);

    /*
     * This is really driving me insane and I don't know what to do about it.
     *
     * A number of games will use different resolutions when reading from the
     * framebuffer than they will when writing to it.  Most of the time it's
     * just a couple of extra rows which isn't that big of a deal, but in
     * SoulCalibur's case there's also an extra column.  Lobbing pixels off of
     * a texture isn't so easy to do in OpenGL, so IDK what to do.
     */
    unsigned read_width, read_height;
    framebuffer_get_render_target_dims(pvr2, tgt, &read_width, &read_height);
    if (read_width != width || read_height != height) {
        /*
         * Also I suspect that the read-width needs to be doubled because it's
         * always half what I expect it to be.  That's fairly reasonably and
         * not nearly as exasperating as the case described above.
         */
        LOG_WARN("Warning: read-dimensions of framebuffer are %ux%u, but "
                 "write-dimensions are %ux%u\n",
                 read_width, read_height, width, height);
    }

    /*
     * TODO: This is extremely inaccurate.  PVR2 only draws on a per-tile
     * basis; I think that includes clearing the framebuffer on a per-tile
     * basis as well.
     *
     * This is definitely the reason why the spinning graphics during Namco
     * Museum's bootup disappear when they come to their final position (because
     * the game issues a STARTRENDER without intending to draw anything).  It
     * might also be related to the bad background color on the first frame of
     * Dreamcast bootup (see issue #10 on github) and also maybe related to some
     * of the incorrect screen transitions (like when the silver background at
     * the end of IP.BIN is supposed to fade to black but instead it instantly
     * turns black).
     */

    // set up rendering context
    cmd.op = GFX_IL_BEGIN_REND;
    cmd.arg.begin_rend.screen_width = width;
    cmd.arg.begin_rend.screen_height = height;
    cmd.arg.begin_rend.rend_tgt_obj = tgt;
    rend_exec_il(&cmd, 1);

    cmd.op = GFX_IL_SET_CLIP_RANGE;
    cmd.arg.set_clip_range.clip_min = ta->clip_min;
    cmd.arg.set_clip_range.clip_max = ta->clip_max;
    rend_exec_il(&cmd, 1);

    // initial rendering settings
    cmd.op = GFX_IL_CLEAR;
    cmd.arg.clear.bgcolor[0] = ta->pvr2_bgcolor[0];
    cmd.arg.clear.bgcolor[1] = ta->pvr2_bgcolor[1];
    cmd.arg.clear.bgcolor[2] = ta->pvr2_bgcolor[2];
    cmd.arg.clear.bgcolor[3] = ta->pvr2_bgcolor[3];
    rend_exec_il(&cmd, 1);

    // execute queued gfx_il commands
    enum display_list_type list;
    for (list = DISPLAY_LIST_FIRST; list <= DISPLAY_LIST_LAST; list++) {
        bool sort_mode = false;
        if (list == DISPLAY_LIST_TRANS) {
            /*
             * order-independent transparency is enabled when bit 0 of
             * ISP_FEED_CFG is 0.
             */
            if (!(pvr2->reg_backing[PVR2_ISP_FEED_CFG] & 1)) {
                sort_mode = true;
                cmd.op = GFX_IL_BEGIN_DEPTH_SORT;
                rend_exec_il(&cmd, 1);
            }
        }
        struct gfx_il_inst_chain *chain = ta->disp_list_begin[list];
        while (chain) {
            rend_exec_il(&chain->cmd, 1);
            chain = chain->next;
        }
        if (sort_mode) {
            cmd.op = GFX_IL_END_DEPTH_SORT;
            rend_exec_il(&cmd, 1);
        }
    }

    // tear down rendering context
    cmd.op = GFX_IL_END_REND;
    cmd.arg.end_rend.rend_tgt_obj = tgt;
    rend_exec_il(&cmd, 1);

    ta->next_frame_stamp++;
    render_frame_init(pvr2);

    if (!ta->pvr2_render_complete_int_event_scheduled) {
        struct dc_clock *clk = pvr2->clk;
        ta->pvr2_render_complete_int_event_scheduled = true;
        ta->pvr2_render_complete_int_event.when = clock_cycle_stamp(clk) +
            PVR2_RENDER_COMPLETE_INT_DELAY;
        sched_event(clk, &ta->pvr2_render_complete_int_event);
    }
}

void pvr2_ta_reinit(struct pvr2 *pvr2) {
    memset(pvr2->ta.list_submitted, 0, sizeof(pvr2->ta.list_submitted));
}

static void next_poly_group(struct pvr2 *pvr2, enum display_list_type disp_list) {
    struct pvr2_ta *ta = &pvr2->ta;
    PVR2_TRACE("%s(%s)\n", __func__, display_list_names[disp_list]);

    if (disp_list < 0) {
        LOG_WARN("%s - no lists are open\n", __func__);
        return;
    }

    if (ta->open_group)
        finish_poly_group(pvr2, disp_list);
    ta->open_group = true;

    ta->pvr2_ta_vert_cur_group = ta->pvr2_ta_vert_buf_count;
}

static void finish_poly_group(struct pvr2 *pvr2, enum display_list_type disp_list) {
    struct pvr2_ta *ta = &pvr2->ta;
    PVR2_TRACE("%s(%s)\n", __func__, display_list_names[disp_list]);
    struct gfx_il_inst cmd;

    if (disp_list < 0) {
        PVR2_TRACE("%s - no lists are open\n", __func__);
        return;
    }

    // filter out modifier volumes from being rendered
    if (disp_list == DISPLAY_LIST_OPAQUE_MOD ||
        disp_list == DISPLAY_LIST_TRANS_MOD)
        return;

    if (!ta->open_group) {
        LOG_WARN("%s - still waiting for a polygon header to be opened!\n",
               __func__);
        return;
    }

    cmd.op = GFX_IL_SET_REND_PARAM;
    if (ta->hdr.tex_enable) {
        PVR2_TRACE("tex_enable should be true\n");
        cmd.arg.set_rend_param.param.tex_enable = true;
        cmd.arg.set_rend_param.param.tex_idx = ta->tex_idx;
    } else {
        PVR2_TRACE("tex_enable should be false\n");
        cmd.arg.set_rend_param.param.tex_enable = false;
    }

    cmd.arg.set_rend_param.param.src_blend_factor = ta->hdr.src_blend_factor;
    cmd.arg.set_rend_param.param.dst_blend_factor = ta->hdr.dst_blend_factor;
    cmd.arg.set_rend_param.param.tex_wrap_mode[0] = ta->hdr.tex_wrap_mode[0];
    cmd.arg.set_rend_param.param.tex_wrap_mode[1] = ta->hdr.tex_wrap_mode[1];

    cmd.arg.set_rend_param.param.enable_depth_writes =
        ta->hdr.enable_depth_writes;
    cmd.arg.set_rend_param.param.depth_func = ta->hdr.depth_func;

    cmd.arg.set_rend_param.param.tex_inst = ta->hdr.tex_inst;
    cmd.arg.set_rend_param.param.tex_filter = ta->hdr.tex_filter;

    // enqueue the configuration command
    pvr2_ta_push_gfx_il(pvr2, cmd);

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
    if (((unsigned)cmd.arg.set_rend_param.param.src_blend_factor >= PVR2_BLEND_FACTOR_COUNT) ||
        ((unsigned)cmd.arg.set_rend_param.param.dst_blend_factor >= PVR2_BLEND_FACTOR_COUNT)) {
        error_set_src_blend_factor(cmd.arg.set_rend_param.param.src_blend_factor);
        error_set_dst_blend_factor(cmd.arg.set_rend_param.param.dst_blend_factor);
        error_set_display_list_index((unsigned)disp_list);
        /* error_set_geo_buf_group_index(group - list->groups); */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    // TODO: this only needs to be done once per list, not once per polygon group
    cmd.op = GFX_IL_SET_BLEND_ENABLE;
    cmd.arg.set_blend_enable.do_enable = (disp_list == DISPLAY_LIST_TRANS);
    pvr2_ta_push_gfx_il(pvr2, cmd);

    unsigned n_verts = ta->pvr2_ta_vert_buf_count - ta->pvr2_ta_vert_cur_group;
    pvr2->stat.poly_count[disp_list] += n_verts / 3;

    cmd.op = GFX_IL_DRAW_ARRAY;
    cmd.arg.draw_array.n_verts = n_verts;
    cmd.arg.draw_array.verts =
        ta->pvr2_ta_vert_buf + ta->pvr2_ta_vert_cur_group * GFX_VERT_LEN;
    pvr2_ta_push_gfx_il(pvr2, cmd);

    ta->pvr2_ta_vert_cur_group = ta->pvr2_ta_vert_buf_count;

    ta->open_group = false;
}

static void ta_fifo_finish_packet(struct pvr2_ta *ta) {
    ta->ta_fifo_byte_count = 0;
}

static void render_frame_init(struct pvr2 *pvr2) {
    struct pvr2_ta *ta = &pvr2->ta;

    // free vertex arrays
    ta->pvr2_ta_vert_buf_count = 0;
    ta->pvr2_ta_vert_cur_group = 0;

    ta->open_group = false;

    // free up gfx_il commands
    ta->gfx_il_inst_buf_count = 0;
    enum display_list_type list;
    for (list = DISPLAY_LIST_FIRST; list < DISPLAY_LIST_COUNT; list++) {
        ta->disp_list_begin[list] = NULL;
        ta->disp_list_end[list] = NULL;
    }

    ta->clip_min = -1.0f;
    ta->clip_max = 1.0f;

    memset(ta->list_submitted, 0, sizeof(ta->list_submitted));
    ta->cur_list = DISPLAY_LIST_NONE;

    memset(&pvr2->stat, 0, sizeof(pvr2->stat));
}

unsigned get_cur_frame_stamp(struct pvr2 *pvr2) {
    return pvr2->ta.next_frame_stamp;
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
