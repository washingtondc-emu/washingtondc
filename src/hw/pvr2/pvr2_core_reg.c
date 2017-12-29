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

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "spg.h"
#include "types.h"
#include "mem_code.h"
#include "MemoryMap.h"
#include "error.h"
#include "framebuffer.h"
#include "pvr2_ta.h"
#include "pvr2_tex_cache.h"
#include "log.h"
#include "mmio.h"

#include "pvr2_core_reg.h"

static uint32_t fb_r_sof1, fb_r_sof2, fb_r_ctrl, fb_r_size,
    fb_w_sof1, fb_w_sof2, fb_w_ctrl, fb_w_linestride, isp_backgnd_t,
    isp_backgnd_d, glob_tile_clip, fb_x_clip, fb_y_clip;

static enum palette_tp palette_tp;

// 5f8128, 5f8138
static uint32_t ta_vertbuf_pos, ta_vertbuf_start;

static uint32_t ta_next_opb_init;

DEF_MMIO_REGION(pvr2_core_reg, N_PVR2_CORE_REGS, ADDR_PVR2_CORE_FIRST, uint32_t)
static uint8_t reg_backing[N_PVR2_CORE_REGS];

uint8_t pvr2_palette_ram[PVR2_PALETTE_RAM_LEN];

static uint32_t id_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);

static uint32_t revision_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);

static void ta_startrender_mmio_write(struct mmio_region_pvr2_core_reg *region,
                                      unsigned idx, uint32_t val);

static uint32_t fb_r_ctrl_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_r_ctrl_mmio_write(struct mmio_region_pvr2_core_reg *region,
                                 unsigned idx, uint32_t val);

static uint32_t fb_w_ctrl_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_w_ctrl_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_w_linestride_mmio_read(struct mmio_region_pvr2_core_reg *region,
                                          unsigned idx);
static void fb_w_linestride_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                       uint32_t val);

static uint32_t fb_r_sof1_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_r_sof1_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_r_sof2_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_r_sof2_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_r_size_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_r_size_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_w_sof1_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_w_sof1_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_w_sof2_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_w_sof2_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_x_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_x_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t fb_y_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void fb_y_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val);

static uint32_t
isp_backgnd_d_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void isp_backgnd_d_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                     uint32_t val);

static uint32_t
isp_backgnd_t_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void isp_backgnd_t_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                     uint32_t val);

static void
vo_control_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx, uint32_t val);

static uint32_t
ta_palette_tp_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void
ta_palette_tp_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                         uint32_t val);

static uint32_t
ta_vertbuf_start_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void
ta_vertbuf_start_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                            uint32_t val);

static uint32_t
ta_next_opb_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);

static uint32_t
ta_vertbuf_pos_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);

static uint32_t
ta_glob_tile_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void
ta_glob_tile_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                             uint32_t val);

static uint32_t ta_reset_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void ta_reset_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                uint32_t val);

static uint32_t
ta_next_opb_init_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void
ta_next_opb_init_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                            uint32_t val);

static uint32_t
pal_ram_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx);
static void
pal_ram_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx, uint32_t val);

