/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef GFX_IL_H_
#define GFX_IL_H_

#include <stdbool.h>

#include "gfx/gfx.h"

enum gfx_il {
    // load a texture into the cache
    GFX_IL_SET_TEX,

    // get rid of a texture in the cache
    GFX_IL_FREE_TEX,

    /*
     * call this before sending any rendering commands (not incluiding texcache
     * updates).
     */
    GFX_IL_BEGIN_REND,

    // call this at the end of every frame
    GFX_IL_END_REND,

    // clear the screen to a given background color
    GFX_IL_CLEAR,

    /*
     * use this to enable/disable blending.  There's no reason why this setting
     * can't be merged into GFX_IL_SET_REND_PARAM, but keeping it separate
     * reduces the number of OpenGL state changes that need to be made since
     * opaque polygons will all be sent together and transparent polygons will
     * all be sent together.
     */
    GFX_IL_SET_BLEND_ENABLE,

    // call this to configure rendering parameters
    GFX_IL_SET_REND_PARAM,

    // use this to render a group of polygons
    GFX_IL_DRAW_ARRAY
};

struct gfx_rend_param {
    bool tex_enable;
    unsigned tex_idx;
    // only valid if tex_enable=true
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;
    enum tex_wrap_mode tex_wrap_mode[2]; // wrap mode for u and v coordinates

    // only valid if blend_enable=true
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    float clip_min, clip_max;
    unsigned screen_width, screen_height;
};

union gfx_il_arg {
    struct {
        void const *tex_dat;
        unsigned tex_no;
        int pix_fmt;
        int w_shift, h_shift;
    } set_tex;

    struct {
        unsigned tex_no;
    } free_tex;

    struct {
        unsigned screen_width, screen_height;
    } begin_rend;

    struct {
        float bgcolor[4];
    } clear;

    struct {
        bool do_enable;
    } set_blend_enable;

    struct {
        struct gfx_rend_param param;
    } set_rend_param;

    struct {
        /*
         * each vert has a len of GFX_IL_VERT_LEN; ergo the total length of
         * verts (in terms of sizeof float) is n_verts * GFX_IL_VERT_LEN.
         */
        unsigned n_verts;
        float const *verts;
    } draw_array;
};

struct gfx_il_inst {
    enum gfx_il op;
    union gfx_il_arg arg;
};

void rend_exec_il(struct gfx_il_inst *cmd, unsigned n_cmd);

#endif
