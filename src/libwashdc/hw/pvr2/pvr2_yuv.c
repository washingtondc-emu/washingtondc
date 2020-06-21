/*******************************************************************************
 *
 * Copyright 2018-2020 snickerbockers
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

#include "log.h"
#include "washdc/error.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "pvr2_reg.h"
#include "hw/sys/holly_intc.h"
#include "dc_sched.h"
#include "pvr2.h"
#include "framebuffer.h"
#include "pvr2_tex_cache.h"

#include "pvr2_yuv.h"

static void pvr2_yuv_input_byte(struct pvr2 *pvr2, unsigned byte);
static void
pvr2_yuv_complete_int_event_handler(struct SchedEvent *event);

void pvr2_yuv_init(struct pvr2 *pvr2) {
    pvr2->yuv.pvr2_yuv_complete_int_event.handler =
        pvr2_yuv_complete_int_event_handler;
    pvr2->yuv.pvr2_yuv_complete_int_event.arg_ptr = pvr2;
}

void pvr2_yuv_cleanup(struct pvr2 *pvr2) {
}

/*
 * I'm not sure what kind of a latency this should have, but it seems that it's
 * extremely fast.  Resident Evil 2 will input more FIFO data almost
 * immediately after finishing a frame which can cause problems if
 * cur_macroblock_x and cur_macroblock_y haven't been reset yet.
 */
#define PVR2_YUV_COMPLETE_INT_DELAY 0

static void
pvr2_yuv_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2 *pvr2 = (struct pvr2*)event->arg_ptr;
    pvr2->yuv.yuv_complete_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE);
    pvr2_yuv_set_base(pvr2, pvr2->reg_backing[PVR2_TA_YUV_TEX_BASE]);
}

static void pvr2_yuv_schedule_int(struct pvr2 *pvr2) {
    struct pvr2_yuv *yuv = &pvr2->yuv;
    struct dc_clock *clk = pvr2->clk;
    if (!yuv->yuv_complete_event_scheduled) {
        dc_cycle_stamp_t int_when =
            clock_cycle_stamp(clk) + PVR2_YUV_COMPLETE_INT_DELAY;
        yuv->yuv_complete_event_scheduled = true;
        yuv->pvr2_yuv_complete_int_event.when = int_when;
        sched_event(clk, &yuv->pvr2_yuv_complete_int_event);
    }
}

void pvr2_yuv_set_base(struct pvr2 *pvr2, uint32_t new_base) {
    struct pvr2_yuv *yuv = &pvr2->yuv;

#ifdef INVARIANTS
    if (new_base % 8)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    /*
     * TODO: what happens if any of these settings change without updating the
     * base address?
     */
    LOG_DBG("PVR2 YUV RESETTING BASE\n");
    yuv->dst_addr = new_base;
    yuv->macroblock_offset = 0;
    yuv->cur_macroblock_x = 0;
    yuv->cur_macroblock_y = 0;
}

