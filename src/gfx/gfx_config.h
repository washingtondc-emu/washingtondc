/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef GFX_CONFIG_H_
#define GFX_CONFIG_H_

#include <stdbool.h>

/*
 * The purpose of this file is to store settings for the graphics renderer.
 * There's a "default" configuration that renders everything the way you'd
 * expect (ie like a real Dreamcast would), but users can make changes to those
 * settings over the CLI to render things differently.  The primary usecase for
 * this is that sometimes I have to debug rendering bugs (like incorrect
 * depth-sorting), and I think it would be helpful to be able to do things like
 * render in wireframe or selectively disable polygons based on display lists,
 * etc.
 *
 * This code should not be used to implement graphics emulation.  These settings
 * are write-only from the cmd thread and read-only from the gfx thread.  No
 * other threads should ever touch this.
 */

struct gfx_cfg {
    // if true, the renderer will render polygons as lines
    bool wireframe;

    // if false, textures will be forcibly disabled
    bool tex_enable;

    // if false, depth-testing will be forcibly disabled
    bool depth_enable;

    // if false, blending will be forcibly disabled
    bool blend_enable;

    // if false, the background color will always be black
    bool bgcolor_enable;

    // if false, all polygons will be white
    bool color_enable;
};

/*
 * regardless of what the current settings are, this function restores them to
 * the defaults.
 */
void gfx_config_default(void);

// set the config to wireframe mode
void gfx_config_wireframe(void);

// only call the following function from the gfx_thread
void gfx_config_read(struct gfx_cfg *cfg);

#endif
