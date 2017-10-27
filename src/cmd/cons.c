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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "cmd_sys.h"
#include "text_ring.h"

#include "cons.h"

// fifo queue for program-output being printed to the console
static struct text_ring cons_txq = TEXT_RING_INITIALIZER;

// fifo queue for user-input text
static struct text_ring cons_rxq = TEXT_RING_INITIALIZER;

static pthread_mutex_t cons_tx_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cons_rx_mtx = PTHREAD_MUTEX_INITIALIZER;

static void cons_tx_prod_lock(void);
static void cons_tx_prod_unlock(void);

static void cons_rx_prod_lock(void);
static void cons_rx_prod_unlock(void);

static void cons_rx_recv_single_no_lock(char ch);

void cons_puts(char const *txt) {
    cons_tx_prod_lock();
    while (*txt)
        text_ring_produce(&cons_txq, *txt++);
    cons_tx_prod_unlock();
}

#define CONS_PRINTF_BUF_LEN 128

void cons_printf(char const *txt, ...) {
    char buf[CONS_PRINTF_BUF_LEN];
    va_list ap;

    va_start(ap, txt);
    vsnprintf(buf, CONS_PRINTF_BUF_LEN, txt, ap);
    buf[CONS_PRINTF_BUF_LEN - 1] = '\0';
    va_end(ap);

    cons_puts(buf);
}

bool cons_getc(char *ch) {
    if (text_ring_empty(&cons_rxq))
        return false;
    *ch = text_ring_consume(&cons_rxq);
    return true;
}

bool cons_tx_drain_single(char *out) {
    if (text_ring_empty(&cons_txq))
        return false;
    *out = text_ring_consume(&cons_txq);
    return true;
}

void cons_rx_recv_single(char ch) {
    cons_rx_prod_lock();
    cons_rx_recv_single_no_lock(ch);
    cons_rx_prod_unlock();
}

void cons_rx_recv_text(char const *txt) {
    cons_rx_prod_lock();
    while (*txt)
        cons_rx_recv_single_no_lock(*txt++);
    cons_rx_prod_unlock();
}

static void cons_rx_recv_single_no_lock(char ch) {
    text_ring_produce(&cons_rxq, ch);
}

static void cons_tx_prod_lock(void) {
    if (pthread_mutex_lock(&cons_tx_mtx) < 0)
        abort(); // TODO: error checking
}

static void cons_tx_prod_unlock(void) {
    if (pthread_mutex_unlock(&cons_tx_mtx) < 0)
        abort(); // TODO: error checking
}

static void cons_rx_prod_lock(void) {
    if (pthread_mutex_lock(&cons_rx_mtx) < 0)
        abort(); // TODO: error checking
}

static void cons_rx_prod_unlock(void) {
    if (pthread_mutex_unlock(&cons_rx_mtx) < 0)
        abort(); // TODO: error checking
}
