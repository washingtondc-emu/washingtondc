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

#ifndef OPENGL_OUTPUT_H_
#define OPENGL_OUTPUT_H_

/*
 * opengl_output.h: the final stage of rendering, where the framebuffer is
 * turned into and opengl texture that's rendered onto a quadrilateral
 * stretched across the screen.
 */

#include <stdint.h>
#include <stdbool.h>

/*
 * this gets called every time the framebuffer has a new frame to render.
 * fb_new belongs to the caller, and its contents will be copied into a new
 * storage area.
 *
 * this function is safe to call from outside of the graphics thread
 * from outside of the graphics thread, it should only be called indirectly via
 * gfx_thread_post_framebuffer.
 */
void opengl_video_new_framebuffer(int obj_handle,
                                  unsigned fb_new_width,
                                  unsigned fb_new_height, bool do_flip);

void opengl_video_present();

void opengl_video_output_init();
void opengl_video_output_cleanup();

// vertex position (x, y, z)
#define OUTPUT_SLOT_VERT_POS 0

// vertex texture coordinates (s, t)
#define OUTPUT_SLOT_VERT_ST 1

#define OUTPUT_SLOT_TRANS_MAT 2

#define OUTPUT_SLOT_TEX_MAT 3

#endif
