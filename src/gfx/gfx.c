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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdbool.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "glfw/window.h"
#include "dreamcast.h"
#include "gfx/opengl/opengl_output.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/rend_common.h"
#include "gfx/gfx_tex_cache.h"
#include "log.h"

// for the palette_tp stuff
#include "hw/pvr2/pvr2_core_reg.h"

#include "gfx/gfx.h"
static unsigned win_width, win_height;

// Only call gfx_thread_signal and gfx_thread_wait when you hold the lock.
static void gfx_do_init(void);

void gfx_init(unsigned width, unsigned height) {
    win_width = width;
    win_height = height;

    LOG_INFO("GFX: rendering graphics from within the main emulation thread\n");
    gfx_do_init();
}

void gfx_render_geo_buf(struct geo_buf *geo) {
    rend_draw_geo_buf(geo);
}

void gfx_expose(void) {
    opengl_video_present();
    win_update();
}

static void gfx_do_init(void) {
    win_make_context_current();

    glewExperimental = GL_TRUE;
    glewInit();
    glViewport(0, 0, win_width, win_height);

    opengl_target_init();
    opengl_video_output_init();
    gfx_tex_cache_init();
    rend_init();

    glClear(GL_COLOR_BUFFER_BIT);
}

void gfx_read_framebuffer(void *dat, unsigned n_bytes) {
    opengl_target_grab_pixels(dat, n_bytes);
}

void gfx_post_framebuffer(uint32_t const *fb_new,
                          unsigned fb_new_width,
                          unsigned fb_new_height) {
    opengl_video_new_framebuffer(fb_new, fb_new_width, fb_new_height);
}
