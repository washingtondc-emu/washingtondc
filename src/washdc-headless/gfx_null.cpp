/*******************************************************************************
 *
 * Copyright 2019, 2020 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "gfx_obj.h"

#include "gfx_null.hpp"

static bool flip_screen;
static int bound_obj_handle;
static unsigned bound_obj_w, bound_obj_h;

static void null_render_init(void);
static void null_render_cleanup(void);
static void null_render_bind_tex(struct gfx_il_inst *cmd);
static void null_render_unbind_tex(struct gfx_il_inst *cmd);
static void null_render_set_blend_enable(struct gfx_il_inst *cmd);
static void null_render_set_rend_param(struct gfx_il_inst *cmd);
static void null_render_draw_array(struct gfx_il_inst *cmd);
static void null_render_clear(struct gfx_il_inst *cmd);
static void null_render_set_clip_range(struct gfx_il_inst *cmd);
static void null_render_begin_sort_mode(struct gfx_il_inst *cmd);
static void null_render_end_sort_mode(struct gfx_il_inst *cmd);
static void null_render_bind_obj(struct gfx_il_inst *cmd);
static void null_render_unbind_obj(struct gfx_il_inst *cmd);
static int null_render_get_fb(int *obj_handle_out, unsigned *width_out,
                              unsigned *height_out, bool *flip_out);
static void null_render_new_framebuffer(int obj_handle,
                                        unsigned fb_new_width,
                                        unsigned fb_new_height,
                                        bool do_flip, bool interlaced);

static void null_render_obj_read(struct gfx_obj *obj, void *out,
                                 size_t n_bytes);
static void null_render_obj_init(struct gfx_il_inst *cmd);
static void null_render_obj_write(struct gfx_il_inst *cmd);
static void null_render_obj_read(struct gfx_il_inst *cmd);
static void null_render_obj_free(struct gfx_il_inst *cmd);
static void null_render_post_framebuffer(struct gfx_il_inst *cmd);
static void null_render_grab_framebuffer(struct gfx_il_inst *cmd);
static void null_render_begin_rend(struct gfx_il_inst *cmd);
static void null_render_end_rend(struct gfx_il_inst *cmd);

static void null_render_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd);

struct gfx_rend_if const *null_rend_if_get(void) {
    static struct gfx_rend_if null_rend_if;

    null_rend_if.init = null_render_init;
    null_rend_if.cleanup = null_render_cleanup;
    null_rend_if.exec_gfx_il = null_render_exec_gfx_il;

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

static void null_render_set_blend_enable(struct gfx_il_inst *cmd) {
}

static void null_render_set_rend_param(struct gfx_il_inst *cmd) {
}

static void null_render_draw_array(struct gfx_il_inst *cmd) {
}

static void null_render_clear(struct gfx_il_inst *cmd) {
}

static void null_render_set_clip_range(struct gfx_il_inst *cmd) {
}

static void null_render_begin_sort_mode(struct gfx_il_inst *cmd) {
}

static void null_render_end_sort_mode(struct gfx_il_inst *cmd) {
}

static void null_render_bind_obj(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.bind_render_target.gfx_obj_handle;

#ifdef INVARIANTS
    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    if (obj->on_write ||
        (obj->on_read && obj->on_read != null_render_obj_read))
        RAISE_ERROR(ERROR_INTEGRITY);
#endif
    gfx_obj_get(obj_handle)->on_read = null_render_obj_read;
}

static void null_render_unbind_obj(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.unbind_render_target.gfx_obj_handle;
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

static void null_render_obj_init(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.init_obj.obj_no;
    size_t n_bytes = cmd->arg.init_obj.n_bytes;
    gfx_obj_init(obj_no, n_bytes);
}

static void null_render_obj_write(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.write_obj.obj_no;
    size_t n_bytes = cmd->arg.write_obj.n_bytes;
    void const *dat = cmd->arg.write_obj.dat;
    gfx_obj_write(obj_no, dat, n_bytes);
}

static void null_render_obj_read(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.read_obj.obj_no;
    size_t n_bytes = cmd->arg.read_obj.n_bytes;
    void *dat = cmd->arg.read_obj.dat;
    gfx_obj_read(obj_no, dat, n_bytes);
}

static void null_render_obj_free(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.free_obj.obj_no;
    gfx_obj_free(obj_no);
}

static void null_render_grab_framebuffer(struct gfx_il_inst *cmd) {
    int handle;
    unsigned width, height;
    bool do_flip;

    if (null_render_get_fb(&handle, &width, &height, &do_flip) != 0) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    struct gfx_obj *obj = gfx_obj_get(handle);
    if (!obj) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    size_t n_bytes = obj->dat_len;
    void *dat = malloc(n_bytes);
    if (!dat) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    gfx_obj_read(handle, dat, n_bytes);

    cmd->arg.grab_framebuffer.fb->valid = true;
    cmd->arg.grab_framebuffer.fb->width = width;
    cmd->arg.grab_framebuffer.fb->height = height;
    cmd->arg.grab_framebuffer.fb->dat = dat;
    cmd->arg.grab_framebuffer.fb->flip = do_flip;
}

static void null_render_post_framebuffer(struct gfx_il_inst *cmd) {
}

static void null_render_begin_rend(struct gfx_il_inst *cmd) {
}

static void null_render_end_rend(struct gfx_il_inst *cmd) {
    gfx_obj_get(cmd->arg.end_rend.rend_tgt_obj)->state = GFX_OBJ_STATE_TEX;
}

static void null_render_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            null_render_bind_tex(cmd);
            break;
        case GFX_IL_UNBIND_TEX:
            null_render_unbind_tex(cmd);
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            null_render_bind_obj(cmd);
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            null_render_unbind_obj(cmd);
            break;
        case GFX_IL_BEGIN_REND:
            null_render_begin_rend(cmd);
            break;
        case GFX_IL_END_REND:
            null_render_end_rend(cmd);
            break;
        case GFX_IL_CLEAR:
            null_render_clear(cmd);
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            null_render_set_blend_enable(cmd);
            break;
        case GFX_IL_SET_REND_PARAM:
            null_render_set_rend_param(cmd);
            break;
        case GFX_IL_SET_CLIP_RANGE:
            null_render_set_clip_range(cmd);
            break;
        case GFX_IL_DRAW_ARRAY:
            null_render_draw_array(cmd);
            break;
        case GFX_IL_INIT_OBJ:
            null_render_obj_init(cmd);
            break;
        case GFX_IL_WRITE_OBJ:
            null_render_obj_write(cmd);
            break;
        case GFX_IL_READ_OBJ:
            null_render_obj_read(cmd);
            break;
        case GFX_IL_FREE_OBJ:
            null_render_obj_free(cmd);
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            null_render_post_framebuffer(cmd);
            break;
        case GFX_IL_GRAB_FRAMEBUFFER:
            null_render_grab_framebuffer(cmd);
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            null_render_begin_sort_mode(cmd);
            break;
        case GFX_IL_END_DEPTH_SORT:
            null_render_end_sort_mode(cmd);
            break;
        default:
            fprintf(stderr, "ERROR: UNKNOWN GFX IL COMMAND %02X\n",
                    (unsigned)cmd->op);
        }
        cmd++;
    }
}
