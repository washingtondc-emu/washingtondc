/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifndef WASHDC_GFX_TEX_CACHE_H_
#define WASHDC_GFX_TEX_CACHE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum gfx_tex_fmt {
    GFX_TEX_FMT_ARGB_1555,
    GFX_TEX_FMT_RGB_565,
    GFX_TEX_FMT_ARGB_4444,
    GFX_TEX_FMT_ARGB_8888,
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

#ifdef __cplusplus
}
#endif

#endif