void pvr2_core_reg_init(void) {
    init_mmio_region_pvr2_core_reg(&mmio_region_pvr2_core_reg, (void*)reg_backing);

    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "ID", 0x5f8000,
                   id_mmio_read, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "REVISION", 0x5f8004,
                   revision_mmio_read, mmio_region_pvr2_core_reg_readonly_write_error);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SOFTRESET", 0x5f8008,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "STARTRENDER", 0x5f8014,
                   mmio_region_pvr2_core_reg_writeonly_read_error, ta_startrender_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "PARAM_BASE", 0x5f8020,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "REGION_BASE", 0x5f802c,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPAN_SORT_CFG", 0x5f8030,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "VO_BORDER_COL", 0x5f8040,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_R_CTRL", 0x5f8044,
                   fb_r_ctrl_mmio_read, fb_r_ctrl_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_W_CTRL", 0x5f8048,
                   fb_w_ctrl_mmio_read, fb_w_ctrl_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_W_LINESTRIDE", 0x5f804c,
                   fb_w_linestride_mmio_read, fb_w_linestride_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_R_SOF1", 0x5f8050,
                   fb_r_sof1_mmio_read, fb_r_sof1_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_R_SOF2", 0x5f8054,
                   fb_r_sof2_mmio_read, fb_r_sof2_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_R_SIZE", 0x5f805c,
                   fb_r_size_mmio_read, fb_r_size_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_W_SOF1", 0x5f8060,
                   fb_w_sof1_mmio_read, fb_w_sof1_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_W_SOF2", 0x5f8064,
                   fb_w_sof2_mmio_read, fb_w_sof2_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_X_CLIP", 0x5f8068,
                   fb_x_clip_mmio_read, fb_x_clip_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_Y_CLIP", 0x5f806c,
                   fb_y_clip_mmio_read, fb_y_clip_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FPU_SHAD_SCALE", 0x5f8074,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FPU_CULL_VAL", 0x5f8078,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FPU_PARAM_CFG", 0x5f807c,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "HALF_OFFSET", 0x5f8080,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FPU_PERP_VAL", 0x5f8084,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "ISP_BACKGND_D", 0x5f8088,
                   isp_backgnd_d_mmio_read, isp_backgnd_d_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "ISP_BACKGND_T", 0x5f808c,
                   isp_backgnd_t_mmio_read, isp_backgnd_t_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "ISP_FEED_CFG", 0x5f8098,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_CLAMP_MAX", 0x5f80bc,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_CLAMP_MIN", 0x5f80c0,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TEXT_CONTROL", 0x5f80e4,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SCALER_CTL", 0x5f80f4,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FB_BURSTXTRL", 0x5f8110,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "Y_COEFF", 0x5f8118,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SDRAM_REFRESH", 0x5f80a0,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SDRAM_CFG", 0x5f80a8,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_COL_RAM", 0x5f80b0,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_COL_VERT", 0x5f80b4,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_DENSITY", 0x5f80b8,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_HBLANK_INT", 0x5f80c8,
                   spg_hblank_int_mmio_read, spg_hblank_int_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_VBLANK_INT", 0x5f80cc,
                   spg_vblank_int_mmio_read, spg_vblank_int_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_CONTROL", 0x5f80d0,
                   spg_control_mmio_read, spg_control_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_HBLANK", 0x5f80d4,
                   spg_hblank_mmio_read, spg_hblank_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_LOAD", 0x5f80d8,
                   spg_load_mmio_read, spg_load_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_VBLANK", 0x5f80dc,
                   spg_vblank_mmio_read, spg_vblank_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_WIDTH", 0x5f80e0,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "VO_CONTROL", 0x5f80e8,
                   mmio_region_pvr2_core_reg_warn_read_handler, vo_control_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "VO_STARTX", 0x5f80ec,
                   mmio_region_pvr2_core_reg_warn_read_handler, vo_control_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "VO_STARTY", 0x5f80f0,
                   mmio_region_pvr2_core_reg_warn_read_handler, vo_control_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "PALETTE_TP", 0x5f8108,
                   ta_palette_tp_mmio_read, ta_palette_tp_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "SPG_STATUS", 0x5f810c,
                   spg_status_mmio_read, mmio_region_pvr2_core_reg_readonly_write_error);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_OL_BASE", 0x5f8124,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "PT_ALPHA_CMP", 0x5f8124,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_VERTBUF_START", 0x5f8128,
                   ta_vertbuf_start_mmio_read, ta_vertbuf_start_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_ISP_LIMIT", 0x5f8130,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_NEXT_OPB", 0x5f8134,
                   ta_next_opb_mmio_read, mmio_region_pvr2_core_reg_readonly_write_error);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_VERTBUF_POS", 0x5f8138,
                   ta_vertbuf_pos_mmio_read, mmio_region_pvr2_core_reg_readonly_write_error);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_OL_LIMIT", 0x5f812c,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_GLOB_TILE_CLIP", 0x5f813c,
                   ta_glob_tile_clip_mmio_read, ta_glob_tile_clip_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_ALLOC_CTRL", 0x5f8140,
                   mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_RESET", 0x5f8144,
                   ta_reset_mmio_read, ta_reset_mmio_write);
    mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "TA_NEXT_OPB_INIT", 0x5f8164,
                   ta_next_opb_init_mmio_read, ta_next_opb_init_mmio_write);

    // The PVR2 fog table - 128 single-precision floats
    unsigned idx;
    for (idx = 0; idx < 128; idx++)
        mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "FOG_TBL", 0x5f8200 + 4 * idx,
                       mmio_region_pvr2_core_reg_warn_read_handler, mmio_region_pvr2_core_reg_warn_write_handler);

    // palette ram
    for (idx = 0; idx < 1024; idx++)
        mmio_region_pvr2_core_reg_init_cell(&mmio_region_pvr2_core_reg, "PALETTE_RAM", 0x5f9000 + 4 * idx,
                       pal_ram_mmio_read, pal_ram_mmio_write);
}

