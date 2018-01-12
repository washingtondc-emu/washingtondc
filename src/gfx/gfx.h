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

#ifndef GFX_THREAD_H_
#define GFX_THREAD_H_

#include "gfx/gfx_tex_cache.h"
#include "hw/pvr2/geo_buf.h"

// The purpose of the GFX layer is to handle all the OpenGL-related things.

void gfx_init(unsigned width, unsigned height);

// consume a geo_buf (by drawing it)
void gfx_render_geo_buf(struct geo_buf *geo_buf);

// refresh the window
void gfx_expose(void);

/*
 * read OpenGL's view of the framebuffer into dat.  dat must be at least
 * (width*height*4) bytes.
 */
void gfx_read_framebuffer(void *dat, unsigned n_bytes);

void gfx_post_framebuffer(uint32_t const *fb_new,
                          unsigned fb_new_width,
                          unsigned fb_new_height);

#endif
