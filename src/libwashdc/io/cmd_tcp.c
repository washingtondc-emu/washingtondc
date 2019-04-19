/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include "cmd/cons.h"
#include "washdc/error.h"
#include "ring.h"
#include "io_thread.h"
#include "cmd/cmd_sys.h"
#include "log.h"

#include "cmd_tcp.h"

enum cmd_tcp_state {
    // the cmd_tcp system is not in use
    CMD_TCP_DISABLED,

    // the cmd_tcp system is awaiting an incoming connection
    CMD_TCP_LISTENING,

    // the cmd_tcp system is in use
    CMD_TCP_ATTACHED
};

static struct text_ring tx_ring = RING_INITIALIZER;
static struct text_ring rx_ring = RING_INITIALIZER;

static enum cmd_tcp_state state = CMD_TCP_DISABLED;

static struct event *check_tx_event;

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);

static void listener_lock(void);
static void listener_unlock(void);
static void listener_wait(void);
static void listener_signal(void);

static void handle_read(struct bufferevent *bev, void *arg);

static void on_check_tx_event(evutil_socket_t fd, short ev, void *arg);

static void drain_tx(void);

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t listener_cond = PTHREAD_COND_INITIALIZER;

static struct evconnlistener *listener;

static struct evbuffer *outbound_buf;
static struct bufferevent *bev;

void cmd_tcp_init(void) {
    state = CMD_TCP_DISABLED;
    outbound_buf = evbuffer_new();
    check_tx_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                               on_check_tx_event, NULL);
}

void cmd_tcp_attach(void) {
    LOG_INFO("Awaiting remote cli connection on port %d...\n", CMD_TCP_PORT_NO);

    listener_lock();

    if (!outbound_buf) {
        LOG_ERROR("evbuffer_new returned NULL!\n");
        goto unlock;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(CMD_TCP_PORT_NO);
    unsigned evflags = LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE |
        LEV_OPT_CLOSE_ON_FREE;
    listener = evconnlistener_new_bind(io_thread_event_base, listener_cb,
                                       NULL, evflags, -1,
                                       (struct sockaddr*)&sin, sizeof(sin));
    if (!listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    state = CMD_TCP_LISTENING;
    do {
        listener_wait();
    } while (state == CMD_TCP_LISTENING);

    if (state == CMD_TCP_ATTACHED)
        LOG_INFO("CMD remote connection established\n");
    else
        LOG_INFO("Failed to establish a remote CMD TCP/IP connection.\n");

unlock:
    listener_unlock();
}

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    listener_lock();

    bev = bufferevent_socket_new(io_thread_event_base, fd,
                                 BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        warnx("Unable to allocate a new bufferevent\n");
        state = CMD_TCP_DISABLED;
        goto signal_listener;
    }

    bufferevent_setcb(bev, handle_read, NULL, NULL, NULL);
    bufferevent_enable(bev, EV_READ);

    state = CMD_TCP_ATTACHED;

signal_listener:
    listener_signal();
    listener_unlock();

    drain_tx();
}

void cmd_tcp_cleanup(void) {
    event_free(check_tx_event);

    if (state == CMD_TCP_ATTACHED)
        bufferevent_free(bev);

    if (outbound_buf)
        evbuffer_free(outbound_buf);

    outbound_buf = NULL;
    bev = NULL;
}

#define CMD_TCP_READ_BUF_LEN_SHIFT 10
#define CMD_TCP_READ_BUF_LEN (1 << CMD_TCP_READ_BUF_LEN_SHIFT)

// libevent callback for when the socket has data for us to read
static void handle_read(struct bufferevent *bev, void *arg) {
    static char read_buf[CMD_TCP_READ_BUF_LEN];
    struct evbuffer *read_buffer;

    if (!(read_buffer = evbuffer_new())) {
        warnx("CMD_THREAD %s unable to allocate a new evbuffer\n", __func__);
        return;
    }

    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);

    size_t idx;
    unsigned read_buf_idx = 0;
    for (idx = 0; idx < buflen; idx++) {
        uint8_t tmp;
        if (evbuffer_remove(read_buffer, &tmp, sizeof(tmp)) < 0) {
            warnx("CMD_THREAD %s unable to remove text\n", __func__);
            continue;
        }

        /*
         * transmit data in CMD_TCP_READ_BUF_LEN-sized chunks.
         * Some characters will get dropped if the buffer overflows
         */
        read_buf[read_buf_idx++] = (char)tmp;
        if (read_buf_idx >= (CMD_TCP_READ_BUF_LEN - 1)) {
            read_buf[CMD_TCP_READ_BUF_LEN - 1] = '\0';
            cons_rx_recv_text(read_buf);
            read_buf_idx = 0;
        }
    }

    // transmit any residual data.
    if (read_buf_idx) {
        read_buf[read_buf_idx] = '\0';
        cons_rx_recv_text(read_buf);
    }
}

/*
 * this is a libevent callback for an event that gets triggered whenever the
 * cmd code calls cmd_tcp_put
 */
static void on_check_tx_event(evutil_socket_t fd, short ev, void *arg) {
    if (state == CMD_TCP_ATTACHED)
        drain_tx();
}

// drain the tx_ring
static void drain_tx(void) {
    char ch;
    while (text_ring_consume(&tx_ring, &ch)) {
        if (evbuffer_add(outbound_buf, &ch, sizeof(ch)) < 0)
            warnx("%s - evbuffer_add returned an error\n", __func__);
    }

    bufferevent_write_buffer(bev, outbound_buf);
}

/*
 * write a string to the tcp frontend.
 * This function gets called from the cmd thread
 */
void cmd_tcp_put_text(char const *txt) {
    while (*txt)
        text_ring_produce(&tx_ring, *txt++);

    event_active(check_tx_event, 0, 0);
}

/*
 * read a character from the tcp frontend and store it in *out
 *
 * returns true if the operation succeeded, else returns false
 *
 * this function gets called from the emu thread
 */
bool cmd_tcp_get(char *out) {
    return text_ring_consume(&rx_ring, out);
}

static void listener_lock(void) {
    if (pthread_mutex_lock(&listener_mutex) < 0)
        abort(); // TODO error handling
}

static void listener_unlock(void) {
    if (pthread_mutex_unlock(&listener_mutex) < 0)
        abort(); // TODO error handling
}

static void listener_wait(void) {
    if (pthread_cond_wait(&listener_cond, &listener_mutex) < 0)
        abort(); // TODO: error handling
}

static void listener_signal(void) {
    if (pthread_cond_signal(&listener_cond) < 0)
        abort(); // TODO error handling
}
