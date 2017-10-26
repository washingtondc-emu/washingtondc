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

#ifndef GEO_BUF_H_
#define GEO_BUF_H_

#include <assert.h>

#include "error.h"
#include "pvr2_tex_cache.h"

/*
 * a geo_buf is a pre-allocated buffer used to pass data from the emulation
 * thread to the gfx_thread.  They are stored in a ringbuffer in which the
 * emulation code produces and the rendering code consumes.  Currently this code
 * supports only triangles, but it will eventually grow to encapsulate
 * everything.
 */

/*
 * max number of triangles for a single geo_buf.  Maybe it doesn't need to be
 * this big, or maybe it isn't big enough.  Who is John Galt?
 */
#define GEO_BUF_TRIANGLE_COUNT 131072
#define GEO_BUF_VERT_COUNT (GEO_BUF_TRIANGLE_COUNT * 3)

/*
 * offsets to vertex components within the geo_buf's vert array
 * these are in terms of sizeof(float)
 */
#define GEO_BUF_POS_OFFSET 0
#define GEO_BUF_BASE_COLOR_OFFSET 3
#define GEO_BUF_OFFS_COLOR_OFFSET 7
#define GEO_BUF_TEX_COORD_OFFSET 11

/*
 * the number of elements per vertex.  Currently this means 3 floats for the
 * coordinates, 4 floats for the base color, 4 floats for the offset color and
 * two floats for the texture coordinates
 */
#define GEO_BUF_VERT_LEN 13

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

static_assert(PVR2_DEPTH_FUNC_COUNT == 8,
              "incorrect number of depth functions");

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

/*
 * There is one poly_group for each polygon header sent to the pvr2.
 * The poly-group contains per-header settings such as textures.
 */
struct poly_group {
    unsigned n_verts;
    float verts[GEO_BUF_VERT_COUNT * GEO_BUF_VERT_LEN];

    bool tex_enable;
    unsigned tex_idx; // only valid if tex_enable=true
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;
    enum tex_wrap_mode tex_wrap_mode[2]; // wrap mode for u and v coordinates

    // only valid of blend_enable=true in the display_list structure
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;
};

/*
 * There are five display lists:
 *
 * Opaque
 * Punch-through polygon
 * Opaque/punch-through modifier volume
 * Translucent
 * Translucent modifier volume
 *
 * They are rendered by the opengl backend in that order.
 * Currently all 5 lists are treated as being opaque; this is obviously
 * incorrect.
 */
struct display_list {
    unsigned n_groups;
    struct poly_group *groups;

    bool blend_enable;
};

enum display_list_type {
    DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE = DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

struct geo_buf {
    struct pvr2_tex tex_cache[PVR2_TEX_CACHE_SIZE];

    // each group of polygons has a distinct texture, shader, depth-sorting etc.
    struct display_list lists[DISPLAY_LIST_COUNT];

    unsigned frame_stamp;

    // render dimensions
    unsigned screen_width, screen_height;

    float bgcolor[4];
    float bgdepth;

    // near and far clipping plane Z-coordinates
    float clip_min, clip_max;
};

/*
 * return the next geo_buf to be consumed, or NULL if there are none.
 * This function never blocks.
 */
struct geo_buf *geo_buf_get_cons(void);

/*
 * return the next geo_buf to be produced.  This function never returns NULL.
 */
struct geo_buf *geo_buf_get_prod(void);

// consume the current geo_buf (which is the one returned by geo_buf_get_cons)
void geo_buf_consume(void);

/*
 * mark the current geo_buf as having been consumed.
 *
 * This function can block if the buffer is full; this is not ideal
 * and I would like to find a way to revisit this some time in the future.  For
 * now, stability trumps performance.
 */
void geo_buf_produce(void);

unsigned get_cur_frame_stamp(void);

ERROR_INT_ATTR(src_blend_factor);
ERROR_INT_ATTR(dst_blend_factor);
ERROR_INT_ATTR(display_list_index);
ERROR_INT_ATTR(geo_buf_group_index);

#endif
