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

#ifndef GFX_THREAD_H_
#define GFX_THREAD_H_

#include <assert.h>

#include "washdc/washdc.h"
#include "washdc/gfx/def.h"

static_assert(PVR2_DEPTH_FUNC_COUNT == 8,
              "incorrect number of depth functions");

// The purpose of the GFX layer is to handle all the OpenGL-related things.

struct rend_if;
void gfx_init(struct rend_if const * rend_if, unsigned width, unsigned height);
void gfx_cleanup(void);

// refresh the window
void gfx_expose(void);
void gfx_redraw(void);
void gfx_resize(int xres, int yres);

void gfx_post_framebuffer(int obj_handle,
                          unsigned fb_new_width,
                          unsigned fb_new_height, bool do_flip, bool interlaced);

/*
 * This takes place immediately because the user can toggle it asynchronously
 * with a keybind.  It is not part of gfx_il.
 */
void gfx_toggle_output_filter(void);

void gfx_set_overlay_intf(struct washdc_overlay_intf const *intf);

#endif
