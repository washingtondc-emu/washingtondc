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
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/thread.h>

#ifdef ENABLE_SERIAL_SERVER
#include "serial_server.h"
#endif

#include "dreamcast.h"

#include "io_thread.h"

struct event_base *event_base;

static pthread_t io_thread;

static pthread_mutex_t io_thread_create_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t io_thread_create_condition = PTHREAD_COND_INITIALIZER;

static void *io_main(void *arg);

void io_thread_launch(void) {
    int err_code;

    if (pthread_mutex_lock(&io_thread_create_mutex) != 0)
        abort(); // TODO: error handling

    if ((err_code = pthread_create(&io_thread, NULL, io_main, NULL)) != 0)
        err(errno, "Unable to launch io thread");

    if (pthread_cond_wait(&io_thread_create_condition,
                          &io_thread_create_mutex) != 0) {
            abort(); // TODO: error handling
    }

    if (pthread_mutex_unlock(&io_thread_create_mutex) != 0)
        abort(); // TODO: error handling
}

void io_thread_join(void) {
    pthread_join(io_thread, NULL);
}

static void *io_main(void *arg) {
    if (pthread_mutex_lock(&io_thread_create_mutex) != 0)
        abort(); // TODO: error handling

    evthread_use_pthreads();

    event_base = event_base_new();
    if (!event_base)
        errx(1, "event_base_new returned -1!");

    if (pthread_cond_signal(&io_thread_create_condition) != 0)
        abort(); // TODO: error handling

    if (pthread_mutex_unlock(&io_thread_create_mutex) != 0)
        abort(); // TODO: error handling

#ifdef ENABLE_SERIAL_SERVER
    serial_server_init(dreamcast_get_cpu());
#endif

    while (event_base_loop(event_base, EVLOOP_ONCE) >= 0) {
        if (!dc_is_running())
            break;

#ifdef ENABLE_SERIAL_SERVER
        serial_server_run();
#endif
    }

#ifdef ENABLE_SERIAL_SERVER
    serial_server_cleanup();
#endif

    event_base_free(event_base);

    pthread_exit(NULL);
    return NULL; // this line never executes
}

void io_thread_kick(void) {
    event_base_loopbreak(event_base);
}
