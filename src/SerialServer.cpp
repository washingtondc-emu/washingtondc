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

#include "Dreamcast.hpp"

#include "SerialServer.hpp"

#ifndef ENABLE_SERIAL_SERVER
#error This file should not be built unless the serial server is enabled
#endif

SerialServer::SerialServer(Sh4 *cpu) {
    this->cpu = cpu;
    listener = NULL;
    bev = NULL;

    ready_to_write = false;

    if (!(outbound = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);
}

SerialServer::~SerialServer() {
    evbuffer_free(outbound);
    if (bev)
        bufferevent_free(bev);
    if (listener)
        evconnlistener_free(listener);
}

void SerialServer::attach() {
    std::cout << "Awaiting serial connection on port " << PORT_NO << "..." <<
        std::endl;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT_NO);
    listener = evconnlistener_new_bind(dc_event_base, listener_cb_static, this,
                                       LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
                                       (struct sockaddr*)&sin, sizeof(sin));

    if (!listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    // the listener_cb will set is_listening = false when we have a connection
    is_listening = true;
    do {
        std::cout << "still waiting..." << std::endl;
        if (event_base_loop(dc_event_base, EVLOOP_ONCE) != 0)
            exit(4);
    } while (is_listening);

    std::cout << "Connection established." << std::endl;
}

void SerialServer::handle_read(struct bufferevent *bev) {
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

        sh4_scif_rx(cpu, cur_byte);
    }

    evbuffer_free(read_buffer);
}

/*
 * this function gets called when boost is done writing
 * and is hungry for more data
 */
void SerialServer::handle_write(struct bufferevent *bev) {
    if (!evbuffer_get_length(outbound)) {
        ready_to_write = true;
        sh4_scif_cts(cpu);
        return;
    }

    bufferevent_write_buffer(bev, outbound);
    ready_to_write = false;
}

void SerialServer::put(uint8_t dat) {
    evbuffer_add(outbound, &dat, sizeof(dat));

    if (ready_to_write) {
        bufferevent_write_buffer(bev, outbound);
        ready_to_write = false;
    }
}

void SerialServer::notify_tx_ready() {
    sh4_scif_cts(cpu);
}

void
SerialServer::listener_cb_static(struct evconnlistener *listener,
                                 evutil_socket_t fd, struct sockaddr *saddr,
                                 int socklen, void *arg) {
    SerialServer *ss = (SerialServer*)arg;
    ss->listener_cb(listener, fd, saddr, socklen);
}

void
SerialServer::listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
                          struct sockaddr *saddr, int socklen) {
    bev = bufferevent_socket_new(dc_event_base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(bev, &SerialServer::handle_read_static,
                      &SerialServer::handle_write_static,
                      &SerialServer::handle_events_static, this);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_enable(bev, EV_READ);

    is_listening = false;
}

void SerialServer::handle_read_static(struct bufferevent *bev, void *arg) {
    SerialServer *ss = (SerialServer*)arg;

    ss->handle_read(bev);
}

/*
 * this function gets called when libevent is done writing
 * and is hungry for more data
 */
void SerialServer::handle_write_static(struct bufferevent *bev, void *arg) {
    SerialServer *ss = (SerialServer*)arg;

    ss->handle_write(bev);
}

void SerialServer::handle_events_static(struct bufferevent *bev, short events,
                                        void *arg) {
    SerialServer *ss = (SerialServer*)arg;

    ss->handle_events(bev, events);
}

void SerialServer::handle_events(struct bufferevent *bev, short events) {
    exit(2);
}
