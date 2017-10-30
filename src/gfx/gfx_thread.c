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

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "glfw/window.h"
#include "dreamcast.h"
#include "gfx/opengl/opengl_output.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/rend_common.h"
#include "gfx/gfx_tex_cache.h"

// for the palette_tp stuff
#include "hw/pvr2/pvr2_core_reg.h"

#include "gfx/gfx_thread.h"

static pthread_t gfx_thread;

// if this is set, it means that there's been a vblank
static bool pending_redraw;

/*
 * if this is set, it means that userspace is waiting for us to read the
 * framebuffer.
 */
static bool reading_framebuffer;

// if this is set, it means that there's a geo_buf waiting for us
static bool rendering_geo_buf;

/*
 * if this is set, it means that there's nothing to draw
 * but we need to refresh the window
 */
static bool pending_expose;

/*
 * when gfx_thread_read_framebuffer gets called it sets this to point to where
 * the framebuffer should be written to, sets reading_framebuffer, then
 * waits on the fb_read_condtion condition.
 *
 * These variables should only be accessed by whomever holds the gfx_thread_work_lock
 */
static void * volatile fb_out;
static volatile unsigned fb_out_size;

static pthread_cond_t fb_read_condition = PTHREAD_COND_INITIALIZER;

static pthread_cond_t gfx_thread_work_condition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t gfx_thread_work_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned win_width, win_height;

static void* gfx_main(void *arg);

// Only call gfx_thread_signal and gfx_thread_wait when you hold the lock.
static void gfx_thread_lock(void);
static void gfx_thread_unlock(void);
static void gfx_thread_signal(void);
static void gfx_thread_wait(void);

void gfx_thread_launch(unsigned width, unsigned height) {
    int err_code;

    win_width = width;
    win_height = height;

    if ((err_code = pthread_create(&gfx_thread, NULL, gfx_main, NULL)) != 0)
        err(errno, "Unable to launch gfx thread");
}

void gfx_thread_join(void) {
    pthread_join(gfx_thread, NULL);
}

void gfx_thread_redraw() {
    gfx_thread_lock();
    pending_redraw = true;
    gfx_thread_signal();
    gfx_thread_unlock();
}

void gfx_thread_render_geo_buf(void) {
    gfx_thread_lock();
    rendering_geo_buf = true;
    gfx_thread_signal();
    gfx_thread_unlock();
}

void gfx_thread_expose(void) {
    gfx_thread_lock();
    pending_expose = true;
    gfx_thread_signal();
    gfx_thread_unlock();
}

static void* gfx_main(void *arg) {
    win_make_context_current();

    glewExperimental = GL_TRUE;
    glewInit();
    glViewport(0, 0, win_width, win_height);

    opengl_target_init();
    opengl_video_output_init();
    gfx_tex_cache_init();
    rend_init();

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

    gfx_thread_lock();

    do {
        gfx_thread_run_once();
        gfx_thread_wait();
    } while (dc_is_running());

    if (pending_redraw)
        printf("%s - there was a pending redraw\n", __func__);
    if (reading_framebuffer)
        printf("%s - there was a pending framebuffer read\n", __func__);
    if (rendering_geo_buf)
        printf("%s - there was a pending geo_buf render\n", __func__);

    gfx_thread_unlock();

    gfx_tex_cache_cleanup();
    rend_cleanup();

    opengl_video_output_cleanup();

    pthread_exit(NULL);
    return NULL; /* this line will never execute */
}

void gfx_thread_run_once(void) {
    if (pending_redraw) {
        pending_redraw = false;
        opengl_video_update_framebuffer();
        opengl_video_present();
        win_update();
    }

    if (pending_expose) {
        pending_expose = false;
        opengl_video_present();
        win_update();
    }

    if (reading_framebuffer) {
        reading_framebuffer = false;
        opengl_target_grab_pixels(fb_out, fb_out_size);
        fb_out = NULL;
        fb_out_size = 0;

        if (pthread_cond_signal(&fb_read_condition) != 0)
            abort(); // TODO: error handling
    }

    if (rendering_geo_buf) {
        rendering_geo_buf = false;
        rend_draw_next_geo_buf();
    }
}

void gfx_thread_read_framebuffer(void *dat, unsigned n_bytes) {
    gfx_thread_lock();

    fb_out = dat;
    fb_out_size = n_bytes;
    reading_framebuffer = true;

    gfx_thread_signal();

    while (fb_out) {
        pthread_cond_wait(&fb_read_condition, &gfx_thread_work_lock);
    }

    gfx_thread_unlock();
}

void gfx_thread_notify_wake_up(void) {
    gfx_thread_lock();
    gfx_thread_signal();
    gfx_thread_unlock();
}

void gfx_thread_wait_for_geo_buf_stamp(unsigned stamp) {
    rend_wait_for_frame_stamp(stamp);
}

void gfx_thread_post_framebuffer(uint32_t const *fb_new,
                                 unsigned fb_new_width,
                                 unsigned fb_new_height) {
    opengl_video_new_framebuffer(fb_new, fb_new_width, fb_new_height);
}

static void gfx_thread_lock(void) {
    if (pthread_mutex_lock(&gfx_thread_work_lock) != 0)
        abort(); // TODO: error handling
}

static void gfx_thread_unlock(void) {
    if (pthread_mutex_unlock(&gfx_thread_work_lock) != 0)
        abort(); // TODO: error handling
}

static void gfx_thread_signal(void) {
    if (pthread_cond_signal(&gfx_thread_work_condition) != 0)
        abort(); // TODO: error handling
}

static void gfx_thread_wait(void) {
    if (pthread_cond_wait(&gfx_thread_work_condition, &gfx_thread_work_lock) != 0)
        abort(); // TODO: error handling
}
