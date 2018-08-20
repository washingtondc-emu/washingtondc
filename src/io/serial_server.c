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

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#include "dreamcast.h"
#include "io_thread.h"
#include "log.h"

#include "serial_server.h"

static struct serial_server {
    struct evconnlistener *listener;
    struct bufferevent *bev;
    struct evbuffer *outbound;

    struct Sh4 *cpu;

    /*
     * used to signal whether or not the serial server is
     * listening for a remote connection over TCP.
     */
    volatile bool is_listening;

    volatile bool ready_to_write;

    atomic_flag no_more_work;
} srv = {
    .no_more_work = ATOMIC_FLAG_INIT
};

static pthread_mutex_t srv_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t listener_condition = PTHREAD_COND_INITIALIZER;

typedef struct Sh4 Sh4;

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

void serial_server_init(struct Sh4 *cpu) {
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

void serial_server_attach(void) {
    LOG_INFO("Awaiting serial connection on port %d...\n", SERIAL_PORT_NO);

    ser_srv_lock();

    srv.cpu = dreamcast_get_cpu();
    if (!(srv.outbound = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERIAL_PORT_NO);
    unsigned evflags = LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE |
        LEV_OPT_CLOSE_ON_FREE;
    srv.listener = evconnlistener_new_bind(io_thread_event_base, listener_cb,
                                           &srv, evflags, -1,
                                           (struct sockaddr*)&sin, sizeof(sin));

    if (!srv.listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    wait_for_connection();

    ser_srv_unlock();

    LOG_INFO("Connection established.\n");
}

static void handle_read(struct bufferevent *bev, void *arg) {
    struct text_ring *rxq = &srv.cpu->scif.rxq;

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
        text_ring_produce(rxq, (char)cur_byte);

        sh4_scif_rx(srv.cpu);
    }

    evbuffer_free(read_buffer);
}

/*
 * this function gets called when libevent is done writing
 * and is hungry for more data
 */
static void handle_write(struct bufferevent *bev, void *arg) {
    if (!evbuffer_get_length(srv.outbound)) {
        srv.ready_to_write = true;
        drain_txq();
        sh4_scif_cts(srv.cpu);
        return;
    }

    bufferevent_write_buffer(bev, srv.outbound);
    srv.ready_to_write = false;
}

void serial_server_notify_tx_ready(void) {
    atomic_flag_clear(&srv.no_more_work);
    io_thread_kick();
}

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    ser_srv_lock();

    srv.bev = bufferevent_socket_new(io_thread_event_base, fd,
                                     BEV_OPT_CLOSE_ON_FREE);
    if (!srv.bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(srv.bev, handle_read, handle_write, handle_events, &srv);
    bufferevent_enable(srv.bev, EV_WRITE);
    bufferevent_enable(srv.bev, EV_READ);

    srv.is_listening = false;

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
        LOG_ERROR("%s called: \"%s\" (%d) event received; exiting with code 2\n",
                  __func__, ev_type, (int)events);
        exit(2);
    } else {
        LOG_WARN("%s called - EOF received\n", __func__);
        bufferevent_free(srv.bev);
        srv.bev = NULL;
    }
}

static void ser_srv_lock(void) {
    if (pthread_mutex_lock(&srv_mutex) < 0)
        abort(); // TODO error handling
}

static void ser_srv_unlock(void) {
    if (pthread_mutex_unlock(&srv_mutex) < 0)
        abort(); // TODO error handling
}

static void wait_for_connection(void) {
    srv.is_listening = true;

    do {
        LOG_INFO("still waiting...\n");
        if (pthread_cond_wait(&listener_condition, &srv_mutex) < 0)
            abort(); // TODO: error handling

        // TODO: handle case where dreamcast_is_running() is now false?
    } while (srv.is_listening);
}

static void signal_connection(void) {
    if (pthread_cond_signal(&listener_condition) < 0)
        abort(); // TODO error handling
}

void serial_server_run(void) {
    if (!atomic_flag_test_and_set(&srv.no_more_work)) {
        drain_txq();
    }
}

// returns true if tx was successful, false otherwise
static bool do_tx_char(struct text_ring *txq) {
    if (text_ring_empty(txq))
        return false;
    char ch = text_ring_consume(txq);
    evbuffer_add(srv.outbound, &ch, sizeof(ch));
    return true;
}

static void drain_txq(void) {
    Sh4 *sh4 = dreamcast_get_cpu();

    // drain the txq
    struct text_ring *txq = &sh4->scif.txq;
    bool did_tx = false;
    while (do_tx_char(txq))
        did_tx = true;
    if (srv.ready_to_write && did_tx) {
        bufferevent_write_buffer(srv.bev, srv.outbound);
        srv.ready_to_write = false;
    }
}
