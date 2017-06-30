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

#ifndef PVR2_TEX_CACHE_H_
#define PVR2_TEX_CACHE_H_

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pvr2_ta.h"

struct geo_buf;

/*
 * this is arbitrary.  I chose a value that is smaller than it should be
 * because I want to make sure the system can properly swap textures in/out
 * of the cache.
 */
#define PVR2_TEX_CACHE_SIZE 8
#define PVR2_TEX_CACHE_MASK (PVR2_TEX_CACHE_SIZE - 1)

struct pvr2_tex {
    uint32_t addr_first, addr_last;

    unsigned w, h;
    int pix_fmt;

    // if this is not set then this part of the cache is empty
    bool valid;

    /*
     * if this is set, it means that this entry in the texture cache has
     * changed since the last update.  If this is not set, then the data in
     * dat is not valid (although the data in the corresponding entry in
     * OpenGL's tex cache is).
     */
    bool dirty;

    /*
     * TODO: this is a pretty big waste of memory
     * 4 MB * PVR2_TEX_CACHE_SIZE * (1 + GEO_BUF_COUNT) bytes
     *
     * since we only use this when the texture needs to be updated, a malloc'd
     * pointer will probably be good enough.
     */
    uint8_t dat[PVR2_TEX_MAX_BYTES];
};

// insert the given texture into the cache
struct pvr2_tex *pvr2_tex_cache_add(uint32_t addr,
                                    unsigned w, unsigned h,
                                    int pix_fmt);

struct pvr2_tex *pvr2_tex_cache_find(uint32_t addr, unsigned w,
                                     unsigned h, int pix_fmt);

void pvr2_tex_cache_notify_write(uint32_t addr_first, uint32_t len);

int pvr2_tex_cache_get_idx(struct pvr2_tex const *tex);

/*
 * this function sends the texture cache over to the rendering thread
 * by copying it to the given geo_buf
 */
void pvr2_tex_cache_xmit(struct geo_buf *out);

#endif
