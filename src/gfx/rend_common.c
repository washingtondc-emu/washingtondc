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

#include <stdio.h>
#include <stdlib.h>

#include "gfx/gfx_tex_cache.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/opengl/opengl_renderer.h"
#include "dreamcast.h"
#include "log.h"
#include "gfx_il.h"

#include "rend_common.h"

static struct rend_if const *rend_ifp = &opengl_rend_if;

// initialize and clean up the graphics renderer
void rend_init(void) {
    rend_ifp->init();
}

void rend_cleanup(void) {
    rend_ifp->cleanup();
}

// tell the renderer to update the given texture from the cache
void rend_update_tex(unsigned tex_no) {
    rend_ifp->update_tex(tex_no);
}

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no) {
    rend_ifp->release_tex(tex_no);
}

static void rend_bind_tex(struct gfx_il_inst *cmd) {
    unsigned tex_no = cmd->arg.bind_tex.tex_no;
    int obj_handle = cmd->arg.bind_tex.gfx_obj_handle;
    enum gfx_tex_fmt pix_fmt = cmd->arg.bind_tex.pix_fmt;
    int width = cmd->arg.bind_tex.width;
    int height = cmd->arg.bind_tex.height;

    gfx_tex_cache_bind(tex_no, obj_handle, width, height, pix_fmt);
}

static void rend_unbind_tex(struct gfx_il_inst *cmd) {
    gfx_tex_cache_unbind(cmd->arg.unbind_tex.tex_no);
}

static void rend_begin_rend(struct gfx_il_inst *cmd) {
    opengl_target_begin(cmd->arg.begin_rend.screen_width,
                        cmd->arg.begin_rend.screen_height,
                        cmd->arg.begin_rend.rend_tgt_obj);
    rend_ifp->set_screen_dim(cmd->arg.begin_rend.screen_width,
                             cmd->arg.begin_rend.screen_height);
}

static void rend_end_rend(struct gfx_il_inst *cmd) {
    opengl_target_end(cmd->arg.end_rend.rend_tgt_obj);
}

static void rend_set_blend_enable(struct gfx_il_inst *cmd) {
    bool en = cmd->arg.set_blend_enable.do_enable;
    rend_ifp->set_blend_enable(en);
}

static void rend_set_rend_param(struct gfx_il_inst *cmd) {
    struct gfx_rend_param const *param = &cmd->arg.set_rend_param.param;
    rend_ifp->set_rend_param(param);
}

static void rend_set_clip_range(struct gfx_il_inst *cmd) {
    float clip_min = cmd->arg.set_clip_range.clip_min;
    float clip_max = cmd->arg.set_clip_range.clip_max;
    rend_ifp->set_clip_range(clip_min, clip_max);
}

static void rend_draw_array(struct gfx_il_inst *cmd) {
    unsigned n_verts = cmd->arg.draw_array.n_verts;
    float const *verts = cmd->arg.draw_array.verts;
    rend_ifp->draw_array(verts, n_verts);
}

static void rend_clear(struct gfx_il_inst *cmd) {
    rend_ifp->clear(cmd->arg.clear.bgcolor);
}

static void rend_obj_init(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.init_obj.obj_no;
    size_t n_bytes = cmd->arg.init_obj.n_bytes;
    gfx_obj_init(obj_no, n_bytes);
}

static void rend_obj_write(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.write_obj.obj_no;
    size_t n_bytes = cmd->arg.write_obj.n_bytes;
    void const *dat = cmd->arg.write_obj.dat;
    gfx_obj_write(obj_no, dat, n_bytes);
}

static void rend_obj_read(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.read_obj.obj_no;
    size_t n_bytes = cmd->arg.read_obj.n_bytes;
    void *dat = cmd->arg.read_obj.dat;
    gfx_obj_read(obj_no, dat, n_bytes);
}

static void rend_obj_free(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.free_obj.obj_no;
    gfx_obj_free(obj_no);
}

static void rend_bind_render_target(struct gfx_il_inst *cmd) {
    opengl_target_bind_obj(cmd->arg.bind_render_target.gfx_obj_handle);
}

static void rend_unbind_render_target(struct gfx_il_inst *cmd) {
    opengl_target_unbind_obj(cmd->arg.unbind_render_target.gfx_obj_handle);
}

static void rend_post_framebuffer(struct gfx_il_inst *cmd) {
    unsigned width = cmd->arg.post_framebuffer.width;
    unsigned height = cmd->arg.post_framebuffer.height;
    int obj_handle = cmd->arg.post_framebuffer.obj_handle;
    bool do_flip = cmd->arg.post_framebuffer.vert_flip;

    gfx_post_framebuffer(obj_handle, width, height, do_flip);
}

void rend_exec_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    /* bool rendering = false; */

    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            rend_bind_tex(cmd);
            break;
        case GFX_IL_UNBIND_TEX:
            rend_unbind_tex(cmd);
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            rend_bind_render_target(cmd);
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            rend_unbind_render_target(cmd);
            break;
        case GFX_IL_BEGIN_REND:
            rend_begin_rend(cmd);
            /* rendering = true; */
            break;
        case GFX_IL_END_REND:
            rend_end_rend(cmd);
            /* rendering = false; */
            break;
        case GFX_IL_CLEAR:
            rend_clear(cmd);
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            rend_set_blend_enable(cmd);
            break;
        case GFX_IL_SET_REND_PARAM:
            rend_set_rend_param(cmd);
            break;
        case GFX_IL_SET_CLIP_RANGE:
            rend_set_clip_range(cmd);
            break;
        case GFX_IL_DRAW_ARRAY:
            rend_draw_array(cmd);
            break;
        case GFX_IL_INIT_OBJ:
            rend_obj_init(cmd);
            break;
        case GFX_IL_WRITE_OBJ:
            rend_obj_write(cmd);
            break;
        case GFX_IL_READ_OBJ:
            rend_obj_read(cmd);
            break;
        case GFX_IL_FREE_OBJ:
            rend_obj_free(cmd);
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            rend_post_framebuffer(cmd);
            break;
        }
        cmd++;
    }

    /* if (rendering) { */
    /*     LOG_ERROR("Failure to end rendering!\n"); */
    /*     RAISE_ERROR(ERROR_INTEGRITY); */
    /* } */
}
