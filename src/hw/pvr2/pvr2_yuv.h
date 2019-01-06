/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef PVR2_YUV_H_
#define PVR2_YUV_H_

#include <stdint.h>

void pvr2_yuv_init(struct pvr2 *pvr2);
void pvr2_yuv_cleanup(struct pvr2 *pvr2);

void pvr2_yuv_set_base(struct pvr2 *pvr2, uint32_t new_base);

void pvr2_yuv_input_data(struct pvr2 *pvr2, void const *dat, unsigned n_bytes);

enum pvr2_yuv_fmt {
    PVR2_YUV_FMT_420,
    PVR2_YUV_FMT_422
};

struct pvr2_yuv {
    uint32_t dst_addr;
    enum pvr2_yuv_fmt fmt;
    unsigned macroblock_offset;

    unsigned cur_macroblock_x, cur_macroblock_y;

    // width and height, in terms of 16x16 macroblocks
    unsigned macroblock_count_x, macroblock_count_y;

    uint8_t u_buf[64];
    uint8_t v_buf[64];
    uint8_t y_buf[256];

    bool yuv_complete_event_scheduled;

    struct SchedEvent pvr2_yuv_complete_int_event;
};

#endif
