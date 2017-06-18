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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdbool.h>
#include <stdatomic.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "window.h"
#include "dreamcast.h"
#include "video/opengl/opengl_output.h"
#include "video/opengl/opengl_target.h"
#include "video/opengl/opengl_renderer.h"

#include "gfx_thread.h"

/*
 * used to pass the window width/height from the main thread to the gfx
 * thread for the win_init function
 */
static unsigned win_width, win_height;
static pthread_t gfx_thread;

// if this is cleared, it means that there's been a vblank
static atomic_flag not_pending_redraw = ATOMIC_FLAG_INIT;

// if this is cleared, it means that userspace is waiting for us to read the framebuffer
static atomic_flag not_reading_framebuffer = ATOMIC_FLAG_INIT;

// if this is cleared, it means that there's a geo_buf waiting for us
static atomic_flag not_rendering_geo_buf = ATOMIC_FLAG_INIT;

/*
 * when gfx_thread_read_framebuffer gets called it sets this to point to where
 * the framebuffer should be written to, clears not_reading_framebuffer, then
 * waits on the fb_read_condtion condition.
 */
static void * volatile fb_out;
static volatile unsigned fb_out_size;

static pthread_cond_t fb_read_condition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t fb_out_lock = PTHREAD_MUTEX_INITIALIZER;

static void* gfx_main(void *arg);

void gfx_thread_launch(unsigned width, unsigned height) {
    int err_code;

    win_width = width;
    win_height = height;

    atomic_flag_test_and_set(&not_pending_redraw);
    atomic_flag_test_and_set(&not_reading_framebuffer);

    if ((err_code = pthread_create(&gfx_thread, NULL, gfx_main, NULL)) != 0)
        err(errno, "Unable to launch gfx thread");
}

void gfx_thread_join(void) {
    pthread_join(gfx_thread, NULL);
}

void gfx_thread_redraw() {
    atomic_flag_clear(&not_pending_redraw);
    glfwPostEmptyEvent();
}

void gfx_thread_render_geo_buf(void) {
    atomic_flag_clear(&not_rendering_geo_buf);
    glfwPostEmptyEvent();
}

static void* gfx_main(void *arg) {
    win_init(win_width, win_height);

    opengl_target_init();
    opengl_video_output_init();
    render_init();

    /*
     * this is just here for some testing/validation so I can make sure that
     * the picture in opengl makes its way to the framebuffer and back, feel
     * free to delete it at any time.
     */
    opengl_target_begin(640, 480);
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    opengl_target_end();

    glClear(GL_COLOR_BUFFER_BIT);

    do {
        if (!atomic_flag_test_and_set(&not_pending_redraw)) {
            opengl_video_update_framebuffer();
            opengl_video_present();
            win_update();
        }

        if (!atomic_flag_test_and_set(&not_reading_framebuffer)) {
            if (pthread_mutex_lock(&fb_out_lock) != 0)
                abort(); // TODO: error handling

            // TODO: render 3d graphics here
            opengl_target_grab_pixels(fb_out, fb_out_size);
            fb_out = NULL;
            fb_out_size = 0;

            if (pthread_cond_signal(&fb_read_condition) != 0)
                abort(); // TODO: error handling

            if (pthread_mutex_unlock(&fb_out_lock) != 0)
                abort(); // TODO: error handling
        }

        if (!atomic_flag_test_and_set(&not_rendering_geo_buf))
            render_next_geo_buf();

        win_check_events();
    } while (dc_is_running());

    render_cleanup();

    opengl_video_output_cleanup();
    win_cleanup();

    pthread_exit(NULL);
    return NULL; /* this line will never execute */
}

void gfx_thread_read_framebuffer(void *dat, unsigned n_bytes) {
    if (pthread_mutex_lock(&fb_out_lock) != 0)
        abort(); // TODO: error handling

    fb_out = dat;
    fb_out_size = n_bytes;
    atomic_flag_clear(&not_reading_framebuffer);

    while (fb_out) {
        glfwPostEmptyEvent();
        pthread_cond_wait(&fb_read_condition, &fb_out_lock);
    }

    if (pthread_mutex_unlock(&fb_out_lock) != 0)
        abort(); // TODO: error handling
}

void gfx_thread_notify_wake_up(void) {
    glfwPostEmptyEvent();
}
