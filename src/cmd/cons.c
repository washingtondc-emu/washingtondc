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

#include <stdlib.h>
#include <pthread.h>

#include "cmd_thread.h"
#include "text_ring.h"

#include "cons.h"

// fifo queue for program-output being printed to the console
static struct text_ring cons_txq = TEXT_RING_INITIALIZER;

static pthread_mutex_t cons_mtx = PTHREAD_MUTEX_INITIALIZER;

static void cons_lock(void);
static void cons_unlock(void);

void cons_puts(char const *txt) {
    cons_lock();
    while (*txt)
        text_ring_produce(&cons_txq, *txt++);
    cons_unlock();
}

bool cons_tx_drain_single(char *out) {
    if (text_ring_empty(&cons_txq))
        return false;
    *out = text_ring_consume(&cons_txq);
    return true;
}

static void cons_lock(void) {
    if (pthread_mutex_lock(&cons_mtx) < 0)
        abort(); // TODO: error checking
}

static void cons_unlock(void) {
    if (pthread_mutex_unlock(&cons_mtx) < 0)
        abort(); // TODO: error checking
}
