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

#ifndef GFX_TEX_CACHE_H_
#define GFX_TEX_CACHE_H_

#include <stdbool.h>
#include <stdint.h>

#include "hw/pvr2/pvr2_tex_cache.h"

/*
 * This is the gfx_thread's copy of the texture cache.  It mirrors the one
 * in the PVR2 code, and is updated every time a new geo_buf is submitted by
 * the PVR2 STARTRENDER command.
 */

struct gfx_tex {
    // if false, none of the other members of this struct are valid
    bool valid;

    // base-2 logarithim of the width and height of the texture
    unsigned w_shift, h_shift;

    // the texture format, as pvr2 sees it
    int pvr2_pix_fmt;

    /*
     * the image data.  This pointer belongs to the gfx thread and will be
     * freed by it when this texture gets evicted from the cache.
     */
    void *dat;

    /*
     * everything after this comment is just useless metadata we hold on to
     * in case the cmd_thread wants to see what's in the texture cache.
     * Textures are detwiddled by the emulation thread before it sends them to
     * the gfx_thread, and the gfx code never touches the pvr2 texture memory so
     * the addresses are invalid, too.
     */
    bool twiddled;
    bool vq_compression;
    uint32_t addr_first, addr_last;
};

/*
 * store the given tex in the gfx_thread's copy of the texture cache.
 *
 * tex will be shallow-copied into the texture cache, so the texture-cache will
 * assume ownership of tex->dat.  tex itself will not belong to the texture cache
 * since it is copied over.
 *
 * gfx_tex_cache_add will automatically evict the existing texture if there is one.
 */
void gfx_tex_cache_add(unsigned idx, struct gfx_tex const *tex);

void gfx_tex_cache_evict(unsigned idx);

struct gfx_tex const* gfx_tex_cache_get(unsigned idx);

void gfx_tex_cache_init(void);
void gfx_tex_cache_cleanup(void);

#endif
