/*******************************************************************************
 *
 * Copyright 2018-2020 snickerbockers
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

#ifndef GFX_IL_H_
#define GFX_IL_H_

#include <stdbool.h>

#include "washdc/gfx/def.h"
#include "washdc/gfx/obj.h"
#include "washdc/gfx/tex_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

enum gfx_il {
    // load a texture into the cache
    GFX_IL_BIND_TEX,

    GFX_IL_UNBIND_TEX,

    /*
     * Set a gfx_obj as the current render target.  This needs to be done
     * *before* senidng GFX_IL_BEGIN_REND.
     */
    GFX_IL_BIND_RENDER_TARGET,

    /*
     * Unbind a given render target.  This cannot be done between
     * GFX_IL_BEGIN_REND and GFX_IL_END_REND
     */
    GFX_IL_UNBIND_RENDER_TARGET,

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

    // call this to set clip_min, clip_max
    GFX_IL_SET_CLIP_RANGE,

    GFX_IL_SET_USER_CLIP,

    // use this to render a group of polygons
    GFX_IL_DRAW_ARRAY,

    GFX_IL_INIT_OBJ,

    GFX_IL_WRITE_OBJ,

    GFX_IL_READ_OBJ,

    GFX_IL_FREE_OBJ,

    // render data in a gfx_obj to the framebuffer
    GFX_IL_POST_FRAMEBUFFER,

    GFX_IL_GRAB_FRAMEBUFFER,

    /*
     * all triangles submitted between GFX_IL_BEGIN_DEPTH_SORT and
     * GFX_IL_END_DEPTH_SORT will be depth-sorted.
     */
    GFX_IL_BEGIN_DEPTH_SORT,
    GFX_IL_END_DEPTH_SORT
};

struct gfx_framebuffer {
    void *dat;
    unsigned width, height;
    bool valid;
    bool flip;
};

union gfx_il_arg {
    struct {
        int gfx_obj_handle;
        unsigned tex_no;
        enum gfx_tex_fmt pix_fmt;
        int width, height;
    } bind_tex;

    struct {
        unsigned tex_no;
    } unbind_tex;

    // The chosen render-target must be large enough to hold the framebuffer
    struct {
        int gfx_obj_handle;
    } bind_render_target;

    struct {
        int gfx_obj_handle;
    } unbind_render_target;

    /*
     * GFX_IL_UNBIND_RENDER_TARGET doesn't take any arguments because only one
     * gfx_obj can be bound as the render target at a time.
     */

    struct {
        unsigned screen_width, screen_height;

        /*
         * clip rectangle, pixels within it will be written to; pixels outside
         * of it will not.
         *
         * [0] - x_min
         * [1] - y_min
         * [2] - x_max
         * [3] - y_max
         */
        unsigned clip[4];

        int rend_tgt_obj;
    } begin_rend;

    struct {
        int rend_tgt_obj;
    } end_rend;

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
        float clip_min, clip_max;
    } set_clip_range;

    struct {
        unsigned x_min, y_min, x_max, y_max;
    } set_user_clip;

    struct {
        /*
         * each vert has a len of GFX_IL_VERT_LEN; ergo the total length of
         * verts (in terms of sizeof float) is n_verts * GFX_IL_VERT_LEN.
         *
         * note that the contents of verts can be modified by the gfx_il
         * implementation; contents after drawing are undefined.
         */
        unsigned n_verts;
        float *verts;
    } draw_array;

    struct {
        int obj_no;
        size_t n_bytes;
    } init_obj;

    struct {
        void const *dat;
        int obj_no;
        size_t n_bytes;
    } write_obj;

    struct {
        void *dat;
        int obj_no;
        size_t n_bytes;
    } read_obj;

    struct {
        int obj_no;
    } free_obj;

    struct {
        int obj_handle;
        unsigned width, height;
        bool vert_flip;
        bool interlaced;
    } post_framebuffer;

    struct {
        struct gfx_framebuffer *fb;
    } grab_framebuffer;
};

struct gfx_il_inst {
    enum gfx_il op;
    union gfx_il_arg arg;
};

void rend_exec_il(struct gfx_il_inst *cmd, unsigned n_cmd);

#ifdef __cplusplus
}
#endif

#endif
