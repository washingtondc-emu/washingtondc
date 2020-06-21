/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#ifndef USE_LIBEVENT
#error recompile with -DUSE_LIBEVENT=On
#endif

#ifndef ENABLE_TCP_SERIAL
#error recompile with -DENABLE_TCP_SERIAL=On
#endif

#include "threading.h"

#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cstring>
#include <iostream>

#include "washdc/error.h"
#include "washdc/washdc.h"
#include "washdc/serial_server.h"
#include "io_thread.hpp"

#include "serial_server.hpp"

static struct serial_server {
    struct evconnlistener *listener;
    struct bufferevent *bev;
    struct evbuffer *outbound;

    /*
     * used to signal whether or not the serial server is
     * listening for a remote connection over TCP.
     */
    std::atomic_bool is_listening;

    std::atomic_bool ready_to_write;

    std::atomic_flag no_more_work;
} srv;

/*
 * The SCIF calls this to let us know that it has data ready to transmit.
 * If the SerialServer is idling, it will immediately call sh4_scif_cts, and the
 * sh4 will send the data to the SerialServer via the SerialServer's put method
 *
 * If the SerialServer is active, this function does nothing and the server will call
 * sh4_scif_cts later when it is ready.
 */
static void serial_server_notify_tx_ready(void);

// this function can be safely called from outside of the context of the io thread
static void serial_server_attach(void);

struct serial_server_intf sersrv_intf;

static washdc_mutex srv_mutex = WASHDC_MUTEX_STATIC_INIT;
static washdc_cvar listener_condition = WASHDC_CVAR_STATIC_INIT;

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);
static void handle_events(struct bufferevent *bev, short events, void *arg);
static void handle_read(struct bufferevent *bev, void *arg);
static void handle_write(struct bufferevent *bev, void *arg);

static void ser_srv_lock(void);
static void ser_srv_unlock(void);
static void wait_for_connection(void);
static void signal_connection(void);

static void drain_txq(void);

void serial_server_init(void) {

    sersrv_intf.attach = serial_server_attach;
    sersrv_intf.notify_tx_ready = serial_server_notify_tx_ready;

    atomic_flag_test_and_set(&srv.no_more_work);
}

void serial_server_cleanup(void) {
    if (srv.outbound)
        evbuffer_free(srv.outbound);
    if (srv.bev)
        bufferevent_free(srv.bev);
    if (srv.listener)
        evconnlistener_free(srv.listener);

    memset(&srv, 0, sizeof(srv));
}

static void serial_server_attach(void) {
    std::cout << "Awaiting serial connection on port " << SERIAL_PORT_NO << std::endl;

    ser_srv_lock();

    if (!(srv.outbound = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERIAL_PORT_NO);
    unsigned evflags = LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE |
        LEV_OPT_CLOSE_ON_FREE;
    srv.listener = evconnlistener_new_bind(io::event_base, listener_cb,
                                           &srv, evflags, -1,
                                           (struct sockaddr*)&sin, sizeof(sin));

    if (!srv.listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    wait_for_connection();

    ser_srv_unlock();

    std::cout << "Connection established." << std::endl;
}

static void handle_read(struct bufferevent *bev, void *arg) {
    struct evbuffer *read_buffer;
    if (!(read_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    // now send the data to the SCIF one char at a time
    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);
    for (unsigned idx = 0; idx < buflen; idx++) {
        uint8_t cur_byte;
        if (evbuffer_remove(read_buffer, &cur_byte, sizeof(cur_byte)) < 0)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        // TODO: it is possible for data to get dropped here.
        washdc_serial_server_rx((char)cur_byte);
    }

    evbuffer_free(read_buffer);
}

/*
 * this function gets called when libevent is done writing
 * and is hungry for more data
 */
static void handle_write(struct bufferevent *bev, void *arg) {
    if (!evbuffer_get_length(srv.outbound)) {
        atomic_store(&srv.ready_to_write, true);
        drain_txq();
        washdc_serial_server_cts();
        return;
    }

    bufferevent_write_buffer(bev, srv.outbound);
    atomic_store(&srv.ready_to_write, false);
}

static void serial_server_notify_tx_ready(void) {
    atomic_flag_clear(&srv.no_more_work);
    io::kick();
}

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    ser_srv_lock();

    srv.bev = bufferevent_socket_new(io::event_base, fd,
                                     BEV_OPT_CLOSE_ON_FREE);
    if (!srv.bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(srv.bev, handle_read, handle_write, handle_events, &srv);
    bufferevent_enable(srv.bev, EV_WRITE);
    bufferevent_enable(srv.bev, EV_READ);

    atomic_store(&srv.is_listening, false);

    signal_connection();

    ser_srv_unlock();
}

static void handle_events(struct bufferevent *bev, short events, void *arg) {
    // I must confess, I don't know why this is here...
    char const *ev_type;
    switch (events) {
    case BEV_EVENT_EOF:
        ev_type = "eof";
        break;
    case BEV_EVENT_ERROR:
        ev_type = "error";
        break;
    case BEV_EVENT_TIMEOUT:
        ev_type = "timeout";
        break;
    case BEV_EVENT_READING:
        ev_type = "reading";
        break;
    case BEV_EVENT_WRITING:
        ev_type = "writing";
        break;
    case BEV_EVENT_CONNECTED:
        ev_type = "connected";
        break;
    default:
        ev_type = "unknown";
    }
    if (events != BEV_EVENT_EOF) {
        std::cerr << __func__ << " called: \"" << ev_type << "\" (" << events
                  << ") event received; calling washdc_kill" << std::endl;
        washdc_kill();
    } else {
        std::cerr << __func__ << " called - EOF received" << std::endl;
        bufferevent_free(srv.bev);
        srv.bev = NULL;
    }
}

static void ser_srv_lock(void) {
    washdc_mutex_lock(&srv_mutex);
}

static void ser_srv_unlock(void) {
    washdc_mutex_unlock(&srv_mutex);
}

static void wait_for_connection(void) {
    atomic_store(&srv.is_listening, true);

    do {
        std::cout << "still waiting..." << std::endl;
        washdc_cvar_wait(&listener_condition, &srv_mutex);

        // TODO: handle case where dreamcast_is_running() is now false?
    } while (atomic_load(&srv.is_listening));
}

static void signal_connection(void) {
    washdc_cvar_signal(&listener_condition);
}

void serial_server_run(void) {
    if (!atomic_flag_test_and_set(&srv.no_more_work)) {
        drain_txq();
    }
}

// returns true if tx was successful, false otherwise
static bool do_tx_char(void) {
    char ch;
    if (washdc_serial_server_tx(&ch) == 0) {
        evbuffer_add(srv.outbound, &ch, sizeof(ch));
        return true;
    }
    return false;
}

static void drain_txq(void) {
    // drain the txq
    bool did_tx = false;
    while (do_tx_char())
        did_tx = true;
    if (atomic_load(&srv.ready_to_write) && did_tx) {
        bufferevent_write_buffer(srv.bev, srv.outbound);
        srv.ready_to_write = false;
    }
}
