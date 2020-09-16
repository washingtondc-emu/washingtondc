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

#include "washdc/gfx/gfx_all.h"
#include "washdc/gfx/def.h"

#include "soft_gfx.h"

static struct soft_gfx_callbacks const *switch_table;

static void soft_gfx_init(void);
static void soft_gfx_cleanup(void);
static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd);

struct rend_if const soft_gfx_if = {
    .init = soft_gfx_init,
    .cleanup = soft_gfx_cleanup,
    .exec_gfx_il = soft_gfx_exec_gfx_il
};

static void soft_gfx_init(void) {
}

static void soft_gfx_cleanup(void) {
}

void soft_gfx_set_callbacks(struct soft_gfx_callbacks const *callbacks) {
    switch_table = callbacks;
}

static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            printf("GFX_IL_BIND_TEX\n");
            break;
        case GFX_IL_UNBIND_TEX:
            printf("GFX_IL_UNBIND_TEX\n");
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            printf("GFX_IL_BIND_RENDER_TARGET\n");
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            printf("GFX_IL_UNBIND_RENDER_TARGET\n");
            break;
        case GFX_IL_BEGIN_REND:
            printf("GFX_IL_BEGIN_REND\n");
            break;
        case GFX_IL_END_REND:
            printf("GFX_IL_END_REND\n");
            break;
        case GFX_IL_CLEAR:
            printf("GFX_IL_CLEAR\n");
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            printf("GFX_IL_SET_BLEND_ENABLE\n");
            break;
        case GFX_IL_SET_REND_PARAM:
            printf("GFX_IL_SET_REND_PARAM\n");
            break;
        case GFX_IL_SET_CLIP_RANGE:
            printf("GFX_IL_SET_CLIP_RANGE\n");
            break;
        case GFX_IL_DRAW_ARRAY:
            printf("GFX_IL_DRAW_ARRAY\n");
            break;
        case GFX_IL_INIT_OBJ:
            printf("GFX_IL_INIT_OBJ\n");
            break;
        case GFX_IL_WRITE_OBJ:
            printf("GFX_IL_WRITE_OBJ\n");
            break;
        case GFX_IL_READ_OBJ:
            printf("GFX_IL_READ_OBJ\n");
            break;
        case GFX_IL_FREE_OBJ:
            printf("GFX_IL_FREE_OBJ\n");
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            printf("GFX_IL_POST_FRAMEBUFFER\n");
            break;
        case GFX_IL_GRAB_FRAMEBUFFER:
            printf("GFX_IL_GRAB_FRAMEBUFFER\n");
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            printf("GFX_IL_BEGIN_DEPTH_SORT\n");
            break;
        case GFX_IL_END_DEPTH_SORT:
            printf("GFX_IL_END_DEPTH_SORT\n");
            break;
        default:
            fprintf(stderr, "ERROR: UNKNOWN GFX IL COMMAND %02X\n",
                    (unsigned)cmd->op);
        }
        cmd++;
    }
}
