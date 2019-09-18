/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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
#include <pthread.h>
#include <cstdlib>
#include <iostream>
#include <atomic>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/thread.h>

#include "washdc/log.h"
#include "washdc/washdc.h"

#ifdef ENABLE_DEBUGGER
#include "gdb_stub.hpp"
#include "washdbg_tcp.hpp"
#endif

#ifdef ENABLE_TCP_SERIAL
#include "serial_server.hpp"
#endif

#ifndef USE_LIBEVENT
#error this file should not be built with USE_LIBEVENT disabled!
#endif

namespace io {

struct event_base *event_base;
/*
 * event that gets invoked whenever somebody calls io_thread_kick
 * to tell the io_thread that it has work to do
 */
static struct event *work_event;

static pthread_t td;

static pthread_mutex_t create_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t create_condition = PTHREAD_COND_INITIALIZER;

static void* io_main(void *arg);
static void work_callback(evutil_socket_t fd, short ev, void *arg);

static std::atomic_bool alive;

void init() {
    int err_code;
    alive = true;

    if (pthread_mutex_lock(&create_mutex) != 0)
        abort(); // TODO: error handling

    if ((err_code = pthread_create(&td, NULL, io_main, NULL)) != 0)
        err(errno, "Unable to launch io thread");

    if (pthread_cond_wait(&create_condition, &create_mutex) != 0) {
            abort(); // TODO: error handling
    }

    if (pthread_mutex_unlock(&create_mutex) != 0)
        abort(); // TODO: error handling
}

void cleanup() {
    pthread_join(td, NULL);
}

void kick() {
    if (alive)
        event_active(work_event, 0, 0);
}

static void* io_main(void *arg) {
    if (pthread_mutex_lock(&create_mutex) != 0)
        abort(); // TODO: error handling

    evthread_use_pthreads();

    event_base = event_base_new();
    if (!event_base)
        errx(1, "event_base_new returned -1!");

    work_event = event_new(event_base, -1, EV_PERSIST,
                           work_callback, NULL);
    if (!work_event)
        errx(1, "event_new returned NULL!");

#ifdef ENABLE_TCP_SERIAL
    serial_server_init();
#endif

#ifdef ENABLE_DEBUGGER
    gdb_init();
    washdbg_tcp_init();
#endif


    if (pthread_cond_signal(&create_condition) != 0)
        abort(); // TODO: error handling

    if (pthread_mutex_unlock(&create_mutex) != 0)
        abort(); // TODO: error handling

    int const evflags = EVLOOP_NO_EXIT_ON_EMPTY;
    while (event_base_loop(event_base, evflags) >= 0) {
        if (!washdc_is_running())
            break;

        alive = false;
#ifdef ENABLE_TCP_SERIAL
        serial_server_run();
#endif
    }

    std::cout << "io thread finished" << std::endl;

    event_free(work_event);



#ifdef ENABLE_DEBUGGER
    washdbg_tcp_cleanup();
    gdb_cleanup();
#endif

#ifdef ENABLE_TCP_SERIAL
    serial_server_cleanup();
#endif

    event_base_free(event_base);

    pthread_exit(NULL);
    return NULL; // this line never executes
}

static void work_callback(evutil_socket_t fd, short ev, void *arg) {
    if (!washdc_is_running())
        event_base_loopbreak(event_base);

#ifdef ENABLE_TCP_SERIAL
    serial_server_run();
#endif
}

}
