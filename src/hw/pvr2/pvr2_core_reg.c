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

#include "pvr2_core_reg.h"

static uint32_t fb_r_sof1, fb_r_sof2, fb_r_ctrl, fb_r_size,
    fb_w_sof1, fb_w_sof2, fb_w_ctrl, fb_w_linestride, isp_backgnd_t,
    isp_backgnd_d, glob_tile_clip, fb_x_clip, fb_y_clip;

static enum palette_tp palette_tp;

// 5f8128, 5f8138
static uint32_t ta_vertbuf_pos, ta_vertbuf_start;

static uint32_t ta_next_opb_init;

#define N_PVR2_CORE_REGS (ADDR_PVR2_CORE_LAST - ADDR_PVR2_CORE_FIRST + 1)
static reg32_t pvr2_core_regs[N_PVR2_CORE_REGS];

uint8_t pvr2_palette_ram[PVR2_PALETTE_RAM_LEN];

struct pvr2_core_mem_mapped_reg;

typedef int(*pvr2_core_reg_read_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
typedef int(*pvr2_core_reg_write_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_write_only_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len);
static int
pvr2_core_revision_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len);

static int
fb_r_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static int
vo_control_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
vo_control_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);

static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_size_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_size_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_w_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_w_linestride_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void *buf, addr32_t addr, unsigned len);
static int
fb_w_linestride_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void const *buf, addr32_t addr, unsigned len);
static int
ta_reset_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len);
static int
ta_reset_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void const *buf, addr32_t addr, unsigned len);
static int
ta_vertbuf_start_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
static int
ta_vertbuf_start_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);
static int
ta_vertbuf_pos_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
static int
ta_startrender_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);
static int
isp_backgnd_t_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
isp_backgnd_t_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);
static int
isp_backgnd_d_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
isp_backgnd_d_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);
static int
glob_tile_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len);
static int
glob_tile_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);
static int
fb_x_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_x_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);
static int
fb_y_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
fb_y_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static int
ta_next_opb_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len);

static int
ta_next_opb_init_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
static int
ta_next_opb_init_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr, unsigned len);

static int
ta_palette_tp_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
ta_palette_tp_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);

static int
ta_palette_ram_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len);
static int
ta_palette_ram_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);


