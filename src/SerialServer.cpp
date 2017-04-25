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
    is_writing = false;
    this->cpu = cpu;
    listener = NULL;
    bev = NULL;
}

SerialServer::~SerialServer() {
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
    // TODO: input_queue doesn't actually need to exist, does it ?
    struct evbuffer *read_buffer;

    if (!(read_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);
    for (unsigned idx = 0; idx < buflen ; idx++) {
        uint8_t tmp;
        if (evbuffer_remove(read_buffer, &tmp, sizeof(tmp)) < 0)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        input_queue.push(tmp);
    }

    // now send the data to the SCIF one char at a time
    while (input_queue.size()) {
        sh4_scif_rx(cpu, input_queue.front());
        input_queue.pop();
    }

    evbuffer_free(read_buffer);
}

void SerialServer::write_start() {
    if (output_queue.empty())
        is_writing = false;

    if (is_writing || output_queue.empty())
        return;

    struct evbuffer *write_buffer;
    if (!(write_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    while (!output_queue.empty()) {
        uint8_t dat = output_queue.front();
        evbuffer_add(write_buffer, &dat, sizeof(dat));
        output_queue.pop();
    }

    bufferevent_write_buffer(bev, write_buffer);
    evbuffer_free(write_buffer);
}

/*
 * this function gets called when boost is done writing
 * and is hungry for more data
 */
void SerialServer::handle_write(struct bufferevent *bev) {
    is_writing = false;

    if (output_queue.empty()) {
        sh4_scif_cts(cpu);

        if (output_queue.empty())
            return;
    }

    write_start();
}

void SerialServer::put(uint8_t dat) {
    output_queue.push(dat);

    write_start();
}

void SerialServer::notify_tx_ready() {
    if (output_queue.empty())
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
