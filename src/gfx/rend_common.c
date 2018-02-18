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
void rend_update_tex(unsigned tex_no, void const *tex_dat) {
    rend_ifp->update_tex(tex_no, tex_dat);
}

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no) {
    rend_ifp->release_tex(tex_no);
}

static void rend_set_tex(struct gfx_il_inst *cmd) {
    unsigned tex_no = cmd->arg.set_tex.tex_no;
    void *tex_dat = cmd->arg.set_tex.tex_dat;
    struct gfx_tex new_tex_entry = {
        .valid = true,
        .pix_fmt = cmd->arg.set_tex.pix_fmt,
        .w_shift = cmd->arg.set_tex.w_shift,
        .h_shift = cmd->arg.set_tex.h_shift,
    };
    gfx_tex_cache_add(tex_no, &new_tex_entry, tex_dat);
    free(tex_dat);
}

static void rend_free_tex(struct gfx_il_inst *cmd) {
    gfx_tex_cache_evict(cmd->arg.free_tex.tex_no);
}

static void rend_begin_rend(struct gfx_il_inst *cmd) {
    opengl_target_begin(cmd->arg.begin_rend.screen_width,
                        cmd->arg.begin_rend.screen_height);
}

static void rend_end_rend(struct gfx_il_inst *cmd) {
    opengl_target_end();
}

static void rend_set_blend_enable(struct gfx_il_inst *cmd) {
    bool en = cmd->arg.set_blend_enable.do_enable;
    rend_ifp->set_blend_enable(en);
}

static void rend_set_rend_param(struct gfx_il_inst *cmd) {
    struct gfx_rend_param const *param = &cmd->arg.set_rend_param.param;
    rend_ifp->set_rend_param(param);
}

static void rend_draw_array(struct gfx_il_inst *cmd) {
    unsigned n_verts = cmd->arg.draw_array.n_verts;
    float *verts = cmd->arg.draw_array.verts;
    rend_ifp->draw_array(verts, n_verts);
}

static void rend_clear(struct gfx_il_inst *cmd) {
    rend_ifp->clear(cmd->arg.clear.bgcolor);
}

void rend_exec_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    /* bool rendering = false; */

    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_SET_TEX:
            rend_set_tex(cmd);
            break;
        case GFX_IL_FREE_TEX:
            rend_free_tex(cmd);
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
        case GFX_IL_DRAW_ARRAY:
            rend_draw_array(cmd);
            break;
        }
        cmd++;
    }

    /* if (rendering) { */
    /*     LOG_ERROR("Failure to end rendering!\n"); */
    /*     RAISE_ERROR(ERROR_INTEGRITY); */
    /* } */
}
