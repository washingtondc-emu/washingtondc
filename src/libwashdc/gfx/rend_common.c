/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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

#include "dreamcast.h"
#include "log.h"
#include "washdc/gfx/gfx_il.h"
#include "gfx.h"
#include "gfx_obj.h"

#include "rend_common.h"

struct rend_if const * gfx_rend_ifp;

// initialize and clean up the graphics renderer
void rend_init(struct rend_if const *rend_if) {
    gfx_rend_ifp = rend_if;
    gfx_rend_ifp->init();
}

void rend_cleanup(void) {
    gfx_rend_ifp->cleanup();
    gfx_rend_ifp = NULL;
}

// tell the renderer to update the given texture from the cache
void rend_update_tex(unsigned tex_no) {
    gfx_rend_ifp->update_tex(tex_no);
}

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no) {
    gfx_rend_ifp->release_tex(tex_no);
}

static void rend_begin_rend(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->target_begin(cmd->arg.begin_rend.screen_width,
                           cmd->arg.begin_rend.screen_height,
                           cmd->arg.begin_rend.rend_tgt_obj);
    gfx_rend_ifp->set_screen_dim(cmd->arg.begin_rend.screen_width,
                             cmd->arg.begin_rend.screen_height);
}

static void rend_end_rend(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->target_end(cmd->arg.end_rend.rend_tgt_obj);
}

static void rend_set_blend_enable(struct gfx_il_inst *cmd) {
    bool en = cmd->arg.set_blend_enable.do_enable;
    gfx_rend_ifp->set_blend_enable(en);
}

static void rend_set_rend_param(struct gfx_il_inst *cmd) {
    struct gfx_rend_param const *param = &cmd->arg.set_rend_param.param;
    gfx_rend_ifp->set_rend_param(param);
}

static void rend_set_clip_range(struct gfx_il_inst *cmd) {
    float clip_min = cmd->arg.set_clip_range.clip_min;
    float clip_max = cmd->arg.set_clip_range.clip_max;
    gfx_rend_ifp->set_clip_range(clip_min, clip_max);
}

static void rend_draw_array(struct gfx_il_inst *cmd) {
    unsigned n_verts = cmd->arg.draw_array.n_verts;
    float const *verts = cmd->arg.draw_array.verts;
    gfx_rend_ifp->draw_array(verts, n_verts);
}

static void rend_clear(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->clear(cmd->arg.clear.bgcolor);
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
    gfx_rend_ifp->target_bind_obj(cmd->arg.bind_render_target.gfx_obj_handle);
}

static void rend_unbind_render_target(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->target_unbind_obj(cmd->arg.unbind_render_target.gfx_obj_handle);
}

static void rend_post_framebuffer(struct gfx_il_inst *cmd) {
    unsigned width = cmd->arg.post_framebuffer.width;
    unsigned height = cmd->arg.post_framebuffer.height;
    int obj_handle = cmd->arg.post_framebuffer.obj_handle;
    bool do_flip = cmd->arg.post_framebuffer.vert_flip;
    bool interlace = cmd->arg.post_framebuffer.interlaced;

    gfx_post_framebuffer(obj_handle, width, height, do_flip, interlace);
}

