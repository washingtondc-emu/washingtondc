/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
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

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include "washdc/log.h"
#include "io_thread.hpp"
#include "washdc/error.h"
#include "washdbg_core.hpp"
#include "washdc/washdc.h"

#include "washdbg_tcp.hpp"

#ifndef USE_LIBEVENT
#error this file should not be built with USE_LIBEVENT disabled!
#endif
#ifndef ENABLE_DEBUGGER
#error this file whould not be built with ENABLE_DEBUGGER disabled!
#endif

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

static washdc_mutex listener_mutex = WASHDC_MUTEX_STATIC_INIT;
static washdc_cvar listener_cond = WASHDC_CVAR_STATIC_INIT;

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

template <typename tp, unsigned log>
class washdbg_ring {
    std::atomic_int prod_idx, cons_idx;
    tp buf[1 << log];

public:
    washdbg_ring() : prod_idx(0), cons_idx(0) {
    }

    void reset() {
        prod_idx = 0;
        cons_idx = 0;
    }

    bool produce(tp val) {
        /* TODO: can I use memory_order_relaxed to load prod_idx ?*/
        int prod = prod_idx.load(std::memory_order_acquire);
        int cons = cons_idx.load(std::memory_order_acquire);
        int next_prod = (prod_idx + 1) & ((1 << (log)) - 1);

        if (next_prod == cons) {
            std::cout << "WARNING: washdbg_ring character dropped" << std::endl;
            return false;
        }

        buf[prod] = val;
        prod_idx.store(next_prod, std::memory_order_release);
        return true;
    }

    bool consume(tp *outp) {
        int prod = prod_idx.load(std::memory_order_acquire);
        int cons = cons_idx.load(std::memory_order_acquire);
        int next_cons = (cons + 1) & ((1 << log) - 1);

        if (prod == cons)
            return false;

        *outp = buf[cons];
        cons_idx.store(next_cons, std::memory_order_release);
        return true;
    }
};

static struct event *request_listen_event, *check_tx_event;

struct debug_frontend washdbg_frontend = {
    washdbg_attach, // attach
    washdbg_core_on_break, // on_break
    NULL, // on_read_watchpoint
    NULL, // on_write_watchpoint
    NULL, // on_softbreak
    washdbg_cleanup, // on_cleanup
    washdbg_run_once, // run_once
};

static washdbg_ring<char, 10> tx_ring, rx_ring;

void washdbg_tcp_init(void) {
    state = WASHDBG_DISABLED;
    outbound_buf = evbuffer_new();
    request_listen_event = event_new(io::event_base, -1, EV_PERSIST,
                                     on_request_listen_event, NULL);
    check_tx_event = event_new(io::event_base, -1, EV_PERSIST,
                               on_check_tx_event, NULL);
    std::cout << "washdbg initialized" << std::endl;
}

void washdbg_tcp_cleanup(void) {
    event_free(check_tx_event);
    event_free(request_listen_event);
    std::cout << "washdbg de-initialized" << std::endl;
    rx_ring.reset();
    tx_ring.reset();
}

/*
 * this gets called from the emulation thread to send text to the remote
 * TCP/IP connection.
 */
int washdbg_tcp_puts(char const *str) {
    int n_chars = 0;
    while (*str) {
        if (!tx_ring.produce(*str)) {
            std::cerr << __func__ << " - tx_ring failed to produce" <<
                std::endl;
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

    while (tx_ring.consume(&ch)) {
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
    while (rx_ring.consume(&ch))
        washdbg_input_ch(ch);
    washdbg_core_run_once();
}

// this function gets called from the emulation thread.
static void washdbg_attach(void* argptr) {
    std::cout << "washdbg awaiting remote connection on port " <<
        WASHDBG_PORT << "..." << std::endl;

    listener_lock();

    event_active(request_listen_event, 0, 0);

    listener_wait();

    if (state == WASHDBG_ATTACHED)
        std::cout << "WashDbg remote connection established" << std::endl;
    else
        std::cout << "Failed to establish a remote WashDbg connection." << std::endl;

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
    listener = evconnlistener_new_bind(io::event_base, listener_cb,
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

    bev = bufferevent_socket_new(io::event_base, fd,
                                 BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Unable to allocate a new bufferevent\n");
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
    std::cerr << __func__ << " called: \"" << ev_type << "\" (" << events
              << ") event received; calling washdc_kill" << std::endl;
    washdc_kill();
}

// dat should *not* be null-terminated
static void dump_to_rx_ring(char const *dat, unsigned n_chars) {
    unsigned idx;
    for (idx = 0; idx < n_chars; idx++)
        if (dat[idx] == 3)
            debug_request_break();
        else
            rx_ring.produce(dat[idx]);
}

// libevent callback for when the socket has data for us to read
static void handle_read(struct bufferevent *bev, void *arg) {
    static char read_buf[WASHDBG_READ_BUF_LEN];
    struct evbuffer *read_buffer;

    if (!(read_buffer = evbuffer_new())) {
        fprintf(stderr, "WashDbg %s unable to allocate a new evbuffer\n",
                __func__);
        return;
    }

    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);

    bool potential_break = false;

    size_t idx;
    unsigned read_buf_idx = 0;
    for (idx = 0; idx < buflen; idx++) {
        uint8_t tmp;
        if (evbuffer_remove(read_buffer, &tmp, sizeof(tmp)) < 0) {
            fprintf(stderr, "CMD_THREAD %s unable to remove text\n", __func__);
            continue;
        }

        if (potential_break) {
            if ((char)tmp == -13) {
                printf("line break!\n");
                debug_request_break();
                continue;
            } else {
                potential_break = false;
            }
        } else if ((char)tmp == -1) {
            potential_break = true;
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
    washdc_mutex_lock(&listener_mutex);
}

static void listener_unlock(void) {
    washdc_mutex_unlock(&listener_mutex);
}

static void listener_wait(void) {
    washdc_cvar_wait(&listener_cond, &listener_mutex);
}

static void listener_signal(void) {
    washdc_cvar_signal(&listener_cond);
}
