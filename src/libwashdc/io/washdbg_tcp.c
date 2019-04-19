/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include "log.h"
#include "io_thread.h"
#include "washdc/error.h"
#include "dbg/washdbg_core.h"
#include "ring.h"

#include "washdbg_tcp.h"

static enum washdbg_state {
    // washdbg is not in use
    WASHDBG_DISABLED,

    // washdbg is awaiting an incoming connection
    WASHDBG_LISTENING,

    // washdbg is in use
    WASHDBG_ATTACHED
} state;

#define WASHDBG_READ_BUF_LEN_SHIFT 10
#define WASHDBG_READ_BUF_LEN (1 << WASHDBG_READ_BUF_LEN_SHIFT)

static pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t listener_cond = PTHREAD_COND_INITIALIZER;

static struct evconnlistener *listener;

static struct evbuffer *outbound_buf;
static struct bufferevent *bev;

static void listener_lock(void);
static void listener_unlock(void);
static void listener_wait(void);
static void listener_signal(void);

static void washdbg_attach(void *argptr);

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);
static void on_request_listen_event(evutil_socket_t fd, short ev, void *arg);

static void handle_read(struct bufferevent *bev, void *arg);

static void washdbg_run_once(void *argptr);

static void handle_events(struct bufferevent *bev, short events, void *arg);
static void handle_read(struct bufferevent *bev, void *arg);
static void on_check_tx_event(evutil_socket_t fd, short ev, void *arg);

// drain the tx_ring
static void drain_tx(void);

static struct event *request_listen_event, *check_tx_event;

struct debug_frontend washdbg_frontend = {
    .attach = washdbg_attach,
    .run_once = washdbg_run_once,
    .on_break = washdbg_core_on_break,
    .on_cleanup = washdbg_cleanup
};

struct text_ring tx_ring, rx_ring;

void washdbg_tcp_init(void) {
    text_ring_init(&rx_ring);
    text_ring_init(&tx_ring);

    state = WASHDBG_DISABLED;
    outbound_buf = evbuffer_new();
    request_listen_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                         on_request_listen_event, NULL);
    check_tx_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                               on_check_tx_event, NULL);
    LOG_INFO("washdbg initialized\n");
}

void washdbg_tcp_cleanup(void) {
    event_free(check_tx_event);
    event_free(request_listen_event);
    LOG_INFO("washdbg de-initialized\n");
}

/*
 * this gets called from the emulation thread to send text to the remote
 * TCP/IP connection.
 */
int washdbg_tcp_puts(char const *str) {
    int n_chars = 0;
    while (*str) {
        if (!text_ring_produce(&tx_ring, *str)) {
            LOG_WARN("%s - tx_ring failed to produce\n", __func__);
            break;
        }
        str++;
        n_chars++;
    }
    event_active(check_tx_event, 0, 0);
    return n_chars;
}

// drain the tx_ring
static void drain_tx(void) {
    static bool have_extra_char = false;
    static char extra_char;
    char ch;

    if (have_extra_char) {
        if (evbuffer_add(outbound_buf, &extra_char, sizeof(extra_char)) < 0)
            return;
        have_extra_char = false;
    }

    while (text_ring_consume(&tx_ring, &ch)) {
        if (evbuffer_add(outbound_buf, &ch, sizeof(ch)) < 0) {
            extra_char = ch;
            have_extra_char = true;
            break;
        }
    }

    bufferevent_write_buffer(bev, outbound_buf);
}

static void washdbg_run_once(void *argptr) {
    char ch;
    while (text_ring_consume(&rx_ring, &ch))
        washdbg_input_ch(ch);
    washdbg_core_run_once();
}

// this function gets called from the emulation thread.
static void washdbg_attach(void* argptr) {
    printf("washdbg awaiting remote connection on port %d...\n", WASHDBG_PORT);

    listener_lock();

    event_active(request_listen_event, 0, 0);

    listener_wait();

    if (state == WASHDBG_ATTACHED)
        LOG_INFO("WashDbg remote connection established\n");
    else
        LOG_INFO("Failed to establish a remote WashDbg connection.\n");

    listener_unlock();

    washdbg_init();
}

static void on_request_listen_event(evutil_socket_t fd, short ev, void *arg) {
    listener_lock();

    state = WASHDBG_LISTENING;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(WASHDBG_PORT);
    unsigned evflags = LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE |
        LEV_OPT_THREADSAFE;
    listener = evconnlistener_new_bind(io_thread_event_base, listener_cb,
                                       NULL, evflags, -1,
                                       (struct sockaddr*)&sin, sizeof(sin));
    if (!listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

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
        state = WASHDBG_DISABLED;
        goto signal_listener;
    }

    bufferevent_enable(bev, EV_READ);
    bufferevent_setcb(bev, handle_read,
                      NULL, handle_events, NULL);

    state = WASHDBG_ATTACHED;

signal_listener:
    listener_signal();
    listener_unlock();

    drain_tx();
}

/*
 * this is a libevent callback for an event that gets triggered whenever the
 * washdbg_core code calls washdbg_tcp_puts
 */
static void on_check_tx_event(evutil_socket_t fd, short ev, void *arg) {
    if (state == WASHDBG_ATTACHED)
        drain_tx();
}

static void handle_events(struct bufferevent *bev, short events, void *arg) {
    exit(2);
}

// dat should *not* be null-terminated
static void dump_to_rx_ring(char const *dat, unsigned n_chars) {
    unsigned idx;
    for (idx = 0; idx < n_chars; idx++)
        if (dat[idx] == 3)
            debug_request_break();
        else
            text_ring_produce(&rx_ring, dat[idx]);
}

// libevent callback for when the socket has data for us to read
static void handle_read(struct bufferevent *bev, void *arg) {
    static char read_buf[WASHDBG_READ_BUF_LEN];
    struct evbuffer *read_buffer;

    if (!(read_buffer = evbuffer_new())) {
        warnx("WashDbg %s unable to allocate a new evbuffer\n", __func__);
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
        if (read_buf_idx >= WASHDBG_READ_BUF_LEN) {
            dump_to_rx_ring(read_buf, read_buf_idx);
            read_buf_idx = 0;
        }
    }

    // transmit any residual data.
    if (read_buf_idx)
        dump_to_rx_ring(read_buf, read_buf_idx);
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