void pvr2_yuv_set_tex_ctrl(struct pvr2 *pvr2, uint32_t new_tex_ctrl) {
    struct pvr2_yuv *yuv = &pvr2->yuv;

    if ((new_tex_ctrl & (1 << 16)) || (new_tex_ctrl & (1 << 24))) {
        error_set_value(new_tex_ctrl);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    yuv->fmt = PVR2_YUV_FMT_420;
    yuv->macroblock_count_x = (new_tex_ctrl & 0x3f) + 1;
    yuv->macroblock_count_y = ((new_tex_ctrl >> 8) & 0x3f) + 1;
}

void pvr2_yuv_input_data(struct pvr2 *pvr2, void const *dat, unsigned n_bytes) {
    uint32_t tex_ctrl = get_ta_yuv_tex_ctrl(pvr2);

    if (tex_ctrl & (1 << 16))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    if (tex_ctrl & (1 << 24))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if ((pvr2->yuv.dst_addr + 3) >= (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1))
        RAISE_ERROR(ERROR_INTEGRITY);

    uint8_t const *dat8 = (uint8_t const*)dat;

    while (n_bytes) {
        uint8_t dat_byte;
        memcpy(&dat_byte, dat8, sizeof(dat_byte));
        pvr2_yuv_input_byte(pvr2, dat_byte);
        dat8++;
        n_bytes--;
    }
}

static void pvr2_yuv_macroblock(struct pvr2 *pvr2) {
    struct pvr2_yuv *yuv = &pvr2->yuv;
    uint32_t block[16][8];

    if (yuv->cur_macroblock_x >= yuv->macroblock_count_x) {
        LOG_ERROR("yuv->cur_macroblock_x is %u\n", yuv->cur_macroblock_x);
        LOG_ERROR("yuv->macroblock_count_x is %u\n", yuv->macroblock_count_x);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    if (yuv->cur_macroblock_y >= yuv->macroblock_count_y) {
        // TODO: should reset to zero here
        LOG_ERROR("yuv->cur_macroblock_y is %u\n", yuv->cur_macroblock_y);
        LOG_ERROR("yuv->macroblock_count_y is %u\n", yuv->macroblock_count_y);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row, col;
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 8; col++) {
            /*
             * For the luminance component, each macro block is stored as four
             * 8x8 sub-macroblocks, each of which is contiguous.
             */
            unsigned col_lum, row_lum;
            unsigned lum_start;
            if (row < 8) {
                if (col < 4) {
                    lum_start = 0;
                    col_lum = col;
                    row_lum = row;
                } else {
                    lum_start = 0x40;
                    col_lum = col - 4;
                    row_lum = row;
                }
            } else {
                if (col < 4) {
                    lum_start = 0x80;
                    col_lum = col;
                    row_lum = row - 8;
                } else {
                    lum_start = 0xc0;
                    col_lum = col - 4;
                    row_lum = row - 8;
                }
            }

            unsigned lum[2] = {
                yuv->y_buf[row_lum * 8 + col_lum * 2 + lum_start],
                yuv->y_buf[row_lum * 8 + col_lum * 2 + lum_start + 1],
            };

            unsigned u_val = yuv->u_buf[(row / 2) * 8 + col];
            unsigned v_val = yuv->v_buf[(row / 2) * 8 + col];

            block[row][col] = (lum[0] << 8) | (lum[1] << 24) | u_val | (v_val << 16);
        }
    }

    unsigned linestride = 2 * 16 * yuv->macroblock_count_x;
    unsigned macroblock_offs = linestride * 16 * yuv->cur_macroblock_y +
        yuv->cur_macroblock_x * 8 * sizeof(uint32_t);

    uint32_t addr_base = yuv->dst_addr + macroblock_offs;

    for (row = 0; row < 16; row++) {
        pvr2_tex_mem_64bit_write_raw(pvr2, addr_base, block[row], 32);
        addr_base += linestride;
    }

    yuv->cur_macroblock_x++;
    if (yuv->cur_macroblock_x >= yuv->macroblock_count_x) {
        yuv->cur_macroblock_x = 0;
        yuv->cur_macroblock_y++;
    }

    if (yuv->cur_macroblock_y == yuv->macroblock_count_y) {
        LOG_DBG("scheduling yuv interrupt\n");
        pvr2_yuv_schedule_int(pvr2);
    }
}

static DEF_ERROR_INT_ATTR(macroblock_count_x)
static DEF_ERROR_INT_ATTR(macroblock_count_y)
static DEF_ERROR_INT_ATTR(cur_macroblock_x)
static DEF_ERROR_INT_ATTR(cur_macroblock_y)

static void pvr2_yuv_input_byte(struct pvr2 *pvr2, unsigned dat) {
    struct pvr2_yuv *yuv = &pvr2->yuv;
    if (yuv->fmt != PVR2_YUV_FMT_420)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (yuv->macroblock_offset < 64) {
        yuv->u_buf[yuv->macroblock_offset++] = dat;
    } else if (yuv->macroblock_offset < 128) {
        yuv->v_buf[yuv->macroblock_offset++ - 64] = dat;
    } else if (yuv->macroblock_offset < 384) {
        yuv->y_buf[yuv->macroblock_offset++ - 128] = dat;
    } else {
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (yuv->macroblock_offset == 384) {
        yuv->macroblock_offset = 0;

        if (yuv->cur_macroblock_x >= yuv->macroblock_count_x ||
            yuv->cur_macroblock_y >= yuv->macroblock_count_y) {
            error_set_cur_macroblock_x(yuv->cur_macroblock_x);
            error_set_cur_macroblock_y(yuv->cur_macroblock_y);
            error_set_macroblock_count_x(yuv->macroblock_count_x);
            error_set_macroblock_count_y(yuv->macroblock_count_y);
            RAISE_ERROR(ERROR_INTEGRITY);
        }

        pvr2_yuv_macroblock(pvr2);
    }
}

static uint32_t pvr2_ta_fifo_yuv_read_32(addr32_t addr, void *ctxt) {
    error_set_length(4);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint16_t pvr2_ta_fifo_yuv_read_16(addr32_t addr, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint8_t pvr2_ta_fifo_yuv_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static float pvr2_ta_fifo_yuv_read_float(addr32_t addr, void *ctxt) {
    error_set_length(4);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static double pvr2_ta_fifo_yuv_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void pvr2_ta_fifo_yuv_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_yuv_input_byte(pvr2, val & 0xff);
    pvr2_yuv_input_byte(pvr2, (val >> 8) & 0xff);
    pvr2_yuv_input_byte(pvr2, (val >> 16) & 0xff);
    pvr2_yuv_input_byte(pvr2, (val >> 24) & 0xff);
}

static void pvr2_ta_fifo_yuv_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_yuv_input_byte(pvr2, val & 0xff);
    pvr2_yuv_input_byte(pvr2, (val >> 8) & 0xff);
}

static void pvr2_ta_fifo_yuv_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_yuv_input_byte(pvr2, val);
}

static void pvr2_ta_fifo_yuv_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t val32;
    memcpy(&val32, &val, sizeof(val32));
    pvr2_ta_fifo_yuv_write_32(addr, val32, ctxt);
}

static void
pvr2_ta_fifo_yuv_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

struct memory_interface pvr2_ta_yuv_fifo_intf = {
    .readdouble = pvr2_ta_fifo_yuv_read_double,
    .readfloat = pvr2_ta_fifo_yuv_read_float,
    .read32 = pvr2_ta_fifo_yuv_read_32,
    .read16 = pvr2_ta_fifo_yuv_read_16,
    .read8 = pvr2_ta_fifo_yuv_read_8,

    .writedouble = pvr2_ta_fifo_yuv_write_double,
    .writefloat = pvr2_ta_fifo_yuv_write_float,
    .write32 = pvr2_ta_fifo_yuv_write_32,
    .write16 = pvr2_ta_fifo_yuv_write_16,
    .write8 = pvr2_ta_fifo_yuv_write_8
};
