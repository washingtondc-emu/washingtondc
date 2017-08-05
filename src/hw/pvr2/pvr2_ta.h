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

#ifndef PVR2_TA_H_
#define PVR2_TA_H_

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
#define TEX_CTRL_TEX_ADDR_MASK (0xfffff << TEX_CTRL_TEX_ADDR_SHIFT)

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

int pvr2_ta_fifo_poly_read(void *buf, size_t addr, size_t len);
int pvr2_ta_fifo_poly_write(void const *buf, size_t addr, size_t len);

void pvr2_ta_startrender(void);

void pvr2_ta_reinit(void);

#endif
