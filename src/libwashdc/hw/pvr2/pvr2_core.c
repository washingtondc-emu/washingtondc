/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
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

#include <limits.h>

#include "pvr2_core.h"
#include "pvr2.h"
#include "pvr2_reg.h"
#include "washdc/error.h"
#include "log.h"
#include "intmath.h"
#include "hw/sys/holly_intc.h"

#define PVR2_CORE_VERT_BUF_LEN (1024 * 1024)
#define PVR2_GFX_IL_INST_BUF_LEN (1024 * 256)

#define ISP_BACKGND_T_ADDR_SHIFT 1
#define ISP_BACKGND_T_ADDR_MASK (0x7ffffc << ISP_BACKGND_T_ADDR_SHIFT)

#define ISP_BACKGND_T_SKIP_SHIFT 24
#define ISP_BACKGND_T_SKIP_MASK (7 << ISP_BACKGND_T_SKIP_SHIFT)

/*
 * the delay between when the STARTRENDER command is received and when the
 * RENDER_COMPLETE interrupt gets raised.
 *
 * TODO: This value has no basis in reality.  I need to run some tests on real
 * hardware and come up with a good heuristic.
 *
 * If this value is too low, it will trigger race conditions in certain games
 * which can cause them to miss interrupts.
 */
#define PVR2_RENDER_COMPLETE_INT_DELAY (SCHED_FREQUENCY / 1024)

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event);

static DEF_ERROR_INT_ATTR(src_blend_factor);
static DEF_ERROR_INT_ATTR(dst_blend_factor);

static void
display_list_exec_header(struct pvr2 *pvr2,
                         struct pvr2_display_list_command const *cmd,
                         bool punch_through, bool blend_enable);
static void
display_list_exec_vertex(struct pvr2 *pvr2,
                         struct pvr2_display_list_command const *cmd);
static void
display_list_exec_quad(struct pvr2 *pvr2,
                       struct pvr2_display_list_command const *cmd);
static void
display_list_exec_end_of_group(struct pvr2 *pvr2,
                               struct pvr2_display_list_command const *cmd);

static inline void pvr2_core_push_vert(struct pvr2 *pvr2,
                                       struct pvr2_core_vert vert);
static inline void
pvr2_core_push_gfx_il(struct pvr2 *pvr2, struct gfx_il_inst inst);

static void render_frame_init(struct pvr2 *pvr2);