static struct pvr2_core_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    pvr2_core_reg_read_handler_t on_read;
    pvr2_core_reg_write_handler_t on_write;
} pvr2_core_reg_info[] = {
    /*
     * in theory this is read-only, but in practise the BIOS tries to write 0
     * to it for some reason which I do not know.  Ergo, let it think it won by
     * sending the write to warn_pvr2_core_reg_write_handler.  This will not effect
     * the output of pvr2_core_id_read_handler.
     */
    { "ID", 0x5f8000, 4,
      pvr2_core_id_read_handler, warn_pvr2_core_reg_write_handler },

    { "REVISION", 0x5f8004, 4,
      pvr2_core_revision_read_handler, pvr2_core_read_only_reg_write_handler },
    { "SOFTRESET", 0x5f8008, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "STARTRENDER", 0x5f8014, 4,
      pvr2_core_write_only_reg_read_handler, ta_startrender_reg_write_handler},

    { "PARAM_BASE", 0x5f8020, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "REGION_BASE", 0x5f802c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPAN_SORT_CFG", 0x5f8030, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_BORDER_COL", 0x5f8040, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_CTRL", 0x5f8044, 4,
      fb_r_ctrl_reg_read_handler, fb_r_ctrl_reg_write_handler },
    { "FB_W_CTRL", 0x5f8048, 4,
      fb_w_ctrl_reg_read_handler, fb_w_ctrl_reg_write_handler },
    { "FB_W_LINESTRIDE", 0x5f804c, 4,
      fb_w_linestride_reg_read_handler, fb_w_linestride_reg_write_handler },
    { "FB_R_SOF1", 0x5f8050, 4,
      fb_r_sof1_reg_read_handler, fb_r_sof1_reg_write_handler },
    { "FB_R_SOF2", 0x5f8054, 4,
      fb_r_sof2_reg_read_handler, fb_r_sof2_reg_write_handler },
    { "FB_R_SIZE", 0x5f805c, 4,
      fb_r_size_reg_read_handler, fb_r_size_reg_write_handler },
    { "FB_W_SOF1", 0x5f8060, 4,
      fb_w_sof1_reg_read_handler, fb_w_sof1_reg_write_handler },
    { "FB_W_SOF2", 0x5f8064, 4,
      fb_w_sof2_reg_read_handler, fb_w_sof2_reg_write_handler },
    { "FB_X_CLIP", 0x5f8068, 4,
      fb_x_clip_reg_read_handler, fb_x_clip_reg_write_handler },
    { "FB_Y_CLIP", 0x5f806c, 4,
      fb_y_clip_reg_read_handler, fb_y_clip_reg_write_handler },
    { "FPU_SHAD_SCALE", 0x5f8074, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_CULL_VAL", 0x5f8078, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PARAM_CFG", 0x5f807c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "HALF_OFFSET", 0x5f8080, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PERP_VAL", 0x5f8084, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_BACKGND_D", 0x5f8088, 4,
      isp_backgnd_d_reg_read_handler, isp_backgnd_d_reg_write_handler },
    { "ISP_BACKGND_T", 0x5f808c, 4,
      isp_backgnd_t_reg_read_handler, isp_backgnd_t_reg_write_handler },
    { "ISP_FEED_CFG", 0x5f8098, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MAX", 0x5f80bc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MIN", 0x5f80c0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TEXT_CONTROL", 0x5f80e4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SCALER_CTL", 0x5f80f4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_BURSTXTRL", 0x5f8110, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "Y_COEFF", 0x5f8118, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_REFRESH", 0x5f80a0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_CFG", 0x5f80a8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_RAM", 0x5f80b0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_VERT", 0x5f80b4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_DENSITY", 0x5f80b8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_HBLANK_INT", 0x5f80c8, 4,
      read_spg_hblank_int, write_spg_hblank_int },
    { "SPG_VBLANK_INT", 0x5f80cc, 4,
      read_spg_vblank_int, write_spg_vblank_int },
    { "SPG_CONTROL", 0x5f80d0, 4,
      read_spg_control, write_spg_control },
    { "SPG_HBLANK", 0x5f80d4, 4,
      read_spg_hblank, write_spg_hblank },
    { "SPG_LOAD", 0x5f80d8, 4,
      read_spg_load, write_spg_load },
    { "SPG_VBLANK", 0x5f80dc, 4,
      read_spg_vblank, write_spg_vblank },
    { "SPG_WIDTH", 0x5f80e0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_CONTROL", 0x5f80e8, 4,
      vo_control_reg_read_handler, vo_control_reg_write_handler },
    { "VO_STARTX", 0x5f80ec, 4,
      default_pvr2_core_reg_read_handler, default_pvr2_core_reg_write_handler },
    { "VO_STARTY", 0x5f80f0, 4,
      default_pvr2_core_reg_read_handler, default_pvr2_core_reg_write_handler },
    { "PALETTE_TP", 0x5f8108, 4,
      ta_palette_tp_reg_read_handler, ta_palette_tp_reg_write_handler },
    { "SPG_STATUS", 0x5f810c, 4,
      read_spg_status, pvr2_core_read_only_reg_write_handler },
    { "TA_OL_BASE", 0x5f8124, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "PT_ALPHA_CMP", 0x5f811c, 4, // TODO: punch-through polygon support
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_VERTBUF_START", 0x5f8128, 4,
      ta_vertbuf_start_reg_read_handler, ta_vertbuf_start_reg_write_handler },
    { "TA_ISP_LIMIT", 0x5f8130, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_NEXT_OPB", 0x5f8134, 4,
      ta_next_opb_reg_read_handler, pvr2_core_read_only_reg_write_handler },
    { "TA_VERTBUF_POS", 0x5f8138, 4,
      ta_vertbuf_pos_reg_read_handler, pvr2_core_read_only_reg_write_handler },
    { "TA_OL_LIMIT", 0x5f812c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_GLOB_TILE_CLIP", 0x5f813c, 4,
      glob_tile_clip_reg_read_handler, glob_tile_clip_reg_write_handler },
    { "TA_ALLOC_CTRL", 0x5f8140, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_RESET", 0x5f8144, 4,
      ta_reset_reg_read_handler, ta_reset_reg_write_handler },
    { "TA_NEXT_OPB_INIT", 0x5f8164, 4,
      ta_next_opb_init_reg_read_handler, ta_next_opb_init_reg_write_handler },

    // The PVR2 fog table - 128 single-precision floats
    { "FOG_TABLE_00", 0x5f8200, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_01", 0x5f8204, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_02", 0x5f8208, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_03", 0x5f820c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_04", 0x5f8210, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_05", 0x5f8214, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_06", 0x5f8218, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_07", 0x5f821c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_08", 0x5f8220, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_09", 0x5f8224, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0a", 0x5f8228, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0b", 0x5f822c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0c", 0x5f8230, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0d", 0x5f8234, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0e", 0x5f8238, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_0f", 0x5f823c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_10", 0x5f8240, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_11", 0x5f8244, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_12", 0x5f8248, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_13", 0x5f824c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_14", 0x5f8250, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_15", 0x5f8254, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_16", 0x5f8258, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_17", 0x5f825c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_18", 0x5f8260, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_19", 0x5f8264, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1a", 0x5f8268, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1b", 0x5f826c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1c", 0x5f8270, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1d", 0x5f8274, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1e", 0x5f8278, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_1f", 0x5f827c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_20", 0x5f8280, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_21", 0x5f8284, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_22", 0x5f8288, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_23", 0x5f828c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_24", 0x5f8290, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_25", 0x5f8294, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_26", 0x5f8298, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_27", 0x5f829c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_28", 0x5f82a0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_29", 0x5f82a4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2a", 0x5f82a8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2b", 0x5f82ac, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2c", 0x5f82b0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2d", 0x5f82b4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2e", 0x5f82b8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_2f", 0x5f82bc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_30", 0x5f82c0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_31", 0x5f82c4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_32", 0x5f82c8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_33", 0x5f82cc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_34", 0x5f82d0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_35", 0x5f82d4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_36", 0x5f82d8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_37", 0x5f82dc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_38", 0x5f82e0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_39", 0x5f82e4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3a", 0x5f82e8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3b", 0x5f82ec, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3c", 0x5f82f0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3d", 0x5f82f4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3e", 0x5f82f8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_3f", 0x5f82fc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_40", 0x5f8300, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_41", 0x5f8304, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_42", 0x5f8308, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_43", 0x5f830c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_44", 0x5f8310, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_45", 0x5f8314, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_46", 0x5f8318, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_47", 0x5f831c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_48", 0x5f8320, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_49", 0x5f8324, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4a", 0x5f8328, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4b", 0x5f832c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4c", 0x5f8330, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4d", 0x5f8334, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4e", 0x5f8338, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_4f", 0x5f833c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_50", 0x5f8340, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_51", 0x5f8344, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_52", 0x5f8348, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_53", 0x5f834c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_54", 0x5f8350, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_55", 0x5f8354, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_56", 0x5f8358, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_57", 0x5f835c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_58", 0x5f8360, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_59", 0x5f8364, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5a", 0x5f8368, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5b", 0x5f836c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5c", 0x5f8370, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5d", 0x5f8374, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5e", 0x5f8378, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_5f", 0x5f837c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_60", 0x5f8380, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_61", 0x5f8384, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_62", 0x5f8388, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_63", 0x5f838c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_64", 0x5f8390, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_65", 0x5f8394, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_66", 0x5f8398, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_67", 0x5f839c, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_68", 0x5f83a0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_69", 0x5f83a4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6a", 0x5f83a8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6b", 0x5f83ac, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6c", 0x5f83b0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6d", 0x5f83b4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6e", 0x5f83b8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_6f", 0x5f83bc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_70", 0x5f83c0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_71", 0x5f83c4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_72", 0x5f83c8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_73", 0x5f83cc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_74", 0x5f83d0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_75", 0x5f83d4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_76", 0x5f83d8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_77", 0x5f83dc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_78", 0x5f83e0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_79", 0x5f83e4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7a", 0x5f83e8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7b", 0x5f83ec, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7c", 0x5f83f0, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7d", 0x5f83f4, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7e", 0x5f83f8, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_TABLE_7f", 0x5f83fc, 4,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { NULL }
};

static struct pvr2_core_mem_mapped_reg pal_ram_reg = {
    "PALETTE_RAM", 0x5f9000, 4,
    ta_palette_ram_reg_read_handler, ta_palette_ram_reg_write_handler
};

int pvr2_core_reg_read(void *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr == curs->addr) && (len == curs->len))
            return curs->on_read(curs, buf, addr, len);
        curs++;
    }

    if (addr >= 0x5f9000 && addr <= 0x5f9fff)
        return pal_ram_reg.on_read(curs, buf, addr, len);

    error_set_feature("reading from one of the pvr2 core registers");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int pvr2_core_reg_write(void const *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr == curs->addr) && (len == curs->len))
            return curs->on_write(curs, buf, addr, len);
        curs++;
    }

    if (addr >= 0x5f9000 && addr <= 0x5f9fff)
        return pal_ram_reg.on_write(curs, buf, addr, len);

    error_set_feature("writing to one of the pvr2 core registers");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(buf, idx + pvr2_core_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(idx + pvr2_core_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_core_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_pvr2_core_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        LOG_DBG("read from pvr2 core register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            LOG_DBG("read 0x%02x from pvr2 core register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            LOG_DBG("read 0x%04x from pvr2 core register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            LOG_DBG("read 0x%08x from pvr2 core register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            LOG_DBG("read from pvr2 core register %s\n", reg_info->reg_name);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        LOG_DBG("writing 0x%02x from pvr2 core register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        LOG_DBG("writing 0x%04x to pvr2 core register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        LOG_DBG("writing 0x%08x to pvr2 core register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        LOG_DBG("reading from pvr2 core register %s\n",
                reg_info->reg_name);
    }

    return default_pvr2_core_reg_write_handler(reg_info, buf, addr, len);
}

static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    /* hardcoded hardware ID */
    uint32_t tmp = 0x17fd11db;

    memcpy(buf, &tmp, len);

    return MEM_ACCESS_SUCCESS;
}

static int
pvr2_core_revision_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 17;

    memcpy(buf, &tmp, len);

    return MEM_ACCESS_SUCCESS;
}

static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    error_set_feature("whatever happens when you write to "
                      "a read-only register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
pvr2_core_write_only_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len) {
    error_set_feature("whatever happens when you read from "
                      "a write-only register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}


static int
fb_r_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_ctrl, sizeof(fb_r_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    reg32_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    if (new_val & PVR2_VCLK_DIV_MASK)
        spg_set_pclk_div(1);
    else
        spg_set_pclk_div(2);

    spg_set_pix_double_y((bool)(new_val & PVR2_LINE_DOUBLE_MASK));

    memcpy(&fb_r_ctrl, buf, sizeof(fb_r_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
vo_control_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    return warn_pvr2_core_reg_read_handler(reg_info, buf, addr, len);
}

static int
vo_control_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    reg32_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    spg_set_pix_double_x((bool)(new_val & PVR2_PIXEL_DOUBLE_MASK));

    return warn_pvr2_core_reg_write_handler(reg_info, buf, addr, len);
}

static int
fb_r_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_sof1, sizeof(fb_r_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_sof1, buf, sizeof(fb_r_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_sof2, sizeof(fb_r_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_sof2, buf, sizeof(fb_r_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_size_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_r_size, sizeof(fb_r_size));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_r_size_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_r_size, buf, sizeof(fb_r_size));
    return MEM_ACCESS_SUCCESS;
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

static int
fb_w_sof1_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_sof1, sizeof(fb_w_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof1_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_sof1, buf, sizeof(fb_w_sof1));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof2_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_sof2, sizeof(fb_w_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_sof2_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_sof2, buf, sizeof(fb_w_sof2));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_ctrl_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_ctrl, sizeof(fb_w_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_ctrl_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_ctrl, buf, sizeof(fb_w_ctrl));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_linestride_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_w_linestride, sizeof(fb_w_linestride));
    return MEM_ACCESS_SUCCESS;
}

static int
fb_w_linestride_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void const *buf, addr32_t addr, unsigned len) {
    framebuffer_sync_from_host_maybe();
    memcpy(&fb_w_linestride, buf, sizeof(fb_w_linestride));
    return MEM_ACCESS_SUCCESS;
}

static int
ta_reset_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    LOG_DBG("reading 0 from TA_RESET\n");
    memset(buf, 0, len);
    return 0;
}

static int
ta_reset_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void const *buf, addr32_t addr, unsigned len) {
    uint32_t val;
    memcpy(&val, buf, sizeof(val));

    if (val & 0x80000000) {
        LOG_DBG("TA_RESET!\n");

        ta_vertbuf_pos = ta_vertbuf_start;
    } else {
        LOG_WARN("WARNING: TA_RESET was written to but the one bit that "
                 "actually matters was not set\n");
    }

    pvr2_ta_reinit();

    return 0;
}

static int
ta_vertbuf_start_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &ta_vertbuf_start, sizeof(ta_vertbuf_start));
    return 0;
}

