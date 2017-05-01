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

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>

#include "Dreamcast.hpp"

#include "serial_server.h"

#ifndef ENABLE_SERIAL_SERVER
#error This file should not be built unless the serial server is enabled
#endif

typedef struct Sh4 Sh4;

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);
static void handle_events(struct bufferevent *bev, short events, void *arg);
static void handle_read(struct bufferevent *bev, void *arg);
static void handle_write(struct bufferevent *bev, void *arg);

extern "C"
void serial_server_init(struct serial_server *srv, struct Sh4 *cpu) {
    memset(srv, 0, sizeof(*srv));

    srv->cpu = cpu;
    srv->listener = NULL;
    srv->bev = NULL;

    srv->ready_to_write = false;

    if (!(srv->outbound = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);
}

extern "C"
void serial_server_cleanup(struct serial_server *srv) {
    evbuffer_free(srv->outbound);
    if (srv->bev)
        bufferevent_free(srv->bev);
    if (srv->listener)
        evconnlistener_free(srv->listener);

    memset(srv, 0, sizeof(*srv));
}

extern "C"
void serial_server_attach(struct serial_server *srv) {
    std::cout << "Awaiting serial connection on port " << SERIAL_PORT_NO << "..." <<
        std::endl;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERIAL_PORT_NO);
    srv->listener = evconnlistener_new_bind(dc_event_base, listener_cb, srv,
                                       LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
                                       (struct sockaddr*)&sin, sizeof(sin));

    if (!srv->listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    // the listener_cb will set is_listening = false when we have a connection
    srv->is_listening = true;
    do {
        std::cout << "still waiting..." << std::endl;
        if (event_base_loop(dc_event_base, EVLOOP_ONCE) != 0)
            exit(4);
    } while (srv->is_listening);

    std::cout << "Connection established." << std::endl;
}

static void handle_read(struct bufferevent *bev, void *arg) {
    struct serial_server *srv = (struct serial_server*)arg;

    struct evbuffer *read_buffer;
    if (!(read_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    // now send the data to the SCIF one char at a time
    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);
    for (unsigned idx = 0; idx < buflen ; idx++) {
        uint8_t cur_byte;
        if (evbuffer_remove(read_buffer, &cur_byte, sizeof(cur_byte)) < 0)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        sh4_scif_rx(srv->cpu, cur_byte);
    }

    evbuffer_free(read_buffer);
}

/*
 * this function gets called when boost is done writing
 * and is hungry for more data
 */
static void handle_write(struct bufferevent *bev, void *arg) {
    struct serial_server *srv = (struct serial_server*)arg;

    if (!evbuffer_get_length(srv->outbound)) {
        srv->ready_to_write = true;
        sh4_scif_cts(srv->cpu);
        return;
    }

    bufferevent_write_buffer(bev, srv->outbound);
    srv->ready_to_write = false;
}

void serial_server_put(struct serial_server *srv, uint8_t dat) {
    evbuffer_add(srv->outbound, &dat, sizeof(dat));

    if (srv->ready_to_write) {
        bufferevent_write_buffer(srv->bev, srv->outbound);
        srv->ready_to_write = false;
    }
}

void serial_server_notify_tx_ready(struct serial_server *srv) {
    sh4_scif_cts(srv->cpu);
}

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    struct serial_server *srv = (struct serial_server*)arg;

    srv->bev = bufferevent_socket_new(dc_event_base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!srv->bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(srv->bev, handle_read, handle_write, handle_events, srv);
    bufferevent_enable(srv->bev, EV_WRITE);
    bufferevent_enable(srv->bev, EV_READ);

    srv->is_listening = false;
}

static void handle_events(struct bufferevent *bev, short events, void *arg) {
    exit(2);
}