void pvr2_core_init(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;

    core->pvr2_render_complete_int_event.handler =
        pvr2_render_complete_int_event_handler;
    core->pvr2_render_complete_int_event.arg_ptr = pvr2;

    int list_idx;
    for (list_idx = 0; list_idx < PVR2_MAX_FRAMES_IN_FLIGHT; list_idx++)
        pvr2_display_list_init(core->disp_lists + list_idx);

    core->disp_list_counter = 0;

    core->pvr2_core_vert_buf = (float*)malloc(PVR2_CORE_VERT_BUF_LEN *
                                              sizeof(float) * GFX_VERT_LEN);
    if (!core->pvr2_core_vert_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    core->pvr2_core_vert_buf_count = 0;
    core->pvr2_core_vert_buf_start = 0;

    core->gfx_il_inst_buf = (struct gfx_il_inst*)malloc(PVR2_GFX_IL_INST_BUF_LEN *
                                                        sizeof(struct gfx_il_inst));
    if (!core->gfx_il_inst_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    render_frame_init(pvr2);
    core->pt_alpha_ref = 0xff;
}

void pvr2_core_cleanup(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;

    free(core->gfx_il_inst_buf);
    core->gfx_il_inst_buf = NULL;

    free(core->pvr2_core_vert_buf);
    core->pvr2_core_vert_buf = NULL;
}

static void render_frame_init(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;

    // free up gfx_il commands
    pvr2->core.gfx_il_inst_buf_count = 0;

    core->clip_min = -1.0f;
    core->clip_max = 1.0f;

    memset(&pvr2->stat.per_frame_counters, 0,
           sizeof(pvr2->stat.per_frame_counters));
}

void pvr2_display_list_init(struct pvr2_display_list *list) {
    list->valid = false;
    unsigned idx;
    for (idx = 0; idx < PVR2_POLY_TYPE_COUNT; idx++) {
        struct pvr2_display_list_group *group = list->poly_groups + idx;
        group->valid = false;
        group->n_cmds = 0;
    }
}

struct pvr2_display_list_command *
pvr2_list_alloc_new_cmd(struct pvr2_display_list *listp,
                        enum pvr2_poly_type poly_tp) {
    if (poly_tp >= PVR2_POLY_TYPE_COUNT || poly_tp < 0)
        RAISE_ERROR(ERROR_INTEGRITY); // protect against buffer overflow

    struct pvr2_display_list_group *group = listp->poly_groups + poly_tp;
    group->valid = true;

    if (group->n_cmds >= PVR2_DISPLAY_LIST_MAX_LEN) {
        // TODO: come up with a better solution than hardcoding a buffer length
        // ie some sort of pool/zone allocator might be a good idea
        LOG_ERROR("command capacity exceeded for display list!\n");
        return NULL;
    }

    return group->cmds + group->n_cmds++;
}

unsigned pvr2_list_age(struct pvr2 const *pvr2,
                       struct pvr2_display_list const *listp) {
    return pvr2->core.disp_list_counter - listp->age_counter;
}

/*
 * increment pvr2->ta.disp_list_counter.  If there's an integer overflow, then
 * the counter will be rolled back as far as possible and all display lists
 * will be adjusted accordingly.
 */
void pvr2_inc_age_counter(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;

#define PVR2_LIST_ROLLBACK_AGE_LIMIT (32 * 1024)
    if (++core->disp_list_counter >= UINT_MAX) {
        /*
         * roll back the odometer as far as we can to prevent integer overflow
         *
         * lists older than PVR2_LIST_ROLLBACK_AGE_LIMIT are marked as invalid
         * because otherwise what can happen is we end up with a really old list
         * that never gets overwritten and prevents us from rolling back the
         * odometer as far as we'd like.
         */
        unsigned oldest_age = UINT_MAX;
        unsigned disp_list_idx;
        for (disp_list_idx = 0; disp_list_idx < PVR2_MAX_FRAMES_IN_FLIGHT;
             disp_list_idx++) {
            struct pvr2_display_list *listp = core->disp_lists + disp_list_idx;
            if (listp->valid && listp->age_counter <= oldest_age &&
                pvr2_list_age(pvr2, listp) < PVR2_LIST_ROLLBACK_AGE_LIMIT) {
                oldest_age = listp->age_counter;
            }
        }

        if (oldest_age >= UINT_MAX) {
            // in case there was no list younger than PVR2_LIST_ROLLBACK_AGE_LIMIT
            for (disp_list_idx = 0; disp_list_idx < PVR2_MAX_FRAMES_IN_FLIGHT;
                 disp_list_idx++) {
                struct pvr2_display_list *listp = core->disp_lists + disp_list_idx;
                if (listp->valid) {
                    PVR2_TRACE("Display list %08X being marked as invalid due "
                               "to advanced age\n", (unsigned)listp->key);
                    listp->valid = false;
                }
            }
            core->disp_list_counter = 0;
        } else {
            /*
             * this is the normal case, where there was at least one list
             * younger than PVR2_LIST_ROLLBACK_AGE_LIMIT.
             */
            for (disp_list_idx = 0; disp_list_idx < PVR2_MAX_FRAMES_IN_FLIGHT;
                 disp_list_idx++) {
                struct pvr2_display_list *listp = core->disp_lists + disp_list_idx;
                if (listp->valid) {
                    if (pvr2_list_age(pvr2, listp) < PVR2_LIST_ROLLBACK_AGE_LIMIT) {
                        listp->age_counter -= oldest_age;
                    } else {
                        PVR2_TRACE("Display list %08X being marked as invalid due "
                                   "to advanced age\n", (unsigned)listp->key);
                        listp->valid = false;
                    }
                }
            }
            core->disp_list_counter -= oldest_age;
        }
    }
}

void
display_list_exec(struct pvr2 *pvr2, struct pvr2_display_list const *listp) {
    unsigned group_no;
    struct pvr2_core *core = &pvr2->core;

    // reset vertex array
    core->pvr2_core_vert_buf_count = 0;
    core->pvr2_core_vert_buf_start = 0;

    for (group_no = PVR2_POLY_TYPE_OPAQUE;
         group_no <= PVR2_POLY_TYPE_PUNCH_THROUGH; group_no++) {

        // TODO: implement modifier volumes
        if (group_no == PVR2_POLY_TYPE_OPAQUE_MOD ||
            group_no == PVR2_POLY_TYPE_TRANS_MOD)
            continue;

        struct pvr2_display_list_group const *group =
            listp->poly_groups + group_no;

        if (!group->valid)
            continue;

        pvr2->core.cur_poly_group = group_no;

        bool sort_mode = false;
        if ((group_no == PVR2_POLY_TYPE_TRANS) &&
            !(pvr2->reg_backing[PVR2_ISP_FEED_CFG] & 1)) {
            /*
             * order-independent transparency is enabled when bit 0 of
             * ISP_FEED_CFG is 0.
             */
            sort_mode = true;
            struct gfx_il_inst gfx_cmd;
            gfx_cmd.op = GFX_IL_BEGIN_DEPTH_SORT;
            pvr2_core_push_gfx_il(pvr2, gfx_cmd);
        }

        unsigned cmd_no;
        unsigned n_cmds = group->n_cmds;
        bool punch_through = (group_no == PVR2_POLY_TYPE_PUNCH_THROUGH);
        bool blend_enable = (group_no == PVR2_POLY_TYPE_TRANS);

        for (cmd_no = 0; cmd_no < n_cmds; cmd_no++) {
            struct pvr2_display_list_command const *cmd = group->cmds + cmd_no;
            switch (cmd->tp) {
            case PVR2_DISPLAY_LIST_COMMAND_TP_HEADER:
                display_list_exec_header(pvr2, cmd, punch_through, blend_enable);
                break;
            case PVR2_DISPLAY_LIST_COMMAND_TP_VERTEX:
                display_list_exec_vertex(pvr2, cmd);
                break;
            case PVR2_DISPLAY_LIST_COMMAND_TP_QUAD:
                display_list_exec_quad(pvr2, cmd);
                break;
            case PVR2_DISPLAY_LIST_COMMAND_TP_END_OF_GROUP:
                display_list_exec_end_of_group(pvr2, cmd);
                break;
            default:
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
        }

        if (sort_mode) {
            struct gfx_il_inst gfx_cmd;
            gfx_cmd.op = GFX_IL_END_DEPTH_SORT;
            pvr2_core_push_gfx_il(pvr2, gfx_cmd);
        }
    }
}

static void
display_list_exec_header(struct pvr2 *pvr2,
                         struct pvr2_display_list_command const *cmd,
                         bool punch_through, bool blend_enable) {
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_display_list_command_header const *cmd_hdr = &cmd->hdr;
    struct gfx_il_inst gfx_cmd;

#ifdef INVARIANTS
    if (core->pvr2_core_vert_buf_start > core->pvr2_core_vert_buf_count)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    if (core->pvr2_core_vert_buf_count != core->pvr2_core_vert_buf_start) {
        unsigned n_verts =
            core->pvr2_core_vert_buf_count - core->pvr2_core_vert_buf_start;
        gfx_cmd.op = GFX_IL_DRAW_ARRAY;
        gfx_cmd.arg.draw_array.n_verts = n_verts;
        gfx_cmd.arg.draw_array.verts = core->pvr2_core_vert_buf + core->pvr2_core_vert_buf_start * GFX_VERT_LEN;
        pvr2_core_push_gfx_il(pvr2, gfx_cmd);

        core->pvr2_core_vert_buf_start = core->pvr2_core_vert_buf_count;
    }

    if (cmd_hdr->tex_enable) {
        PVR2_TRACE("texture enabled\n");
        PVR2_TRACE("the texture format is %d\n", (int)cmd_hdr->pix_fmt);
        PVR2_TRACE("The texture address ix 0x%08x\n", cmd_hdr->tex_addr);

        if (cmd_hdr->tex_twiddle)
            PVR2_TRACE("not twiddled\n");
        else
            PVR2_TRACE("twiddled\n");

        unsigned linestride = cmd_hdr->stride_sel ?
            32 * (pvr2->reg_backing[PVR2_TEXT_CONTROL] & BIT_RANGE(0, 4)) :
            (1 << cmd_hdr->tex_width_shift);
        if (!linestride || linestride > (1 << cmd_hdr->tex_width_shift))
            RAISE_ERROR(ERROR_UNIMPLEMENTED);

        struct pvr2_tex *ent =
            pvr2_tex_cache_find(pvr2, cmd_hdr->tex_addr, cmd_hdr->tex_palette_start,
                                cmd_hdr->tex_width_shift,
                                cmd_hdr->tex_height_shift,
                                linestride,
                                cmd_hdr->pix_fmt, cmd_hdr->tex_twiddle,
                                cmd_hdr->tex_vq_compression,
                                cmd_hdr->tex_mipmap,
                                cmd_hdr->stride_sel);

        PVR2_TRACE("texture dimensions are (%u, %u)\n",
                   1 << cmd_hdr->tex_width_shift,
                   1 << cmd_hdr->tex_height_shift);
        if (ent) {
            PVR2_TRACE("Texture 0x%08x found in cache\n",
                       cmd_hdr->tex_addr);
        } else {
            PVR2_TRACE("Adding 0x%08x to texture cache...\n",
                       cmd_hdr->tex_addr);
            ent = pvr2_tex_cache_add(pvr2,
                                     cmd_hdr->tex_addr, cmd_hdr->tex_palette_start,
                                     cmd_hdr->tex_width_shift,
                                     cmd_hdr->tex_height_shift,
                                     linestride,
                                     cmd_hdr->pix_fmt,
                                     cmd_hdr->tex_twiddle,
                                     cmd_hdr->tex_vq_compression,
                                     cmd_hdr->tex_mipmap,
                                     cmd_hdr->stride_sel);
        }

        if (!ent) {
            LOG_WARN("WARNING: failed to add texture 0x%08x to "
                     "the texture cache\n", cmd_hdr->tex_addr);
            gfx_cmd.arg.set_rend_param.param.tex_enable = false;
        } else {
            unsigned tex_idx = pvr2_tex_cache_get_idx(pvr2, ent);
            gfx_cmd.arg.set_rend_param.param.tex_enable = true;
            gfx_cmd.arg.set_rend_param.param.tex_idx = tex_idx;
        }
    } else {
        gfx_cmd.arg.set_rend_param.param.tex_enable = false;
    }

    gfx_cmd.op = GFX_IL_SET_REND_PARAM;

    /*
     * this check is a little silly, but I get segfaults sometimes when
     * indexing into src_blend_factors and dst_blend_factors and I don't know
     * why.
     *
     * TODO: this was (hopefully) fixed in commit
     * 92059fe4f1714b914cec75fd2f91e676127d3097 but I am keeping the INVARIANTS
     * test here just in case.  It should be safe to delete after a couple of
     * months have gone by without this INVARIANTS test ever failing.
     */
    if (((unsigned)cmd_hdr->src_blend_factor >= PVR2_BLEND_FACTOR_COUNT) ||
        ((unsigned)cmd_hdr->dst_blend_factor >= PVR2_BLEND_FACTOR_COUNT)) {
        error_set_src_blend_factor(cmd_hdr->src_blend_factor);
        error_set_dst_blend_factor(cmd_hdr->dst_blend_factor);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    gfx_cmd.arg.set_rend_param.param.src_blend_factor =
        cmd_hdr->src_blend_factor;
    gfx_cmd.arg.set_rend_param.param.dst_blend_factor =
        cmd_hdr->dst_blend_factor;
    gfx_cmd.arg.set_rend_param.param.tex_wrap_mode[0] =
        cmd_hdr->tex_wrap_mode[0];
    gfx_cmd.arg.set_rend_param.param.tex_wrap_mode[1] =
        cmd_hdr->tex_wrap_mode[1];

    gfx_cmd.arg.set_rend_param.param.enable_depth_writes =
        cmd_hdr->enable_depth_writes;
    gfx_cmd.arg.set_rend_param.param.depth_func = cmd_hdr->depth_func;

    gfx_cmd.arg.set_rend_param.param.tex_inst = cmd_hdr->tex_inst;
    gfx_cmd.arg.set_rend_param.param.tex_filter = cmd_hdr->tex_filter;

    gfx_cmd.arg.set_rend_param.param.pt_mode = punch_through;
    gfx_cmd.arg.set_rend_param.param.pt_ref = core->pt_alpha_ref & 0xff;

    // enqueue the configuration command
    pvr2_core_push_gfx_il(pvr2, gfx_cmd);

    // TODO: this only needs to be done once per group, not once per polygon group
    gfx_cmd.op = GFX_IL_SET_BLEND_ENABLE;
    gfx_cmd.arg.set_blend_enable.do_enable = blend_enable;
    pvr2_core_push_gfx_il(pvr2, gfx_cmd);

    core->strip_len = 0;
    pvr2->core.stride_sel = cmd_hdr->stride_sel;
    pvr2->core.tex_width_shift = cmd_hdr->tex_width_shift;
    pvr2->core.tex_height_shift = cmd_hdr->tex_height_shift;
}

static void
display_list_exec_vertex(struct pvr2 *pvr2,
                         struct pvr2_display_list_command const *cmd) {
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_display_list_vertex const *cmd_vtx = &cmd->vtx;

    /*
     * un-strip triangle strips by duplicating the previous two vertices.
     *
     * TODO: obviously it would be best to preserve the triangle strips and
     * send them to OpenGL via GL_TRIANGLE_STRIP in the rendering backend, but
     * then I need to come up with some way to signal the renderer to stop and
     * re-start strips.  It might also be possible to stitch separate strips
     * together with degenerate triangles...
     */
    if (core->strip_len >= 3) {
        pvr2_core_push_vert(pvr2, core->strip_vert_1);
        pvr2_core_push_vert(pvr2, core->strip_vert_2);
    }

    // first update the clipping planes
    /*
     * TODO: there are FPU instructions on x86 that can do this without
     * branching
     */
    float z_recip = 1.0 / cmd_vtx->pos[2];
    if (z_recip < core->clip_min)
        core->clip_min = z_recip;
    if (z_recip > core->clip_max)
        core->clip_max = z_recip;

    struct pvr2_core_vert vert;

    vert.pos[0] = cmd_vtx->pos[0];
    vert.pos[1] = cmd_vtx->pos[1];
    vert.pos[2] = cmd_vtx->pos[2];

    PVR2_TRACE("(%f, %f, %f)\n", vert.pos[0], vert.pos[1], vert.pos[2]);

    memcpy(vert.base_color, cmd_vtx->base_color, sizeof(cmd_vtx->base_color));
    memcpy(vert.offs_color, cmd_vtx->offs_color, sizeof(cmd_vtx->offs_color));

    if (pvr2->core.stride_sel) {
        unsigned linestride =
            32 * (pvr2->reg_backing[PVR2_TEXT_CONTROL] & BIT_RANGE(0, 4));vert.tex_coord[0] =
            cmd_vtx->tex_coord[0] * ((float)(1 << pvr2->core.tex_width_shift) /
                          (float)linestride);
        vert.tex_coord[1] = cmd_vtx->tex_coord[1];
    } else {
        vert.tex_coord[0] = cmd_vtx->tex_coord[0];
        vert.tex_coord[1] = cmd_vtx->tex_coord[1];
    }

    pvr2_core_push_vert(pvr2, vert);

    if (cmd_vtx->end_of_strip) {
        /*
         * TODO: handle degenerate cases where the user sends an
         * end-of-strip on the first or second vertex
         */
        core->strip_len = 0;
    } else {
        /*
         * shift the new vert into strip_vert2 and
         * shift strip_vert2 into strip_vert1
         */
        core->strip_vert_1 = core->strip_vert_2;
        core->strip_vert_2 = vert;
        core->strip_len++;
    }

    pvr2->stat.per_frame_counters.vert_count[pvr2->core.cur_poly_group]++;
}

static void
display_list_exec_quad(struct pvr2 *pvr2,
                       struct pvr2_display_list_command const *cmd) {
    struct pvr2_core *core = &pvr2->core;
    struct pvr2_display_list_quad const *cmd_quad = &cmd->quad;

    if (cmd_quad->degenerate)
        return;

    /*
     * unpack the texture coordinates.  The third vertex's coordinate is the
     * scond vertex's coordinate plus the two side-vectors.  We do this
     * unconditionally even if textures are disabled.  If textures are disabled
     * then the output of this texture-coordinate algorithm is undefined but it
     * does not matter because the rendering code won't be using it anyways.
     */
    float vert_tex_coords[4][2];
    unpack_uv16(vert_tex_coords[0], vert_tex_coords[0] + 1,
                cmd_quad->tex_coords_packed);
    unpack_uv16(vert_tex_coords[1], vert_tex_coords[1] + 1,
                cmd_quad->tex_coords_packed + 1);
    unpack_uv16(vert_tex_coords[2], vert_tex_coords[2] + 1,
                cmd_quad->tex_coords_packed + 2);

    float uv_vec[2][2] = {
        { vert_tex_coords[0][0] - vert_tex_coords[1][0],
          vert_tex_coords[0][1] - vert_tex_coords[1][1] },
        { vert_tex_coords[2][0] - vert_tex_coords[1][0],
          vert_tex_coords[2][1] - vert_tex_coords[1][1] }
    };

    vert_tex_coords[3][0] =
        vert_tex_coords[1][0] + uv_vec[0][0] + uv_vec[1][0];
    vert_tex_coords[3][1] =
        vert_tex_coords[1][1] + uv_vec[0][1] + uv_vec[1][1];

    if (pvr2->core.stride_sel) {
        // non-power-of-two texture
        unsigned linestride =
            32 * (pvr2->reg_backing[PVR2_TEXT_CONTROL] & BIT_RANGE(0, 4));
        int idx;
        for (idx = 0; idx < 3; idx++) {
            vert_tex_coords[idx][0] *=
                ((float)linestride) / ((float)(1 << pvr2->core.tex_width_shift));
        }
    }

    float const base_col[] = {
        cmd_quad->base_color[0],
        cmd_quad->base_color[1],
        cmd_quad->base_color[2],
        cmd_quad->base_color[3]
    };

    float const offs_col[] = {
        cmd_quad->offs_color[0],
        cmd_quad->offs_color[1],
        cmd_quad->offs_color[2],
        cmd_quad->offs_color[3]
    };

    float const *p1 = cmd_quad->vert_pos[0];
    float const *p2 = cmd_quad->vert_pos[1];
    float const *p3 = cmd_quad->vert_pos[2];
    float const *p4 = cmd_quad->vert_pos[3];

    struct pvr2_core_vert vert1 = {
        .pos = { p1[0], p1[1], 1.0f / p1[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { vert_tex_coords[0][0], vert_tex_coords[0][1] }
    };

    struct pvr2_core_vert vert2 = {
        .pos = { p2[0], p2[1], 1.0f / p2[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { vert_tex_coords[1][0], vert_tex_coords[1][1] }
    };

    struct pvr2_core_vert vert3 = {
        .pos = { p3[0], p3[1], 1.0f / p3[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { vert_tex_coords[2][0], vert_tex_coords[2][1] }
    };

    struct pvr2_core_vert vert4 = {
        .pos = { p4[0], p4[1], 1.0f / p4[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { vert_tex_coords[3][0], vert_tex_coords[3][1] }
    };

    pvr2_core_push_vert(pvr2, vert1);
    pvr2_core_push_vert(pvr2, vert2);
    pvr2_core_push_vert(pvr2, vert3);

    pvr2_core_push_vert(pvr2, vert1);
    pvr2_core_push_vert(pvr2, vert3);
    pvr2_core_push_vert(pvr2, vert4);

    if (p1[2] < core->clip_min)
        core->clip_min = p1[2];
    if (p1[2] > core->clip_max)
        core->clip_max = p1[2];

    if (p2[2] < core->clip_min)
        core->clip_min = p2[2];
    if (p2[2] > core->clip_max)
        core->clip_max = p2[2];

    if (p3[2] < core->clip_min)
        core->clip_min = p3[2];
    if (p3[2] > core->clip_max)
        core->clip_max = p3[2];

    if (p4[2] < core->clip_min)
        core->clip_min = p4[2];
    if (p4[2] > core->clip_max)
        core->clip_max = p4[2];

    pvr2->stat.per_frame_counters.vert_count[pvr2->core.cur_poly_group]++;
}

static void
display_list_exec_end_of_group(struct pvr2 *pvr2,
                               struct pvr2_display_list_command const *cmd) {
    struct gfx_il_inst gfx_cmd;
    struct pvr2_core *core = &pvr2->core;
    unsigned n_verts = core->pvr2_core_vert_buf_count - core->pvr2_core_vert_buf_start;

#ifdef INVARIANTS
    if (core->pvr2_core_vert_buf_start > core->pvr2_core_vert_buf_count)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    if (n_verts) {
        gfx_cmd.op = GFX_IL_DRAW_ARRAY;
        gfx_cmd.arg.draw_array.n_verts = n_verts;
        gfx_cmd.arg.draw_array.verts = core->pvr2_core_vert_buf + core->pvr2_core_vert_buf_start * GFX_VERT_LEN;
        pvr2_core_push_gfx_il(pvr2, gfx_cmd);
        core->pvr2_core_vert_buf_start = core->pvr2_core_vert_buf_count;
    }
}

static inline void pvr2_core_push_vert(struct pvr2 *pvr2, struct pvr2_core_vert vert) {
    struct pvr2_core *core = &pvr2->core;
    if (core->pvr2_core_vert_buf_count >= PVR2_CORE_VERT_BUF_LEN) {
        LOG_WARN("PVR2 CORE vertex buffer overflow\n");
        return;
    }

    float *outp = core->pvr2_core_vert_buf +
        GFX_VERT_LEN * core->pvr2_core_vert_buf_count++;
    PVR2_TRACE("vert_buf_count is now %u\n", core->pvr2_core_vert_buf_count);
    outp[GFX_VERT_POS_OFFSET + 0] = vert.pos[0];
    outp[GFX_VERT_POS_OFFSET + 1] = vert.pos[1];
    outp[GFX_VERT_POS_OFFSET + 2] = vert.pos[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 0] = vert.base_color[0];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 1] = vert.base_color[1];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 2] = vert.base_color[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 3] = vert.base_color[3];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 0] = vert.offs_color[0];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 1] = vert.offs_color[1];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 2] = vert.offs_color[2];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 3] = vert.offs_color[3];
    outp[GFX_VERT_TEX_COORD_OFFSET + 0] = vert.tex_coord[0];
    outp[GFX_VERT_TEX_COORD_OFFSET + 1] = vert.tex_coord[1];
}

static inline void
pvr2_core_push_gfx_il(struct pvr2 *pvr2, struct gfx_il_inst inst) {
    struct pvr2_core *core = &pvr2->core;

    if (core->gfx_il_inst_buf_count >= PVR2_GFX_IL_INST_BUF_LEN)
        RAISE_ERROR(ERROR_OVERFLOW);

    core->gfx_il_inst_buf[core->gfx_il_inst_buf_count++] = inst;
}

void pvr2_ta_startrender(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;
    struct gfx_il_inst cmd;

    render_frame_init(pvr2);

    /*
     * Algorithm here is to find the youngest display list which is within a
     * certain range of where PVR2_PARAM_BASE points.  The reason for this is
     * that in Resident Evil 2 (and probably other Windows games as well) the
     * TA_OL_BASE is offset by 0x27280 from the PARAM_BASE register.  I'm not
     * sure why exactly that is but these sorts of issues are to be expected
     * with HLE.
     */
    uint32_t key = pvr2->reg_backing[PVR2_PARAM_BASE];
    PVR2_TRACE("STARTRENDER requested!  key is %08X\n", (unsigned)key);
    struct pvr2_display_list *listp = NULL;
    unsigned list_no;
    for (list_no = 0; list_no < PVR2_MAX_FRAMES_IN_FLIGHT; list_no++) {
        if (core->disp_lists[list_no].valid &&
            key <= core->disp_lists[list_no].key &&
            (core->disp_lists[list_no].key - key) < 0x00100000) {
            if (!listp ||
                pvr2_list_age(pvr2, core->disp_lists + list_no) <
                pvr2_list_age(pvr2, listp)) {
                listp = core->disp_lists + list_no;
            }
        }
    }

    if (listp) {
        unsigned age;
        if ((age = pvr2_list_age(pvr2, listp)) > 32) {
            /*
             * warn if the list is old.  This could be legitimately
             * correct behavior, but it could also mean that the list used by
             * the TA to generate the list somehow did not match up with the
             * list key used by the CORE to render the list, and we ended up
             * rendering the wrong list because the one that CORE used happened
             * to match a list that actually exists.
             */
            LOG_WARN("PVR2 display list age is %u; possible list mismatch\n",
                     age);
        } else {
            PVR2_TRACE("PVR2 display list age is %u\n", age);
        }

        /*
         * incrememnt the age counter.  The purpose of this is so that lists
         * which are created once but used often don't get old.
         *
         * e.g. one potential example is that you have a game which displays a
         * quad containing a texture which represents a software-rendered
         * framebuffer (as many emulators and 2D game engines do).  You might
         * only generate the display list once and then render it with an
         * updated texture every frame since the vertices never change.  In
         * that situation we don't want WashingtonDC to think the display list
         * is old and outdated just because it was generated a long time ago.
         */
        pvr2_inc_age_counter(pvr2);
        listp->age_counter = core->disp_list_counter;

        display_list_exec(pvr2, listp);
        pvr2_tex_cache_xmit(pvr2);
    } else {
        LOG_ERROR("PVR2 unable to locate display list for key %08X\n",
                  (unsigned)key);
    }

    unsigned tile_w = get_glob_tile_clip_x(pvr2) << 5;
    unsigned tile_h = get_glob_tile_clip_y(pvr2) << 5;
    unsigned x_clip_min = get_fb_x_clip_min(pvr2);
    unsigned x_clip_max = get_fb_x_clip_max(pvr2);
    unsigned y_clip_min = get_fb_y_clip_min(pvr2);
    unsigned y_clip_max = get_fb_y_clip_max(pvr2);

    unsigned x_min = x_clip_min;
    unsigned y_min = y_clip_min;
    unsigned x_max = tile_w < x_clip_max ? tile_w : x_clip_max;
    unsigned y_max = tile_h < y_clip_max ? tile_h : y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    /*
     * backgnd_info points to a structure containing some ISP/TSP parameters
     * and three vertices (potentially including texture coordinate and
     * color data).  These are used to draw a background plane.  isp_backgnd_d
     * contians some sort of depth value which is used in auto-sorting mode (I
     * think?).
     *
     * Obviously, I I don't actually understand how this works, nor do I
     * understand why the vertex coordinates are relevant when it's just going
     * to draw an infinite plane, so I just save the background color from the
     * first vertex in the geo_buf so the renderer can use it to glClear.  I
     * also save the depth value from isp_backgnd_d even though I don't have
     * auto-sorting implemented yet.
     *
     * This hack inspired by MAME's powervr2 code.
     */
    uint32_t backgnd_tag = get_isp_backgnd_t(pvr2);
    addr32_t backgnd_info_addr = (backgnd_tag & ISP_BACKGND_T_ADDR_MASK) >>
        ISP_BACKGND_T_ADDR_SHIFT;
    uint32_t backgnd_skip = ((ISP_BACKGND_T_SKIP_MASK & backgnd_tag) >>
                             ISP_BACKGND_T_SKIP_SHIFT) + 3;
    /* printf("background skip is %d\n", (int)backgnd_skip); */

    /* printf("ISP_BACKGND_D is %f\n", (double)frak); */
    /* printf("ISP_BACKGND_T is 0x%08x\n", (unsigned)backgnd_tag); */

    uint32_t bg_color_src =
        pvr2_tex_mem_32bit_read32(pvr2, backgnd_info_addr +
                                  (3 + 0 * backgnd_skip + 3) * sizeof(uint32_t));

    float bg_color_a = (float)((bg_color_src & 0xff000000) >> 24) / 255.0f;
    float bg_color_r = (float)((bg_color_src & 0x00ff0000) >> 16) / 255.0f;
    float bg_color_g = (float)((bg_color_src & 0x0000ff00) >> 8) / 255.0f;
    float bg_color_b = (float)((bg_color_src & 0x000000ff) >> 0) / 255.0f;
    core->pvr2_bgcolor[0] = bg_color_r;
    core->pvr2_bgcolor[1] = bg_color_g;
    core->pvr2_bgcolor[2] = bg_color_b;
    core->pvr2_bgcolor[3] = bg_color_a;

    /* uint32_t backgnd_depth_as_int = get_isp_backgnd_d(); */
    /* memcpy(&geo->bgdepth, &backgnd_depth_as_int, sizeof(float)); */

    int tgt = framebuffer_set_render_target(pvr2);

    /*
     * This is really driving me insane and I don't know what to do about it.
     *
     * A number of games will use different resolutions when reading from the
     * framebuffer than they will when writing to it.  Most of the time it's
     * just a couple of extra rows which isn't that big of a deal, but in
     * SoulCalibur's case there's also an extra column.  Lobbing pixels off of
     * a texture isn't so easy to do in OpenGL, so IDK what to do.
     */
    unsigned read_width, read_height;
    framebuffer_get_render_target_dims(pvr2, tgt, &read_width, &read_height);
    if (read_width != width || read_height != height) {
        /*
         * Also I suspect that the read-width needs to be doubled because it's
         * always half what I expect it to be.  That's fairly reasonably and
         * not nearly as exasperating as the case described above.
         */
        LOG_DBG("Warning: read-dimensions of framebuffer are %ux%u, but "
                "write-dimensions are %ux%u\n",
                read_width, read_height, width, height);
    }

    /*
     * TODO: This is extremely inaccurate.  PVR2 only draws on a per-tile
     * basis; I think that includes clearing the framebuffer on a per-tile
     * basis as well.
     */

    // set up rendering context
    cmd.op = GFX_IL_BEGIN_REND;
    cmd.arg.begin_rend.screen_width = width;
    cmd.arg.begin_rend.screen_height = height;
    cmd.arg.begin_rend.rend_tgt_obj = tgt;
    rend_exec_il(&cmd, 1);

    cmd.op = GFX_IL_SET_CLIP_RANGE;
    cmd.arg.set_clip_range.clip_min = core->clip_min;
    cmd.arg.set_clip_range.clip_max = core->clip_max;
    rend_exec_il(&cmd, 1);

    // initial rendering settings
    cmd.op = GFX_IL_CLEAR;
    cmd.arg.clear.bgcolor[0] = core->pvr2_bgcolor[0];
    cmd.arg.clear.bgcolor[1] = core->pvr2_bgcolor[1];
    cmd.arg.clear.bgcolor[2] = core->pvr2_bgcolor[2];
    cmd.arg.clear.bgcolor[3] = core->pvr2_bgcolor[3];
    rend_exec_il(&cmd, 1);

    // execute queued gfx_il commands
    rend_exec_il(core->gfx_il_inst_buf, core->gfx_il_inst_buf_count);

    // tear down rendering context
    cmd.op = GFX_IL_END_REND;
    cmd.arg.end_rend.rend_tgt_obj = tgt;
    rend_exec_il(&cmd, 1);

    core->next_frame_stamp++;

    if (!core->pvr2_render_complete_int_event_scheduled) {
        struct dc_clock *clk = pvr2->clk;
        core->pvr2_render_complete_int_event_scheduled = true;
        core->pvr2_render_complete_int_event.when = clock_cycle_stamp(clk) +
            PVR2_RENDER_COMPLETE_INT_DELAY;
        sched_event(clk, &core->pvr2_render_complete_int_event);
    }
}

unsigned get_cur_frame_stamp(struct pvr2 *pvr2) {
    return pvr2->core.next_frame_stamp;
}

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event) {
    struct pvr2_core *core = &((struct pvr2*)event->arg_ptr)->core;
    core->pvr2_render_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}
