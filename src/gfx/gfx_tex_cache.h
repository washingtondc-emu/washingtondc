/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

enum gfx_tex_fmt {
    GFX_TEX_FMT_ARGB_1555,
    GFX_TEX_FMT_RGB_565,
    GFX_TEX_FMT_ARGB_4444,
    GFX_TEX_FMT_YUV_422,

    GFX_TEX_FMT_COUNT
};

/*
 * This is the gfx_thread's copy of the texture cache.  It mirrors the one
 * in the geo_buf code, and is updated every time a new geo_buf is submitted by
 * the PVR2 STARTRENDER command.
 */

#define GFX_TEX_CACHE_SIZE 512
#define GFX_TEX_CACHE_MASK (GFX_TEX_CACHE_SIZE - 1)

struct gfx_tex {
    int obj_handle;
    enum gfx_tex_fmt pix_fmt;
    unsigned width, height;
    bool valid;
};

/*
 * Bind the given gfx_obj to the given texture-unit.
 */
void gfx_tex_cache_bind(unsigned tex_no, int obj_no, unsigned width,
                        unsigned height, enum gfx_tex_fmt pix_fmt);

void gfx_tex_cache_unbind(unsigned tex_no);

void gfx_tex_cache_evict(unsigned idx);

struct gfx_tex const* gfx_tex_cache_get(unsigned idx);

void gfx_tex_cache_init(void);
void gfx_tex_cache_cleanup(void);

#endif