double pvr2_core_reg_read_double(addr32_t addr) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_core_reg_write_double(addr32_t addr, double val) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

float pvr2_core_reg_read_float(addr32_t addr) {
    uint32_t tmp = mmio_region_pvr2_core_reg_read(&mmio_region_pvr2_core_reg,
                                                  addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_core_reg_write_float(addr32_t addr, float val) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_pvr2_core_reg_write(&mmio_region_pvr2_core_reg, addr, val);
}

uint32_t pvr2_core_reg_read_32(addr32_t addr) {
    return mmio_region_pvr2_core_reg_read(&mmio_region_pvr2_core_reg, addr);
}

void pvr2_core_reg_write_32(addr32_t addr, uint32_t val) {
    mmio_region_pvr2_core_reg_write(&mmio_region_pvr2_core_reg, addr, val);
}

uint16_t pvr2_core_reg_read_16(addr32_t addr) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_core_reg_write_16(addr32_t addr, uint16_t val) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t pvr2_core_reg_read_8(addr32_t addr) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_core_reg_write_8(addr32_t addr, uint8_t val) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_core_reg_cleanup(void) {
    cleanup_mmio_region_pvr2_core_reg(&mmio_region_pvr2_core_reg);
}

static uint32_t id_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    /* hardcoded hardware ID */
    return 0x17fd11db;
}

static uint32_t revision_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return 17;
}

static void ta_startrender_mmio_write(struct mmio_region_pvr2_core_reg *region,
                                      unsigned idx, uint32_t val) {
    pvr2_ta_startrender();
}

static uint32_t fb_r_ctrl_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_r_ctrl;
}

static void fb_r_ctrl_mmio_write(struct mmio_region_pvr2_core_reg *region,
                                 unsigned idx, uint32_t val) {
    if (val & PVR2_VCLK_DIV_MASK)
        spg_set_pclk_div(1);
    else
        spg_set_pclk_div(2);

    spg_set_pix_double_y((bool)(val & PVR2_LINE_DOUBLE_MASK));
    fb_r_ctrl = val;
}

static uint32_t fb_w_ctrl_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_w_ctrl;
}

static void fb_w_ctrl_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    framebuffer_sync_from_host_maybe();
    fb_w_ctrl = val;
}

static uint32_t fb_w_linestride_mmio_read(struct mmio_region_pvr2_core_reg *region,
                                          unsigned idx) {
    return fb_w_linestride;
}

static void fb_w_linestride_mmio_write(struct mmio_region_pvr2_core_reg *region,
                                       unsigned idx, uint32_t val) {
    framebuffer_sync_from_host_maybe();
    fb_w_linestride = val;
}

static uint32_t fb_r_sof1_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_r_sof1;
}

static void fb_r_sof1_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    fb_r_sof1 = val;
}

static uint32_t fb_r_sof2_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_r_sof2;
}

static void fb_r_sof2_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    fb_r_sof2 = val;
}

