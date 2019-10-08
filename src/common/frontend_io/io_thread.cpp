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

#include "threading.h"

#include <cstdlib>
#include <iostream>
#include <atomic>
#include <cstring>

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

static washdc_thread td;

static washdc_mutex create_mutex = WASHDC_MUTEX_STATIC_INIT;
static washdc_cvar create_condition = WASHDC_CVAR_STATIC_INIT;

static void io_main(void *arg);
static void work_callback(evutil_socket_t fd, short ev, void *arg);

static std::atomic_bool alive;

void init() {
    int err_code;
    alive = true;

    washdc_mutex_lock(&create_mutex);

    washdc_thread_create(&td, io_main, NULL);

    washdc_cvar_wait(&create_condition, &create_mutex);

    washdc_mutex_unlock(&create_mutex);
}

void cleanup() {
    washdc_thread_join(&td);
}

void kick() {
    if (alive)
        event_active(work_event, 0, 0);
}

static void io_main(void *arg) {
    washdc_mutex_lock(&create_mutex);

#ifdef _WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif

    event_base = event_base_new();
    if (!event_base) {
        fprintf(stderr, "event_base_new returned -1!\n");
        exit(1);
    }

    work_event = event_new(event_base, -1, EV_PERSIST,
                           work_callback, NULL);
    if (!work_event) {
        fprintf(stderr, "event_new returned NULL!\n");
        exit(1);
    }

#ifdef ENABLE_TCP_SERIAL
    serial_server_init();
#endif

#ifdef ENABLE_DEBUGGER
    gdb_init();
    washdbg_tcp_init();
#endif

    washdc_cvar_signal(&create_condition);
    washdc_mutex_unlock(&create_mutex);

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
}

static void work_callback(evutil_socket_t fd, short ev, void *arg) {
    if (!washdc_is_running())
        event_base_loopbreak(event_base);

#ifdef ENABLE_TCP_SERIAL
    serial_server_run();
#endif
}

}
