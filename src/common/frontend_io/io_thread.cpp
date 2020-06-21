/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
    /*
     * I have no idea what this is, but you have to do it to initialize
     * winsock for some stupid reason.
     */
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

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