static uint32_t fb_r_size_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_r_size;
}

static void fb_r_size_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    fb_r_size = val;
}

static uint32_t fb_w_sof1_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_w_sof1;
}

static void fb_w_sof1_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    framebuffer_sync_from_host_maybe();
    fb_w_sof1 = val;
}

static uint32_t fb_w_sof2_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_w_sof2;
}

static void fb_w_sof2_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    framebuffer_sync_from_host_maybe();
    fb_w_sof2 = val;
}

static uint32_t fb_x_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_x_clip;
}

static void fb_x_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    fb_x_clip = val;
    LOG_DBG("writing 0x%08x to FB_X_CLIP\n", (unsigned)fb_x_clip);
}

static uint32_t fb_y_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return fb_y_clip;
}

static void fb_y_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                 uint32_t val) {
    fb_y_clip = val;
    LOG_DBG("writing 0x%08x to FB_Y_CLIP\n", (unsigned)fb_y_clip);
}

static uint32_t
isp_backgnd_d_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return isp_backgnd_d;
}

static void isp_backgnd_d_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                     uint32_t val) {
    isp_backgnd_d = val;
}

static uint32_t
isp_backgnd_t_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return isp_backgnd_t;
}

static void
isp_backgnd_t_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                         uint32_t val) {
    isp_backgnd_t = val;
}

static void
vo_control_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx, uint32_t val) {
    spg_set_pix_double_x((bool)(val & PVR2_PIXEL_DOUBLE_MASK));
    mmio_region_pvr2_core_reg_warn_write_handler(region, idx, val);
}

static uint32_t
ta_palette_tp_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return palette_tp;
}

static void
ta_palette_tp_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                         uint32_t val) {
    palette_tp = val;

    LOG_DBG("PVR2: palette type set to: ");

    switch (palette_tp) {
    case PALETTE_TP_ARGB_1555:
        LOG_DBG("ARGB1555\n");
        break;
    case PALETTE_TP_RGB_565:
        LOG_DBG("RGB565\n");
        break;
    case PALETTE_TP_ARGB_4444:
        LOG_DBG("ARGB4444\n");
        break;
    case PALETTE_TP_ARGB_8888:
        LOG_DBG("ARGB8888\n");
        break;
    default:
        LOG_DBG("<unknown %u>\n", (unsigned)palette_tp);
    }

    pvr2_tex_cache_notify_palette_tp_change();
}

static uint32_t
ta_vertbuf_start_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return ta_vertbuf_start;
}

static void
ta_vertbuf_start_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                            uint32_t val) {
    ta_vertbuf_start = val & ~0x3;
}

static uint32_t
ta_next_opb_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    // TODO: actually track the positions of where the OPB blocks should go

    LOG_WARN("You should *really* come up with a real implementation of "
             "%s at line %d of %s\n", __func__, __LINE__, __FILE__);
    LOG_DBG("reading 0x%08x from TA_NEXT_OPB\n", (unsigned)ta_next_opb_init);

    return ta_next_opb_init;
}

/*
 * I really don't know what to do with this other than reset it to
 * ta_vertbuf_start whenever ta_reset gets written to
 */
static uint32_t
ta_vertbuf_pos_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return ta_vertbuf_pos;
}

static uint32_t
ta_glob_tile_clip_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return glob_tile_clip;
}

static void
ta_glob_tile_clip_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                             uint32_t val) {
    glob_tile_clip = val;
    LOG_DBG("writing 0x%08x to TA_GLOB_TILE_CLIP\n", (unsigned)glob_tile_clip);
}

static uint32_t ta_reset_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    LOG_DBG("reading 0 from TA_RESET\n");
    return 0;
}

static void ta_reset_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                                uint32_t val) {
    if (val & 0x80000000) {
        LOG_DBG("TA_RESET!\n");

        ta_vertbuf_pos = ta_vertbuf_start;
    } else {
        LOG_WARN("WARNING: TA_RESET was written to but the one bit that "
                 "actually matters was not set\n");
    }

    pvr2_ta_reinit();
}

