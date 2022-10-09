/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020, 2022 snickerbockers
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

#include <limits.h>
#include <math.h>

#include "pvr2_core.h"
#include "pvr2.h"
#include "pvr2_reg.h"
#include "washdc/error.h"
#include "log.h"
#include "intmath.h"
#include "hw/sys/holly_intc.h"

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
display_list_exec_quad(struct pvr2 *pvr2,
                       struct pvr2_display_list_command const *cmd);

static void
display_list_exec_user_clip(struct pvr2 *pvr2,
                            struct pvr2_display_list_command const *cmd);
static void
display_list_exec_tri_strip(struct pvr2 *pvr2,
                            struct pvr2_display_list_command const *cmd);

static inline void
pvr2_core_push_gfx_il(struct pvr2 *pvr2, struct gfx_il_inst inst);

static void render_frame_init(struct pvr2 *pvr2);

void pvr2_core_init(struct pvr2 *pvr2) {
    struct pvr2_core *core = &pvr2->core;

    core->pvr2_render_complete_int_event.handler =
        pvr2_render_complete_int_event_handler;
    core->pvr2_render_complete_int_event.arg_ptr = pvr2;

    int list_idx;
    for (list_idx = 0; list_idx < PVR2_MAX_FRAMES_IN_FLIGHT; list_idx++) {
        struct pvr2_display_list *disp_list = core->disp_lists + list_idx;
        int group_idx;

        disp_list->vert_array = malloc(PVR2_DISPLAY_LIST_MAX_VERTS *
                                       sizeof(float) * GFX_VERT_LEN);
        if (!disp_list->vert_array)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        for (group_idx = 0; group_idx < PVR2_POLY_TYPE_COUNT; group_idx++) {
            disp_list->poly_groups[group_idx].cmds =
                malloc(sizeof(struct pvr2_display_list_command) *
                       PVR2_DISPLAY_LIST_MAX_LEN);
        }
        pvr2_display_list_init(disp_list);
    }

    core->disp_list_counter = 0;

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

    int list_idx;
    for (list_idx = 0; list_idx < PVR2_MAX_FRAMES_IN_FLIGHT; list_idx++) {
        struct pvr2_display_list *disp_list = core->disp_lists + list_idx;
        int group_idx;
        for (group_idx = 0; group_idx < PVR2_POLY_TYPE_COUNT; group_idx++)
            free(disp_list->poly_groups[group_idx].cmds);
        free(disp_list->vert_array);
    }
}

