/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018, 2020 snickerbockers
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

#include "washdc/gfx/tex_cache.h"

struct gfxgl4_tex {
    int obj_handle;
    enum gfx_tex_fmt tex_fmt;
    unsigned width, height;
    bool valid;
};

struct gfxgl4_tex const* gfx_gfxgl4_tex_cache_get(unsigned idx);

/*
 * Bind the given gfx_obj to the given texture-unit.
 */
void gfxgl4_tex_cache_bind(unsigned tex_no, int obj_no, unsigned width,
                    unsigned height, enum gfx_tex_fmt tex_fmt);

void gfxgl4_tex_cache_unbind(unsigned tex_no);

void gfxgl4_tex_cache_evict(unsigned idx);

void gfxgl4_tex_cache_init(void);
void gfxgl4_tex_cache_cleanup(void);

#endif
