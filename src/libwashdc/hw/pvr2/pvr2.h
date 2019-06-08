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

#ifndef PVR2_H_
#define PVR2_H_

#include <stdint.h>

#include "dc_sched.h"
#include "pvr2_ta.h"
#include "mem_areas.h"
#include "spg.h"
#include "pvr2_yuv.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)

struct pvr2_stat {
    // performance counters that get reset on a per-frame basis
    struct {
        unsigned poly_count[DISPLAY_LIST_COUNT];
    } per_frame_counters;

    // performance counters that don't get reset ever
    struct {
        unsigned tex_xmit_count;
        unsigned tex_overwrite_count;
    } persistent_counters;
};

struct pvr2 {
    uint32_t reg_backing[N_PVR2_REGS / sizeof(uint32_t)];
    struct dc_clock *clk;
    struct pvr2_ta ta;
    struct pvr2_spg spg;
    struct pvr2_yuv yuv;
    struct pvr2_fb fb;
    struct pvr2_tex_mem mem;
    struct pvr2_tex_cache tex_cache;

    struct pvr2_stat stat;
};

void pvr2_init(struct pvr2 *pvr2, struct dc_clock *clk);
void pvr2_cleanup(struct pvr2 *pvr2);

#endif