static int
ta_vertbuf_start_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr, unsigned len) {
    memcpy(&ta_vertbuf_start, buf, sizeof(ta_vertbuf_start));
    ta_vertbuf_start &= ~0x3;
    return 0;
}

/*
 * I really don't know what to do with this other than reset it to
 * ta_vertbuf_start whenever ta_reset gets written to
 */
static int
ta_vertbuf_pos_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &ta_vertbuf_pos, len);
    return 0;
}

static int
ta_startrender_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    pvr2_ta_startrender();
    return 0;
}

static int
isp_backgnd_t_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &isp_backgnd_t, sizeof(isp_backgnd_t));
    return 0;
}

static int
isp_backgnd_t_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&isp_backgnd_t, buf, sizeof(isp_backgnd_t));
    return 0;
}

static int
isp_backgnd_d_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &isp_backgnd_d, sizeof(isp_backgnd_d));
    return 0;
}

static int
isp_backgnd_d_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&isp_backgnd_d, buf, sizeof(isp_backgnd_d));
    return 0;
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

static int
glob_tile_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &glob_tile_clip, sizeof(glob_tile_clip));
    return 0;
}

static int
glob_tile_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    memcpy(&glob_tile_clip, buf, sizeof(glob_tile_clip));

    LOG_DBG("writing 0x%08x to TA_GLOB_TILE_CLIP\n", (unsigned)glob_tile_clip);

    return 0;
}

