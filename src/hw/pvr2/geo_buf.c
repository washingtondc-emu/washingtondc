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
#include <pthread.h>

#include "dreamcast.h"

#include "geo_buf.h"

#define GEO_BUF_COUNT 4

static pthread_cond_t cons_adv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cons_mtx = PTHREAD_MUTEX_INITIALIZER;

// producer and consumer indices
static volatile unsigned prod_idx, cons_idx;

static struct geo_buf ringbuf[GEO_BUF_COUNT];

static unsigned next_frame_stamp;
static unsigned last_prod_frame_stamp;

struct geo_buf *geo_buf_get_cons(void) {
    struct geo_buf *ret;

    // TODO: maybe I don't need to grab the mutex here since it's read-only
    if (pthread_mutex_lock(&cons_mtx) != 0)
        abort(); // TODO: error handling

    if (prod_idx == cons_idx)
        ret = NULL;
    else
        ret = ringbuf + cons_idx;

    if (pthread_mutex_unlock(&cons_mtx) != 0)
        abort(); // TODO: error handling

    return ret;
}

struct geo_buf *geo_buf_get_prod(void) {
    return ringbuf + prod_idx;
}

void geo_buf_consume(void) {
    if (pthread_mutex_lock(&cons_mtx) != 0)
        abort(); // TODO: error handling

    if (prod_idx == cons_idx) {
        fprintf(stderr, "WARNING: attempt to consume from empty geo_buf "
                "ring\n");
    } else {
        cons_idx = (cons_idx + 1) % GEO_BUF_COUNT;
        if (pthread_cond_signal(&cons_adv) != 0)
            abort(); // TODO: error handling
    }

    if (pthread_mutex_unlock(&cons_mtx) != 0)
        abort(); // TODO: error handling
}

void geo_buf_produce(void) {
    unsigned next_prod_idx = (prod_idx + 1) % GEO_BUF_COUNT;

    if (pthread_mutex_lock(&cons_mtx) != 0)
        abort(); // TODO: error handling
    if (next_prod_idx == cons_idx) {
        /*
         * TODO: this could cause deadlocks maybe since last_prod_frame_stamp
         * will be incorrect
         */
        fprintf(stderr, "WARNING %s: prod_idx == %u, cons_idx == %u.  "
                "Waiting for ring to drain...\n", __func__, prod_idx, cons_idx);
        while (next_prod_idx == cons_idx && dc_is_running())
            pthread_cond_wait(&cons_adv, &cons_mtx);
        fprintf(stderr, "the ring has drained\n");
    }

    prod_idx = next_prod_idx;
    last_prod_frame_stamp = ringbuf[prod_idx].frame_stamp;

    if (pthread_mutex_unlock(&cons_mtx) != 0)
        abort(); // TODO: error handling

    ringbuf[prod_idx].frame_stamp = ++next_frame_stamp;
    ringbuf[prod_idx].n_verts = 0;
}

unsigned geo_buf_latest_frame_stamp(void) {
    return last_prod_frame_stamp;
}
