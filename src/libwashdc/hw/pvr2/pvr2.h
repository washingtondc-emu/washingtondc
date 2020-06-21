/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#ifndef PVR2_H_
#define PVR2_H_

#include <stdint.h>

#include "dc_sched.h"
#include "pvr2_ta.h"
#include "pvr2_core.h"
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
    struct pvr2_core core;
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