static int
fb_x_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_x_clip, sizeof(fb_x_clip));
    return 0;
}

static int
fb_x_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_x_clip, buf, sizeof(fb_x_clip));

    LOG_DBG("writing 0x%08x to FB_X_CLIP\n", (unsigned)fb_x_clip);

    return 0;
}

static int
fb_y_clip_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &fb_y_clip, sizeof(fb_y_clip));
    return 0;
}

static int
fb_y_clip_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    memcpy(&fb_y_clip, buf, sizeof(fb_y_clip));

    LOG_DBG("writing 0x%08x to FB_Y_CLIP\n", (unsigned)fb_y_clip);

    return 0;
}

static int
ta_next_opb_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    // TODO: actually track the positions of where the OPB blocks should go


    LOG_WARN("You should *really* come up with a real implementation of "
             "%s at line %d of %s\n", __func__, __LINE__, __FILE__);

    memcpy(buf, &ta_next_opb_init, len);

    LOG_DBG("reading 0x%08x from TA_NEXT_OPB\n", (unsigned)ta_next_opb_init);

    return 0;
}

static int
ta_next_opb_init_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &ta_next_opb_init, len);
    LOG_DBG("reading 0x%08x from TA_NEXT_OPB_INIT\n",
            (unsigned)ta_next_opb_init);
    return 0;
}

