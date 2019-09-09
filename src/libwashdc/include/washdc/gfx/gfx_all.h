/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

struct rend_if {
    void (*init)(void);

    void (*cleanup)(void);

    /*
     * called to notify the renderer that it needs to update the given
     * texture from the bound gfx_obj
     */
    void (*update_tex)(unsigned tex_obj);

    /*
     * called to notify the renderer that it needs to release the resources
     * associated with the given texture.
     */
    void (*release_tex)(unsigned tex_obj);

    // enable/disable blending
    void (*set_blend_enable)(bool do_enable);

    void (*set_rend_param)(struct gfx_rend_param const *param);

    void (*set_screen_dim)(unsigned width, unsigned height);

    void (*set_clip_range)(float clip_min, float clip_max);

    void (*draw_array)(float const *verts, unsigned n_verts);

    void (*clear)(float const bgcolor[4]);

    void (*begin_sort_mode)(void);

    void (*end_sort_mode)(void);

    void (*target_bind_obj)(int handle);

    void (*target_unbind_obj)(int handle);

    void (*target_begin)(unsigned width, unsigned height, int tgt_handle);

    void (*target_end)(int tgt_handle);

    int (*video_get_fb)(int *obj_handle_out, unsigned *width_out,
                        unsigned *height_out, bool *flip_out);

    void (*video_present)(void);

    void (*video_new_framebuffer)(int obj_handle,
                                  unsigned fb_new_width,
                                  unsigned fb_new_height, bool do_flip);

    void (*video_toggle_filter)(void);
};

#endif
