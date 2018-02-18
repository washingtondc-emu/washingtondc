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

/* #include "hw/pvr2/pvr2_tex_cache.h" */

/*
 * This is the gfx_thread's copy of the texture cache.  It mirrors the one
 * in the geo_buf code, and is updated every time a new geo_buf is submitted by
 * the PVR2 STARTRENDER command.
 */

#define GFX_TEX_CACHE_SIZE 512
#define GFX_TEX_CACHE_MASK (GFX_TEX_CACHE_SIZE - 1)

struct gfx_tex {
    int pix_fmt;
    unsigned w_shift, h_shift;

    // if false, none of the other members of this struct are valid
    bool valid;
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
void gfx_tex_cache_add(unsigned idx, struct gfx_tex const *tex,
                       void const *tex_data);

void gfx_tex_cache_evict(unsigned idx);

struct gfx_tex const* gfx_tex_cache_get(unsigned idx);

void gfx_tex_cache_init(void);
void gfx_tex_cache_cleanup(void);

#endif