static int
ta_next_opb_init_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr, unsigned len) {
    memcpy(&ta_next_opb_init, buf, sizeof(ta_next_opb_init));
    LOG_DBG("writing 0x%08x to TA_NEXT_OPB_INIT\n",
            (unsigned)ta_next_opb_init);
    return 0;
}

static int
ta_palette_tp_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    *(uint32_t*)buf = (uint32_t)palette_tp;
    return 0;
}
static int
ta_palette_tp_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    uint32_t const *in_val = (uint32_t const*)buf;
    palette_tp = (enum palette_tp)(*in_val);

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
    return 0;
}

static int
ta_palette_ram_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    if (addr >= PVR2_PALETTE_RAM_FIRST &&
        (addr + len) <= PVR2_PALETTE_RAM_LAST) {
        memcpy(buf, pvr2_palette_ram + (addr - PVR2_PALETTE_RAM_FIRST), len);
    } else {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    return 0;
}

static int
ta_palette_ram_reg_write_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    LOG_DBG("%s reached (%u-byte write to %08x): ", __func__, len, addr);
    addr32_t addr_tmp;
    __attribute__((unused)) uint8_t *buf8 = (uint8_t*)buf;
    for (addr_tmp = addr+len-1; addr_tmp >= addr; addr_tmp--) {
        LOG_DBG("%02x", (unsigned)*buf8++);
    }
    LOG_DBG("\n");

    if (addr >= PVR2_PALETTE_RAM_FIRST &&
        (addr - 1 + len) <= PVR2_PALETTE_RAM_LAST) {
        memcpy(pvr2_palette_ram + (addr - PVR2_PALETTE_RAM_FIRST), buf, len);
        pvr2_tex_cache_notify_write(addr, len);
    } else {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    return 0;
}