static void rend_grab_framebuffer(struct gfx_il_inst *cmd) {
    int handle;
    unsigned width, height;
    bool do_flip;

    if (gfx_rend_ifp->video_get_fb(&handle, &width, &height, &do_flip) != 0) {
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

static void rend_begin_depth_sort(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->begin_sort_mode();
}

static void rend_end_depth_sort(struct gfx_il_inst *cmd) {
    gfx_rend_ifp->end_sort_mode();
}

#ifdef ENABLE_LOG_DEBUG
#define GFX_IL_TAG "GFX_IL"
static void gfx_log_il_cmd(struct gfx_il_inst const *cmd);
#endif

void rend_exec_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    /* bool rendering = false; */

    while (n_cmd--) {
#ifdef ENABLE_LOG_DEBUG
        gfx_log_il_cmd(cmd);
#endif
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            gfx_rend_ifp->bind_tex(cmd);
            break;
        case GFX_IL_UNBIND_TEX:
            gfx_rend_ifp->unbind_tex(cmd);
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
        case GFX_IL_GRAB_FRAMEBUFFER:
            rend_grab_framebuffer(cmd);
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            rend_begin_depth_sort(cmd);
            break;
        case GFX_IL_END_DEPTH_SORT:
            rend_end_depth_sort(cmd);
            break;
        }
        cmd++;
    }

    /* if (rendering) { */
    /*     LOG_ERROR("Failure to end rendering!\n"); */
    /*     RAISE_ERROR(ERROR_INTEGRITY); */
    /* } */
}

