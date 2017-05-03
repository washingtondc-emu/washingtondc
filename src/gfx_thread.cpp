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
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <err.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "window.h"
#include "dreamcast.h"
#include "video/opengl/opengl_backend.hpp"

#include "gfx_thread.hpp"

/*
 * used to pass the window width/height from the main thread to the gfx
 * thread for the win_init function
 */
static unsigned win_width, win_height;
static pthread_t gfx_thread;

static volatile bool gfx_thread_dead;

static void* gfx_main(void *arg);

void gfx_thread_launch(unsigned width, unsigned height) {
    int err_code;

    win_width = width;
    win_height = height;

    if ((err_code = pthread_create(&gfx_thread, NULL, gfx_main, NULL)) != 0)
        err(errno, "Unable to launch gfx thread");
}

void gfx_thread_kill() {
    gfx_thread_dead = true;
    glfwPostEmptyEvent();
    pthread_join(gfx_thread, NULL);
}

void gfx_thread_redraw() {
    glfwPostEmptyEvent();
}

static void* gfx_main(void *arg) {
    win_init(win_width, win_height);

    opengl_backend_init();

    do {
        /*
         * TODO: only run backend_update_framebuffer
         * when the framebuffer needs to be updated
         */
        backend_update_framebuffer();
        backend_present();
        win_update();
    } while (win_check_events() && !gfx_thread_dead);

    opengl_backend_cleanup();
    win_cleanup();

    dreamcast_kill();
    pthread_exit(NULL);
    return NULL; /* this line will never execute */
}
