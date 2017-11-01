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

#ifndef GFX_THREAD_H_
#define GFX_THREAD_H_

#include "gfx/gfx_tex_cache.h"
#include "hw/pvr2/geo_buf.h"

#ifdef __cplusplus
extern "C"{
#endif

// The purpose of the GFX thread is to handle all the OpenGL-related things.

void gfx_thread_launch(unsigned width, unsigned height);

/*
 * make sure dc_is_running() is false AND make sure to call
 * gfx_thread_notify_wake_up before calling this.
 */
void gfx_thread_join(void);

// signals the gfx thread to wake up and make the opengl backend redraw
void gfx_thread_redraw();

// signals the gfx thread to wake up and consume a geo_buf (by drawing it)
void gfx_thread_render_geo_buf(struct geo_buf *geo_buf);

// signals for the gfx thread to wake up and refresh the window
void gfx_thread_expose(void);

// block until the gfx_thread has rendered the given geo_buf
void gfx_thread_wait_for_geo_buf_stamp(unsigned stamp);

/*
 * causes the gfx_thread to wakeup and check for work that needs to be done.
 * The only reason to call this is when dc_is_running starts returning false
 * (see src/dreamcast.c).  Otherwise, any function that pushes work to the
 * gfx_thread will do this itself.
 *
 * So really, there's only one place where this function should be called, and
 * if you see it called from anywhere else then it *might* mean that somebody
 * goofed up.
 */
void gfx_thread_notify_wake_up(void);

/*
 * read OpenGL's view of the framebuffer into dat.  dat must be at least
 * (width*height*4) bytes.
 */
void gfx_thread_read_framebuffer(void *dat, unsigned n_bytes);

void gfx_thread_post_framebuffer(uint32_t const *fb_new,
                                 unsigned fb_new_width,
                                 unsigned fb_new_height);

void gfx_thread_run_once(void);

#ifdef __cplusplus
}
#endif

#endif