#ifdef ENABLE_LOG_DEBUG
#define GFX_IL_TAG "GFX_IL"
static void gfx_log_il_cmd(struct gfx_il_inst const *cmd) {
    union gfx_il_arg const *arg = &cmd->arg;
    switch (cmd->op) {
    case GFX_IL_BIND_TEX:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_BIND_TEX\n");
        LOG_DBG(GFX_IL_TAG "\tgfx_obj_handle %d\n",
                arg->bind_tex.gfx_obj_handle);
        LOG_DBG(GFX_IL_TAG "\ttex_no %u\n", arg->bind_tex.tex_no);
        LOG_DBG(GFX_IL_TAG "\twidth %u\n", arg->bind_tex.width);
        LOG_DBG(GFX_IL_TAG "\theight %u\n", arg->bind_tex.height);
        break;
    case GFX_IL_UNBIND_TEX:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_UNBIND_TEX\n");
        LOG_DBG(GFX_IL_TAG "\ttex_no %u\n", arg->unbind_tex.tex_no);
        break;
    case GFX_IL_BIND_RENDER_TARGET:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_BIND_RENDER_TARGET\n");
        LOG_DBG(GFX_IL_TAG "\tgfx_obj_handle %u\n",
                arg->bind_render_target.gfx_obj_handle);
        break;
    case GFX_IL_UNBIND_RENDER_TARGET:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_UNBIND_RENDER_TARGET\n");
        LOG_DBG(GFX_IL_TAG "\tgfx_obj_handle %u\n",
                arg->unbind_render_target.gfx_obj_handle);
        break;
    case GFX_IL_BEGIN_REND:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_BEGIN_REND\n");
        LOG_DBG(GFX_IL_TAG "\tscreen_width %u\n",
                arg->begin_rend.screen_width);
        LOG_DBG(GFX_IL_TAG "\tscreen_height %u\n",
                arg->begin_rend.screen_height);
        LOG_DBG(GFX_IL_TAG "\trend_tgt_obj %d\n",
                arg->begin_rend.rend_tgt_obj);
        break;
    case GFX_IL_END_REND:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_END_REND\n");
        LOG_DBG(GFX_IL_TAG "\trend_tgt_obj %d\n", arg->end_rend.rend_tgt_obj);
        break;
    case GFX_IL_CLEAR:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_CLEAR\n");
        LOG_DBG(GFX_IL_TAG "\tbgcolor[0] %f\n", arg->clear.bgcolor[0]);
        LOG_DBG(GFX_IL_TAG "\tbgcolor[1] %f\n", arg->clear.bgcolor[1]);
        LOG_DBG(GFX_IL_TAG "\tbgcolor[2] %f\n", arg->clear.bgcolor[2]);
        LOG_DBG(GFX_IL_TAG "\tbgcolor[3] %f\n", arg->clear.bgcolor[3]);
        break;
    case GFX_IL_SET_BLEND_ENABLE:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_SET_BLEND_ENABLE\n");
        LOG_DBG(GFX_IL_TAG "\tdo_enable %s\n",
                arg->set_blend_enable.do_enable ? "true" : "false");
        break;
    case GFX_IL_SET_REND_PARAM:
        {
            struct gfx_rend_param const *param = &arg->set_rend_param.param;

            char const *tex_inst;
            switch (param->tex_inst) {
            case TEX_INST_DECAL:
                tex_inst = "TEX_INST_DECAL";
                break;
            case TEX_INST_MOD:
                tex_inst = "TEX_INST_MOD";
                break;
            case TEXT_INST_DECAL_ALPHA:
                tex_inst = "TEXT_INST_DECAL_ALPHA";
                break;
            case TEX_INST_MOD_ALPHA:
                tex_inst = "TEX_INST_MOD_ALPHA";
                break;
            default:
                tex_inst = "ERROR/UNKNOWN";
            }

            char const *tex_filter;
            switch (param->tex_filter) {
            case TEX_FILTER_NEAREST:
                tex_filter = "TEX_FILTER_NEAREST";
                break;
            case TEX_FILTER_BILINEAR:
                tex_filter = "TEX_FILTER_BILINEAR";
                break;
            case TEX_FILTER_TRILINEAR_A:
                tex_filter = "TEX_FILTER_TRILINEAR_A";
                break;
            case TEX_FILTER_TRILINEAR_B:
                tex_filter = "TEX_FILTER_TRILINEAR_B";
                break;
            default:
                tex_filter = "ERROR/UNKNOWN";
            }

            char const *tex_wrap_mode[2];
            switch (param->tex_wrap_mode[0]) {
            case TEX_WRAP_REPEAT:
                tex_wrap_mode[0] = "TEX_WRAP_REPEAT";
                break;
            case TEX_WRAP_FLIP:
                tex_wrap_mode[0] = "TEX_WRAP_FLIP";
                break;
            case TEX_WRAP_CLAMP:
                tex_wrap_mode[0] = "TEX_WRAP_CLAMP";
                break;
            default:
                tex_wrap_mode[0] = "ERROR/UNKNOWN";
            }
            switch (param->tex_wrap_mode[1]) {
            case TEX_WRAP_REPEAT:
                tex_wrap_mode[1] = "TEX_WRAP_REPEAT";
                break;
            case TEX_WRAP_FLIP:
                tex_wrap_mode[1] = "TEX_WRAP_FLIP";
                break;
            case TEX_WRAP_CLAMP:
                tex_wrap_mode[1] = "TEX_WRAP_CLAMP";
                break;
            default:
                tex_wrap_mode[1] = "ERROR/UNKNOWN";
            }

            char const *src_blend_factor;
            switch (param->src_blend_factor) {
            case PVR2_BLEND_ZERO:
                src_blend_factor = "PVR2_BLEND_ZERO";
                break;
            case PVR2_BLEND_ONE:
                src_blend_factor = "PVR2_BLEND_ONE";
                break;
            case PVR2_BLEND_OTHER:
                src_blend_factor = "PVR2_BLEND_OTHER";
                break;
            case PVR2_BLEND_ONE_MINUS_OTHER:
                src_blend_factor = "PVR2_BLEND_ONE_MINUS_OTHER";
                break;
            case PVR2_BLEND_SRC_ALPHA:
                src_blend_factor = "PVR2_BLEND_SRC_ALPHA";
                break;
            case PVR2_BLEND_ONE_MINUS_SRC_ALPHA:
                src_blend_factor = "PVR2_BLEND_ONE_MINUS_SRC_ALPHA";
                break;
            case PVR2_BLEND_DST_ALPHA:
                src_blend_factor = "PVR2_BLEND_DST_ALPHA";
                break;
            case PVR2_BLEND_ONE_MINUS_DST_ALPHA:
                src_blend_factor = "PVR2_BLEND_ONE_MINUS_DST_ALPHA";
                break;
            default:
                src_blend_factor = "ERROR/UNKNOWN";
            }

            char const *dst_blend_factor;
            switch (param->dst_blend_factor) {
            case PVR2_BLEND_ZERO:
                dst_blend_factor = "PVR2_BLEND_ZERO";
                break;
            case PVR2_BLEND_ONE:
                dst_blend_factor = "PVR2_BLEND_ONE";
                break;
            case PVR2_BLEND_OTHER:
                dst_blend_factor = "PVR2_BLEND_OTHER";
                break;
            case PVR2_BLEND_ONE_MINUS_OTHER:
                dst_blend_factor = "PVR2_BLEND_ONE_MINUS_OTHER";
                break;
            case PVR2_BLEND_SRC_ALPHA:
                dst_blend_factor = "PVR2_BLEND_SRC_ALPHA";
                break;
            case PVR2_BLEND_ONE_MINUS_SRC_ALPHA:
                dst_blend_factor = "PVR2_BLEND_ONE_MINUS_SRC_ALPHA";
                break;
            case PVR2_BLEND_DST_ALPHA:
                dst_blend_factor = "PVR2_BLEND_DST_ALPHA";
                break;
            case PVR2_BLEND_ONE_MINUS_DST_ALPHA:
                dst_blend_factor = "PVR2_BLEND_ONE_MINUS_DST_ALPHA";
                break;
            default:
                dst_blend_factor = "ERROR/UNKNOWN";
            }

            char const *depth_func;
            switch (param->depth_func) {
            case PVR2_DEPTH_NEVER:
                depth_func = "PVR2_DEPTH_NEVER";
                break;
            case PVR2_DEPTH_LESS:
                depth_func = "PVR2_DEPTH_LESS";
                break;
            case PVR2_DEPTH_EQUAL:
                depth_func = "PVR2_DEPTH_EQUAL";
                break;
            case PVR2_DEPTH_LEQUAL:
                depth_func = "PVR2_DEPTH_LEQUAL";
                break;
            case PVR2_DEPTH_GREATER:
                depth_func = "PVR2_DEPTH_GREATER";
                break;
            case PVR2_DEPTH_NOTEQUAL:
                depth_func = "PVR2_DEPTH_NOTEQUAL";
                break;
            case PVR2_DEPTH_GEQUAL:
                depth_func = "PVR2_DEPTH_GEQUAL";
                break;
            case PVR2_DEPTH_ALWAYS:
                depth_func = "PVR2_DEPTH_ALWAYS";
                break;
            default:
                depth_func = "ERROR/UNKNOWN";
            }

            LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_SET_REND_PARAM\n");
            LOG_DBG(GFX_IL_TAG "\tparam.tex_enable %s\n",
                    param->tex_enable ? "true" : "false");
            LOG_DBG(GFX_IL_TAG "\tparam.tex_idx %u\n", param->tex_idx);
            LOG_DBG(GFX_IL_TAG "\tparam.tex_inst %s\n", tex_inst);
            LOG_DBG(GFX_IL_TAG "\tparam.tex_filter %s\n", tex_filter);
            LOG_DBG(GFX_IL_TAG "\tparam.tex_wrap_mode[0] %s\n",
                    tex_wrap_mode[0]);
            LOG_DBG(GFX_IL_TAG "\tparam.tex_wrap_mode[1] %s\n",
                    tex_wrap_mode[1]);
            LOG_DBG(GFX_IL_TAG "\tparam.src_blend_factor %s\n",
                    src_blend_factor);
            LOG_DBG(GFX_IL_TAG "\tparam.dst_blend_factor %s\n",
                    dst_blend_factor);
            LOG_DBG(GFX_IL_TAG "\tparam.enable_depth_writes %s\n",
                    param->enable_depth_writes ? "true" : "false");
            LOG_DBG(GFX_IL_TAG "\tparam.depth_func %s\n", depth_func);
            LOG_DBG(GFX_IL_TAG "\tparam.pt_mode %s\n",
                    param->pt_mode ? "true" : "false");
            LOG_DBG(GFX_IL_TAG "\tparam.pt_ref %u\n", param->pt_ref);
        }
        break;
    case GFX_IL_SET_CLIP_RANGE:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_SET_CLIP_RANGE\n");
        LOG_DBG(GFX_IL_TAG "\tclip_min %f\n",
                cmd->arg.set_clip_range.clip_min);
        LOG_DBG(GFX_IL_TAG "\tclip_max %f\n",
                cmd->arg.set_clip_range.clip_max);
        break;
    case GFX_IL_DRAW_ARRAY:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_DRAW_ARRAY\n");
        LOG_DBG(GFX_IL_TAG "\tn_verts %u\n", cmd->arg.draw_array.n_verts);
        LOG_DBG(GFX_IL_TAG "\tverts %p\n", cmd->arg.draw_array.verts);
        break;
    case GFX_IL_INIT_OBJ:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_INIT_OBJ\n");
        LOG_DBG(GFX_IL_TAG "\tobj_no %d\n", cmd->arg.init_obj.obj_no);
        LOG_DBG(GFX_IL_TAG "\tn_bytes %u\n",
                (unsigned)cmd->arg.init_obj.n_bytes);
        break;
    case GFX_IL_WRITE_OBJ:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_WRITE_OBJ\n");
        LOG_DBG(GFX_IL_TAG "\tdat %p\n", cmd->arg.write_obj.dat);
        LOG_DBG(GFX_IL_TAG "\tobj_no %d\n", cmd->arg.write_obj.obj_no);
        LOG_DBG(GFX_IL_TAG "\tn_bytes %u\n",
                (unsigned)cmd->arg.write_obj.n_bytes);
        break;
    case GFX_IL_READ_OBJ:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_READ_OBJ\n");
        LOG_DBG(GFX_IL_TAG "\tdat %p\n", cmd->arg.read_obj.dat);
        LOG_DBG(GFX_IL_TAG "\tobj_no %d\n", cmd->arg.read_obj.obj_no);
        LOG_DBG(GFX_IL_TAG "\tn_bytes %u\n",
                (unsigned)cmd->arg.read_obj.n_bytes);
        break;
    case GFX_IL_FREE_OBJ:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_FREE_OBJ\n");
        LOG_DBG(GFX_IL_TAG "\tobj_no %d\n", cmd->arg.free_obj.obj_no);
        break;
    case GFX_IL_POST_FRAMEBUFFER:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_POST_FRAMEBUFFER\n");
        LOG_DBG(GFX_IL_TAG "\tobj_handle %d\n",
                cmd->arg.post_framebuffer.obj_handle);
        LOG_DBG(GFX_IL_TAG "\twidth %u\n", cmd->arg.post_framebuffer.width);
        LOG_DBG(GFX_IL_TAG "\theight %u\n", cmd->arg.post_framebuffer.height);
        LOG_DBG(GFX_IL_TAG "\tvert_flip %s\n",
                cmd->arg.post_framebuffer.vert_flip ? "true" : "false");
        LOG_DBG(GFX_IL_TAG "\tinterlaced %s\n",
                cmd->arg.post_framebuffer.interlaced ? "true" : "false");
        break;
    case GFX_IL_GRAB_FRAMEBUFFER:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_GRAB_FRAMEBUFFER\n");
        LOG_DBG(GFX_IL_TAG "\tfb %p\n", cmd->arg.grab_framebuffer.fb);
        break;
    case GFX_IL_BEGIN_DEPTH_SORT:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_BEGIN_DEPTH_SORT\n");
        break;
    case GFX_IL_END_DEPTH_SORT:
        LOG_DBG(GFX_IL_TAG " COMMAND GFX_IL_END_DEPTH_SORT\n");
        break;
    default:
        LOG_DBG(GFX_IL_TAG "UNKNOWN COMMAND %d\n", (int)cmd->op);
    }
}
#endif
