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

#include <err.h>
#include <stdlib.h>
#include <pthread.h>

#include "cmd.h"
#include "cons.h"
#include "dreamcast.h"
#include "cmd_tcp_link.h"

#include "cmd_thread.h"

static pthread_mutex_t cmd_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cmd_thread_create_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cmd_thread_cond = PTHREAD_COND_INITIALIZER;

static pthread_t cmd_thread;

static void cmd_thread_lock(void);
static void cmd_thread_unlock(void);
static void cmd_thread_wait(void);
static void cmd_thread_signal(void);

static void *cmd_thread_main(void *arg);

static void cmd_thread_drain_cons_tx(void);
static void cmd_thread_drain_cons_rx(void);

// dump the given string onto all of the cmd frontends
static void cmd_thread_print_no_lock(char const *txt);

void cmd_thread_launch(void) {
    int err_code;

    cmd_thread_lock();

    if ((err_code = pthread_create(&cmd_thread, NULL, cmd_thread_main, NULL)) != 0)
        err(1, "unable to launch CMD thread");

    if (pthread_cond_wait(&cmd_thread_create_cond, &cmd_thread_mutex) != 0)
        abort(); // TODO: error handling

    cmd_thread_unlock();
}

void cmd_thread_join(void) {
    pthread_join(cmd_thread, NULL);
}

static void *cmd_thread_main(void *arg) {
    cmd_thread_lock();

    if (pthread_cond_signal(&cmd_thread_create_cond) < 0)
        abort(); // TODO: error handling

    while (dc_is_running()) {
        /*
         * the ordering here is very important.  We have to drain the tx last
         * because we want to make sure that any calls to cons_put that come
         * from the cmd_thread get processed.  cmd_thread_kick is not viable
         * from within the cmd_thread because it would deadlock trying to grab
         * the cmd_thread_lock.
         */
        cmd_thread_drain_cons_rx();
        cmd_thread_drain_cons_tx();

        cmd_thread_wait();
    }

    cmd_thread_unlock();

    pthread_exit(NULL);
    return NULL; // this line never executes
}

void cmd_thread_kick(void) {
    cmd_thread_lock();
    cmd_thread_signal();
    cmd_thread_unlock();
}

void cmd_thread_put_char(char c) {
    cmd_put_char(c);
}

/*
 * this function drains the cons tx ring and sends data one line at a time.
 *
 * If there's any data left over after the last line or there are lines longer
 * than 1024 characters, then this function can send partial lines.
 */
#define CONS_BUF_LINE_LEN_SHIFT 10
#define CONS_BUF_LINE_LEN (1 << CONS_BUF_LINE_LEN_SHIFT)
static void cmd_thread_drain_cons_tx(void) {
    static char cons_buf_line[CONS_BUF_LINE_LEN];
    unsigned idx = 0;
    char ch;

    while (cons_tx_drain_single(&ch)) {
        cons_buf_line[idx++] = ch;

        if ((idx == (CONS_BUF_LINE_LEN - 1)) || (ch == '\n')) {
            cons_buf_line[idx] = '\0';
            cmd_thread_print_no_lock(cons_buf_line);
            idx = 0;
        }
    }

    if (idx) {
        cons_buf_line[idx] = '\0';
        cmd_thread_print_no_lock(cons_buf_line);
    }
}

static void cmd_thread_drain_cons_rx(void) {
    char ch;
    while (cons_getc(&ch))
        cmd_thread_put_char(ch);
}

static void cmd_thread_print_no_lock(char const *txt) {
    // TODO: pass data on to other frontends here, too
    cmd_tcp_link_put_text(txt);
}

static void cmd_thread_lock(void) {
    if (pthread_mutex_lock(&cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_unlock(void) {
    if (pthread_mutex_unlock(&cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_wait(void) {
    if (pthread_cond_wait(&cmd_thread_cond, &cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_signal(void) {
    if (pthread_cond_signal(&cmd_thread_cond) < 0)
        abort(); // TODO: error handling
}
