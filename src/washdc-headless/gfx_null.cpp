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

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "washdc/gfx/obj.h"

#include "gfx_null.hpp"

static bool flip_screen;
static int bound_obj_handle;
static unsigned bound_obj_w, bound_obj_h;

static void null_render_init(void);
static void null_render_cleanup(void);
static void null_render_bind_tex(struct gfx_il_inst *cmd);
static void null_render_unbind_tex(struct gfx_il_inst *cmd);
static void null_render_update_tex(unsigned tex_obj);
static void null_render_release_tex(unsigned tex_obj);
static void null_render_set_blend_enable(bool enable);
static void null_render_set_rend_param(struct gfx_rend_param const *param);
static void null_render_draw_array(float const *verts, unsigned n_verts);
static void null_render_clear(float const bgcolor[4]);
static void null_render_set_screen_dim(unsigned width, unsigned height);
static void null_render_set_clip_range(float new_clip_min, float new_clip_max);
static void null_render_begin_sort_mode(void);
static void null_render_end_sort_mode(void);
static void null_render_bind_obj(int obj_handle);
static void null_render_unbind_obj(int obj_handle);
static void null_render_target_begin(unsigned width,
                                     unsigned height, int tgt_handle);
static void null_render_target_end(int tgt_handle);
static int null_render_get_fb(int *obj_handle_out, unsigned *width_out,
                              unsigned *height_out, bool *flip_out);
static void null_render_present(void);
static void null_render_new_framebuffer(int obj_handle,
                                        unsigned fb_new_width,
                                        unsigned fb_new_height,
                                        bool do_flip, bool interlaced);
static void null_render_toggle_filter(void);

static void null_render_obj_read(struct gfx_obj *obj, void *out,
                                 size_t n_bytes);

struct rend_if const *null_rend_if_get(void) {
    static struct rend_if null_rend_if;

    null_rend_if.init = null_render_init;
    null_rend_if.cleanup = null_render_cleanup;
    null_rend_if.bind_tex = null_render_bind_tex;
    null_rend_if.unbind_tex = null_render_unbind_tex;
    null_rend_if.update_tex = null_render_update_tex;
    null_rend_if.release_tex = null_render_release_tex;
    null_rend_if.set_blend_enable = null_render_set_blend_enable;
    null_rend_if.set_rend_param = null_render_set_rend_param;
    null_rend_if.set_screen_dim = null_render_set_screen_dim;
    null_rend_if.set_clip_range = null_render_set_clip_range;
    null_rend_if.draw_array = null_render_draw_array;
    null_rend_if.clear = null_render_clear;
    null_rend_if.begin_sort_mode = null_render_begin_sort_mode;
    null_rend_if.end_sort_mode = null_render_end_sort_mode;
    null_rend_if.target_bind_obj = null_render_bind_obj;
    null_rend_if.target_unbind_obj = null_render_unbind_obj;
    null_rend_if.target_begin = null_render_target_begin;
    null_rend_if.target_end = null_render_target_end;
    null_rend_if.video_get_fb = null_render_get_fb;
    null_rend_if.video_present = null_render_present;
    null_rend_if.video_new_framebuffer = null_render_new_framebuffer;
    null_rend_if.video_toggle_filter = null_render_toggle_filter;

    return &null_rend_if;
}

static void null_render_init(void) {
    flip_screen = false;
    bound_obj_handle = 0;
    bound_obj_w = 0.0;
    bound_obj_h = 0.0;
}

static void null_render_cleanup(void) {
}

static void null_render_bind_tex(struct gfx_il_inst *cmd) {
}

static void null_render_unbind_tex(struct gfx_il_inst *cmd) {
}

static void null_render_update_tex(unsigned tex_obj) {
}

static void null_render_release_tex(unsigned tex_obj) {
}

static void null_render_set_blend_enable(bool enable) {
}

static void null_render_set_rend_param(struct gfx_rend_param const *param) {
}

static void null_render_draw_array(float const *verts, unsigned n_verts) {
}

static void null_render_clear(float const bgcolor[4]) {
}

static void null_render_set_screen_dim(unsigned width, unsigned height) {
}

static void null_render_set_clip_range(float new_clip_min, float new_clip_max) {
}

static void null_render_begin_sort_mode(void) {
}

static void null_render_end_sort_mode(void) {
}

static void null_render_bind_obj(int obj_handle) {
#ifdef INVARIANTS
    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    if (obj->on_write ||
        (obj->on_read && obj->on_read != null_render_obj_read))
        RAISE_ERROR(ERROR_INTEGRITY);
#endif
    gfx_obj_get(obj_handle)->on_read = null_render_obj_read;
}

static void null_render_unbind_obj(int obj_handle) {
    struct gfx_obj *obj = gfx_obj_get(obj_handle);

    gfx_obj_alloc(obj);
    if (gfx_obj_get(obj_handle)->state == GFX_OBJ_STATE_TEX)
        memset(obj->dat, 0, obj->dat_len);

    obj->on_read = NULL;
}

static void null_render_obj_read(struct gfx_obj *obj, void *out,
                                 size_t n_bytes) {
    memset(out, 0, n_bytes);
}

static void null_render_target_begin(unsigned width,
                                     unsigned height, int tgt_handle) {
}

static void null_render_target_end(int tgt_handle) {
    gfx_obj_get(tgt_handle)->state = GFX_OBJ_STATE_TEX;
}

static int null_render_get_fb(int *obj_handle_out, unsigned *width_out,
                              unsigned *height_out, bool *flip_out) {
    if (bound_obj_handle < 0)
        return -1;
    *obj_handle_out = bound_obj_handle;
    *width_out = bound_obj_w;
    *height_out = bound_obj_h;
    *flip_out = flip_screen;
    return 0;
}

static void null_render_present(void) {
}

static void null_render_new_framebuffer(int obj_handle,
                                        unsigned fb_new_width,
                                        unsigned fb_new_height, bool do_flip, bool interlaced) {
    flip_screen = do_flip;
    if (obj_handle < 0)
        return;
    bound_obj_handle = obj_handle;
    bound_obj_w = fb_new_width;
    bound_obj_h = fb_new_height;
}

static void null_render_toggle_filter(void) {
}