static uint32_t
ta_next_opb_init_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    LOG_DBG("reading 0x%08x from TA_NEXT_OPB_INIT\n",
            (unsigned)ta_next_opb_init);
    return ta_next_opb_init;
}

static void
ta_next_opb_init_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                            uint32_t val) {
    ta_next_opb_init = val;
    LOG_DBG("writing 0x%08x to TA_NEXT_OPB_INIT\n",
            (unsigned)ta_next_opb_init);
}

#define PAL_RAM_FIRST_IDX ((PVR2_PALETTE_RAM_FIRST - ADDR_PVR2_CORE_FIRST) / 4)
#define PAL_RAM_LAST_IDX  ((PVR2_PALETTE_RAM_LAST - ADDR_PVR2_CORE_FIRST) / 4)

/*
 * TODO: bounds-checking in this function seems needlessly pedantic and probably
 * could be disabled for non-INVARIANTS builds (or alternatively disabled
 * entirely).
 */
static uint32_t
pal_ram_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    uint32_t *pal32 = (uint32_t*)pvr2_palette_ram;
    if (idx >= PAL_RAM_FIRST_IDX && idx <= PAL_RAM_LAST_IDX) {
        return pal32[idx - PAL_RAM_FIRST_IDX];
    } else {
        error_set_address(idx * 4 + ADDR_PVR2_CORE_FIRST);
        error_set_length(4);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
}

/*
 * TODO: bounds-checking in this function seems needlessly pedantic and probably
 * could be disabled for non-INVARIANTS builds (or alternatively disabled
 * entirely).
 */
static void
pal_ram_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx, uint32_t val) {
    uint32_t *pal32 = (uint32_t*)pvr2_palette_ram;
    if (idx >= PAL_RAM_FIRST_IDX && idx <= PAL_RAM_LAST_IDX) {
        pal32[idx - PAL_RAM_FIRST_IDX] = val;
        pvr2_tex_cache_notify_write(idx * 4 + ADDR_PVR2_CORE_FIRST, 4);
    } else {
        error_set_address(idx * 4 + ADDR_PVR2_CORE_FIRST);
        error_set_length(4);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
}

uint32_t get_fb_r_sof1() {
    return fb_r_sof1;
}

uint32_t get_fb_r_sof2() {
    return fb_r_sof2;
}

uint32_t get_fb_r_ctrl() {
    return fb_r_ctrl;
}

uint32_t get_fb_r_size() {
    return fb_r_size;
}

uint32_t get_fb_w_sof1() {
    return fb_w_sof1;
}

uint32_t get_fb_w_sof2() {
    return fb_w_sof2;
}

uint32_t get_fb_w_ctrl() {
    return fb_w_ctrl;
}

uint32_t get_fb_w_linestride() {
    return fb_w_linestride & 0x1ff;
}

uint32_t get_isp_backgnd_d(void) {
    return isp_backgnd_d;
}

uint32_t get_isp_backgnd_t(void) {
    return isp_backgnd_t;
}

uint32_t get_glob_tile_clip(void) {
    return glob_tile_clip;
}

uint32_t get_fb_x_clip(void) {
    return fb_x_clip;
}

uint32_t get_fb_y_clip(void) {
    return fb_y_clip;
}

unsigned get_fb_x_clip_min(void) {
    return fb_x_clip & 0x7ff;
}

unsigned get_fb_y_clip_min(void) {
    return fb_y_clip & 0x3ff;
}

unsigned get_fb_x_clip_max(void) {
    return (fb_x_clip >> 16) & 0x7ff;
}

unsigned get_fb_y_clip_max(void) {
    return (fb_y_clip >> 16) & 0x3ff;
}

unsigned get_glob_tile_clip_x(void) {
    return (glob_tile_clip & 0x3f) + 1;
}

unsigned get_glob_tile_clip_y(void) {
    return ((glob_tile_clip >> 16) & 0xf) + 1;
}

enum palette_tp get_palette_tp(void) {
    return palette_tp;
}
