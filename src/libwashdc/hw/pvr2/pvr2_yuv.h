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

#ifndef PVR2_YUV_H_
#define PVR2_YUV_H_

#include <stdint.h>

void pvr2_yuv_init(struct pvr2 *pvr2);
void pvr2_yuv_cleanup(struct pvr2 *pvr2);

void pvr2_yuv_set_base(struct pvr2 *pvr2, uint32_t new_base);
void pvr2_yuv_set_tex_ctrl(struct pvr2 *pvr2, uint32_t new_tex_ctrl);

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

extern struct memory_interface pvr2_ta_yuv_fifo_intf;

#endif
