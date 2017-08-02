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

#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

#include "dreamcast.h"

#include "glfw/window.h"

/*
 * used to pass the window width/height from the main thread to the window
 * thread for the win_init function
 */
static unsigned win_width, win_height;
static pthread_t win_thread;

static pthread_cond_t win_init_condition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t win_init_lock = PTHREAD_MUTEX_INITIALIZER;

static void* win_main(void *arg);

void win_thread_launch(unsigned width, unsigned height) {
    int err_code;

    win_width = width;
    win_height = height;

    if (pthread_mutex_lock(&win_init_lock) != 0)
        abort(); // TODO: error handling

    if ((err_code = pthread_create(&win_thread, NULL, win_main, NULL)) != 0)
        err(errno, "Unable to launch window thread");

    pthread_cond_wait(&win_init_condition, &win_init_lock);

    if (pthread_mutex_unlock(&win_init_lock) != 0)
        abort(); // TODO: error handling
}

static void* win_main(void *arg) {
    if (pthread_mutex_lock(&win_init_lock) != 0)
        abort(); // TODO: error handling

    win_init(win_width, win_height);

    if (pthread_cond_signal(&win_init_condition) != 0)
        abort(); // TODO: error handling
    if (pthread_mutex_unlock(&win_init_lock) != 0)
        abort(); // TODO: error handling

    while (dc_is_running())
        win_check_events();

    win_cleanup();

    pthread_exit(NULL);
    return NULL; // this line will never execute
}

void win_thread_join(void) {
    pthread_join(win_thread, NULL);
}

void win_thread_update(void) {
    win_update();
}

void win_thread_make_context_current(void) {
    win_make_context_current();
}