static void render_frame_init(struct pvr2 *pvr2) {
    // free up gfx_il commands
    pvr2->core.gfx_il_inst_buf_count = 0;

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

    list->n_verts = 0;
    list->clip_min = 0.0f;
    list->clip_max = 0.0f;
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
            case PVR2_DISPLAY_LIST_COMMAND_TP_QUAD:
                display_list_exec_quad(pvr2, cmd);
                break;
            case PVR2_DISPLAY_LIST_COMMAND_TP_USER_CLIP:
                display_list_exec_user_clip(pvr2, cmd);
                break;
            case PVR2_DISPLAY_LIST_COMMAND_TP_TRI_STRIP:
                display_list_exec_tri_strip(pvr2, cmd);
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

    switch (cmd_hdr->user_clip_mode) {
    case PVR2_USER_CLIP_INSIDE:
        gfx_cmd.arg.set_rend_param.param.user_clip_mode = GFX_USER_CLIP_INSIDE;
        break;
    case PVR2_USER_CLIP_OUTSIDE:
        gfx_cmd.arg.set_rend_param.param.user_clip_mode = GFX_USER_CLIP_OUTSIDE;
        break;
    case PVR2_USER_CLIP_DISABLE:
    case PVR2_USER_CLIP_RESERVED:
    default:
        gfx_cmd.arg.set_rend_param.param.user_clip_mode = GFX_USER_CLIP_DISABLE;
        break;
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

    switch (cmd_hdr->cull_mode) {
    case 1:
        gfx_cmd.arg.set_rend_param.param.cull_mode = GFX_CULL_SMALL;
        break;
    case 2:
        gfx_cmd.arg.set_rend_param.param.cull_mode = GFX_CULL_NEGATIVE;
        break;
    case 3:
        gfx_cmd.arg.set_rend_param.param.cull_mode = GFX_CULL_POSITIVE;
        break;
    default:
    case 0:
        gfx_cmd.arg.set_rend_param.param.cull_mode = GFX_CULL_DISABLE;
        break;
    }
    memcpy(&gfx_cmd.arg.set_rend_param.param.cull_bias,
           pvr2->reg_backing + PVR2_FPU_CULL_VAL,
           sizeof(gfx_cmd.arg.set_rend_param.param.cull_bias));

    gfx_cmd.arg.set_rend_param.param.tex_inst = cmd_hdr->tex_inst;
    gfx_cmd.arg.set_rend_param.param.tex_filter = cmd_hdr->tex_filter;

    gfx_cmd.arg.set_rend_param.param.pt_mode = punch_through;
    gfx_cmd.arg.set_rend_param.param.pt_ref = core->pt_alpha_ref & 0xff;

    float *tex_transform = gfx_cmd.arg.set_rend_param.param.tex_transform;
    if (cmd_hdr->stride_sel) {
        unsigned linestride =
            32 * (pvr2->reg_backing[PVR2_TEXT_CONTROL] & BIT_RANGE(0, 4));
        tex_transform[0] = (float)(1 << cmd_hdr->tex_width_shift) /
            (float)linestride;
    } else {
        tex_transform[0] = 1.0f;
    }
    tex_transform[1] = 0.0f;
    tex_transform[2] = 0.0f;
    tex_transform[3] = 1.0f;

    // enqueue the configuration command
    pvr2_core_push_gfx_il(pvr2, gfx_cmd);

    // TODO: this only needs to be done once per group, not once per polygon group
    gfx_cmd.op = GFX_IL_SET_BLEND_ENABLE;
    gfx_cmd.arg.set_blend_enable.do_enable = blend_enable;
    pvr2_core_push_gfx_il(pvr2, gfx_cmd);

    pvr2->core.stride_sel = cmd_hdr->stride_sel;
    pvr2->core.tex_width_shift = cmd_hdr->tex_width_shift;
    pvr2->core.tex_height_shift = cmd_hdr->tex_height_shift;
}

static void
display_list_exec_quad(struct pvr2 *pvr2,
                       struct pvr2_display_list_command const *cmd) {
    struct gfx_il_inst gfx_cmd;
    gfx_cmd.op = GFX_IL_DRAW_VERT_ARRAY;
    gfx_cmd.arg.draw_vert_array.first_idx = cmd->quad.first_vtx;
    gfx_cmd.arg.draw_vert_array.n_verts = 4;
    pvr2_core_push_gfx_il(pvr2, gfx_cmd);
}

static void
display_list_exec_user_clip(struct pvr2 *pvr2,
                            struct pvr2_display_list_command const *cmd) {
    struct gfx_il_inst gfx_cmd;

    gfx_cmd.op = GFX_IL_SET_USER_CLIP;
    gfx_cmd.arg.set_user_clip.x_min = cmd->user_clip.x_min * 32;
    gfx_cmd.arg.set_user_clip.y_min = cmd->user_clip.y_min * 32;
    gfx_cmd.arg.set_user_clip.x_max = cmd->user_clip.x_max * 32 + 31;
    gfx_cmd.arg.set_user_clip.y_max = cmd->user_clip.y_max * 32 + 31;

    pvr2_core_push_gfx_il(pvr2, gfx_cmd);
}

static void
display_list_exec_tri_strip(struct pvr2 *pvr2,
                            struct pvr2_display_list_command const *cmd) {
    unsigned n_verts = cmd->strip.vtx_count;

    if (n_verts) {
        struct gfx_il_inst gfx_cmd;
        gfx_cmd.op = GFX_IL_DRAW_VERT_ARRAY;
        gfx_cmd.arg.draw_vert_array.first_idx = cmd->strip.first_vtx;
        gfx_cmd.arg.draw_vert_array.n_verts = n_verts;
        pvr2_core_push_gfx_il(pvr2, gfx_cmd);
    }
}

static inline void
pvr2_core_push_gfx_il(struct pvr2 *pvr2, struct gfx_il_inst inst) {
    struct pvr2_core *core = &pvr2->core;

    PVR2_TRACE("%s - ENQUEUE %s\n", __func__, gfx_il_op_name(inst.op));

    if (core->gfx_il_inst_buf_count >= PVR2_GFX_IL_INST_BUF_LEN)
        RAISE_ERROR(ERROR_OVERFLOW);

    core->gfx_il_inst_buf[core->gfx_il_inst_buf_count++] = inst;
}

static DEF_ERROR_INT_ATTR(screen_width)
static DEF_ERROR_INT_ATTR(screen_height)
static DEF_ERROR_INT_ATTR(x_clip_min)
static DEF_ERROR_INT_ATTR(y_clip_min)
static DEF_ERROR_INT_ATTR(x_clip_max)
static DEF_ERROR_INT_ATTR(y_clip_max)

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
    uint32_t key = pvr2->reg_backing[PVR2_PARAM_BASE] & PVR2_DISPLAY_LIST_KEY_MASK;
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

    /*
     * XXX glob_tile_clip is supposed to apply to TA display list creation
     * and not core rendering, so it's definitely not correct to be referencing
     * it here but IDK how else to get framebuffer dimensions so we do it
     * anyways.
     *
     * Note that if this ever gets fixed, then framebuffer.c may need to be
     * updated as well
     */
    unsigned screen_width = get_glob_tile_clip_x(pvr2) << 5;
    unsigned screen_height = get_glob_tile_clip_y(pvr2) << 5;
    unsigned x_clip_min = get_fb_x_clip_min(pvr2);
    unsigned x_clip_max = get_fb_x_clip_max(pvr2);
    unsigned y_clip_min = get_fb_y_clip_min(pvr2);
    unsigned y_clip_max = get_fb_y_clip_max(pvr2);

    if (x_clip_max >= screen_width)
        x_clip_max = screen_width - 1;
    if (y_clip_max >= screen_height)
        y_clip_max = screen_height - 1;

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
    if (read_width != screen_width || read_height != screen_height) {
        /*
         * Also I suspect that the read-width needs to be doubled because it's
         * always half what I expect it to be.  That's fairly reasonably and
         * not nearly as exasperating as the case described above.
         */
        LOG_DBG("Warning: read-dimensions of framebuffer are %ux%u, but "
                "write-dimensions are %ux%u\n",
                read_width, read_height, screen_width, screen_height);
    }

    /*
     * TODO: This is extremely inaccurate.  PVR2 only draws on a per-tile
     * basis; I think that includes clearing the framebuffer on a per-tile
     * basis as well.
     */

    // set up rendering context
    cmd.op = GFX_IL_BEGIN_REND;
    cmd.arg.begin_rend.screen_width = screen_width;
    cmd.arg.begin_rend.screen_height = screen_height;
    cmd.arg.begin_rend.clip[0] = x_clip_min;
    cmd.arg.begin_rend.clip[1] = y_clip_min;
    cmd.arg.begin_rend.clip[2] = x_clip_max;
    cmd.arg.begin_rend.clip[3] = y_clip_max;
    cmd.arg.begin_rend.rend_tgt_obj = tgt;
    rend_exec_il(&cmd, 1);

    cmd.op = GFX_IL_SET_CLIP_RANGE;
    if (listp) {
        cmd.arg.set_clip_range.clip_min = listp->clip_min;
        cmd.arg.set_clip_range.clip_max = listp->clip_max;
    } else {
        cmd.arg.set_clip_range.clip_min = 0.0f;
        cmd.arg.set_clip_range.clip_max = 0.0f;
    }
    rend_exec_il(&cmd, 1);

    // initial rendering settings
    cmd.op = GFX_IL_CLEAR;
    cmd.arg.clear.bgcolor[0] = core->pvr2_bgcolor[0];
    cmd.arg.clear.bgcolor[1] = core->pvr2_bgcolor[1];
    cmd.arg.clear.bgcolor[2] = core->pvr2_bgcolor[2];
    cmd.arg.clear.bgcolor[3] = core->pvr2_bgcolor[3];
    rend_exec_il(&cmd, 1);

    // load vertex data
    if (listp) {
        cmd.op = GFX_IL_SET_VERT_ARRAY;
        cmd.arg.set_vert_array.n_verts = listp->n_verts;
        cmd.arg.set_vert_array.verts = listp->vert_array;
        rend_exec_il(&cmd, 1);

        // execute queued gfx_il commands
        rend_exec_il(core->gfx_il_inst_buf, core->gfx_il_inst_buf_count);
    }

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
    struct pvr2 *pvr2 = (struct pvr2*)event->arg_ptr;
    pvr2->core.pvr2_render_complete_int_event_scheduled = false;
    pvr2->irq_callback(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}
