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

#ifndef WASHDC_GFX_DEF_H_
#define WASHDC_GFX_DEF_H_

#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * offsets to vertex components within the vert array.
 * these are in terms of sizeof(float)
 */
#define GFX_VERT_POS_OFFSET 0
#define GFX_VERT_BASE_COLOR_OFFSET 4
#define GFX_VERT_OFFS_COLOR_OFFSET 8
#define GFX_VERT_TEX_COORD_OFFSET 12

/*
 * the number of elements per vertex.  Currently this means 3 floats for the
 * coordinates, 4 floats for the base color, 4 floats for the offset color and
 * two floats for the texture coordinates
 */
#define GFX_VERT_LEN 14

/*
 * how to combine a polygon's vertex color with a texture
 */
enum tex_inst {
    TEX_INST_DECAL,
    TEX_INST_MOD,
    TEXT_INST_DECAL_ALPHA,
    TEX_INST_MOD_ALPHA
};

enum tex_filter {
    TEX_FILTER_NEAREST,
    TEX_FILTER_BILINEAR,
    TEX_FILTER_TRILINEAR_A,
    TEX_FILTER_TRILINEAR_B
};

enum tex_wrap_mode {
    // repeat the texture when coordinates are greater than 1.0 (tiling effect)
    TEX_WRAP_REPEAT,

    /*
     * this is similar to TEXT_WRAP_REPEAT, except the tiles alternate between
     * not-flipped tiles and flipped tiles
     */
    TEX_WRAP_FLIP,

    // all coordinates greater than 1.0 are clamped to 1.0
    TEX_WRAP_CLAMP
};

enum Pvr2BlendFactor {
    PVR2_BLEND_ZERO,
    PVR2_BLEND_ONE,
    PVR2_BLEND_OTHER,
    PVR2_BLEND_ONE_MINUS_OTHER,
    PVR2_BLEND_SRC_ALPHA,
    PVR2_BLEND_ONE_MINUS_SRC_ALPHA,
    PVR2_BLEND_DST_ALPHA,
    PVR2_BLEND_ONE_MINUS_DST_ALPHA,

    PVR2_BLEND_FACTOR_COUNT
};

static_assert(PVR2_BLEND_FACTOR_COUNT == 8,
              "incorrect number of blending functions");

enum Pvr2DepthFunc {
    PVR2_DEPTH_NEVER,
    PVR2_DEPTH_LESS,
    PVR2_DEPTH_EQUAL,
    PVR2_DEPTH_LEQUAL,
    PVR2_DEPTH_GREATER,
    PVR2_DEPTH_NOTEQUAL,
    PVR2_DEPTH_GEQUAL,
    PVR2_DEPTH_ALWAYS,

    PVR2_DEPTH_FUNC_COUNT
};

enum gfx_user_clip_mode {
    GFX_USER_CLIP_DISABLE,
    GFX_USER_CLIP_INSIDE,
    GFX_USER_CLIP_OUTSIDE
};

struct gfx_rend_param {
    bool tex_enable;
    unsigned tex_idx;
    // only valid if tex_enable=true
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;
    enum tex_wrap_mode tex_wrap_mode[2]; // wrap mode for u and v coordinates

    enum gfx_user_clip_mode user_clip_mode;

    // only valid if blend_enable=true
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    // punch-through polygon mode
    bool pt_mode;
    unsigned pt_ref; // 0-255
};

#ifdef __cplusplus
}
#endif

#endif
