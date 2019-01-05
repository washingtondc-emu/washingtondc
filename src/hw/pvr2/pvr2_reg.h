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

#ifndef PVR2_REG_H_
#define PVR2_REG_H_

#include <stddef.h>
#include <stdint.h>

#include "MemoryMap.h"
#include "hw/sh4/types.h"

struct pvr2;

// vlck divider bit for the FB_R_CTRL register
#define PVR2_VCLK_DIV_SHIFT 23
#define PVR2_VCLK_DIV_MASK (1 << PVR2_VCLK_DIV_SHIFT)

// bit in the FB_R_CTRL register that causes each scanline to be sent twice
#define PVR2_LINE_DOUBLE_SHIFT 1
#define PVR2_LINE_DOUBLE_MASK (1 << PVR2_LINE_DOUBLE_SHIFT)

#define PVR2_PALETTE_RAM_FIRST 0x5f9000
#define PVR2_PALETTE_RAM_LAST  0x5f9fff
#define PVR2_PALETTE_RAM_LEN \
    (PVR2_PALETTE_RAM_LAST - PVR2_PALETTE_RAM_FIRST + 1)

uint8_t *pvr2_get_palette_ram(struct pvr2 *pvr2);

void pvr2_reg_init(struct pvr2 *pvr2);
void pvr2_reg_cleanup(struct pvr2 *pvr2);

double pvr2_reg_read_double(addr32_t addr, void *ctxt);
void pvr2_reg_write_double(addr32_t addr, double val, void *ctxt);
float pvr2_reg_read_float(addr32_t addr, void *ctxt);
void pvr2_reg_write_float(addr32_t addr, float val, void *ctxt);
uint32_t pvr2_reg_read_32(addr32_t addr, void *ctxt);
void pvr2_reg_write_32(addr32_t addr, uint32_t val, void *ctxt);
uint16_t pvr2_reg_read_16(addr32_t addr, void *ctxt);
void pvr2_reg_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint8_t pvr2_reg_read_8(addr32_t addr, void *ctxt);
void pvr2_reg_write_8(addr32_t addr, uint8_t val, void *ctxt);

extern struct memory_interface pvr2_reg_intf;

enum palette_tp {
    PALETTE_TP_ARGB_1555,
    PALETTE_TP_RGB_565,
    PALETTE_TP_ARGB_4444,
    PALETTE_TP_ARGB_8888,

    PALETTE_TP_COUNT
};

uint32_t get_fb_r_ctrl(struct pvr2 *pvr2);
uint32_t get_fb_w_ctrl(struct pvr2 *pvr2);
uint32_t get_fb_w_linestride(struct pvr2 *pvr2);
uint32_t get_fb_r_sof1(struct pvr2 *pvr2);
uint32_t get_fb_r_sof2(struct pvr2 *pvr2);
uint32_t get_fb_r_size(struct pvr2 *pvr2);
uint32_t get_fb_w_sof1(struct pvr2 *pvr2);
uint32_t get_fb_w_sof2(struct pvr2 *pvr2);
uint32_t get_isp_backgnd_t(struct pvr2 *pvr2);
uint32_t get_isp_backgnd_d(struct pvr2 *pvr2);
uint32_t get_glob_tile_clip(struct pvr2 *pvr2);
uint32_t get_glob_tile_clip_x(struct pvr2 *pvr2);
uint32_t get_glob_tile_clip_y(struct pvr2 *pvr2);
uint32_t get_fb_x_clip(struct pvr2 *pvr2);
uint32_t get_fb_y_clip(struct pvr2 *pvr2);
unsigned get_fb_x_clip_min(struct pvr2 *pvr2);
unsigned get_fb_y_clip_min(struct pvr2 *pvr2);
unsigned get_fb_x_clip_max(struct pvr2 *pvr2);
unsigned get_fb_y_clip_max(struct pvr2 *pvr2);
enum palette_tp get_palette_tp(struct pvr2 *pvr2);
uint32_t get_ta_yuv_tex_base(struct pvr2 *pvr2);
uint32_t get_ta_yuv_tex_ctrl(struct pvr2 *pvr2);

#endif
