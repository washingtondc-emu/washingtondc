/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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
        unsigned vert_count[PVR2_POLY_TYPE_COUNT];
    } per_frame_counters;

    // performance counters that don't get reset ever
    struct {
        /*
         * number of times textures get transmitted to the gfx infra.
         * this includes both overwritten textures and new textures that aren't
         * overwriting anything that already exists.
         */
        unsigned tex_xmit_count;

        // number of times (non-paletted) textures get invalidated
        unsigned tex_invalidate_count;

        /*
         * number of times paletted textures get invalidated
         *
         * the reason why this is separate from tex_invalidate_count is that this
         * type of overwrite is done through a different code-path so it makes
         * sense to track them separately.  Otherwise they are redundant.
         */
        unsigned pal_tex_invalidate_count;

        /*
         * number of times a texture gets kicked out of the cache to make room
         * for another one
         */
        unsigned texture_overwrite_count;

        /*
         * number of times a new texture gets uploaded into an empty slot in
         * the texture cache.
         */
        unsigned fresh_texture_upload_count;

        /*
         * number of times a texture got kicked out of the cache because it got
         * invalidated but it wasn't immediately needed so we just ignored it.
         *
         * For the sake of simplicity, this counter is included in the
         * tex_xmit_count even though it probably shouldn't since the texture
         * doesn't get transmitted.  It also overlaps with tex_invalidate_count
         * since that's generally how textures end up in this situation.
         */
        unsigned tex_eviction_count;
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

struct maple;
void pvr2_init(struct pvr2 *pvr2, struct dc_clock *clk, struct maple *maple);
void pvr2_cleanup(struct pvr2 *pvr2);

#endif
