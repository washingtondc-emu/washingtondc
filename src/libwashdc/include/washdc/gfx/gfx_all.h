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

#ifndef WASHDC_GFX_H_
#define WASHDC_GFX_H_

#include "config.h"
#include "def.h"
#include "washdc/gfx/gfx_il.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rend_if {
    void (*init)(void);

    void (*cleanup)(void);

    void (*bind_tex)(struct gfx_il_inst *cmd);
    void (*unbind_tex)(struct gfx_il_inst *cmd);

    void (*obj_init)(struct gfx_il_inst *cmd);
    void (*obj_write)(struct gfx_il_inst *cmd);
    void (*obj_read)(struct gfx_il_inst *cmd);
    void (*obj_free)(struct gfx_il_inst *cmd);

    void (*grab_framebuffer)(struct gfx_il_inst *cmd);

    void (*begin_rend)(struct gfx_il_inst *cmd);
    void (*end_rend)(struct gfx_il_inst *cmd);

    // enable/disable blending
    void (*set_blend_enable)(struct gfx_il_inst *cmd);

    void (*set_rend_param)(struct gfx_il_inst *cmd);

    void (*set_clip_range)(struct gfx_il_inst *cmd);

    void (*draw_array)(struct gfx_il_inst *cmd);

    void (*clear)(struct gfx_il_inst *cmd);

    void (*begin_sort_mode)(struct gfx_il_inst *cmd);

    void (*end_sort_mode)(struct gfx_il_inst *cmd);

    void (*target_bind_obj)(struct gfx_il_inst *cmd);

    void (*target_unbind_obj)(struct gfx_il_inst *cmd);

    void (*video_post_framebuffer)(struct gfx_il_inst *cmd);
};

#ifdef __cplusplus
}
#endif

#endif
