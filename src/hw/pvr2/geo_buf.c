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

#include "geo_buf.h"

#define GEO_BUF_COUNT 4

// producer and consumer indices
static volatile unsigned prod_idx, cons_idx;

static struct geo_buf ringbuf[GEO_BUF_COUNT];

static unsigned next_frame_stamp;
static unsigned last_prod_frame_stamp;

struct geo_buf *geo_buf_get_cons(void) {
    if (prod_idx == cons_idx)
        return NULL;
    return ringbuf + cons_idx;
}

struct geo_buf *geo_buf_get_prod(void) {
    return ringbuf + prod_idx;
}

void geo_buf_consume(void) {
    if (prod_idx == cons_idx) {
        fprintf(stderr, "WARNING: attempt to consume from empty geo_buf "
                "ring\n");
    } else {
        cons_idx = (cons_idx + 1) % GEO_BUF_COUNT;
    }
}

void geo_buf_produce(void) {
    unsigned next_prod_idx = (prod_idx + 1) % GEO_BUF_COUNT;

    last_prod_frame_stamp = ringbuf[prod_idx].frame_stamp;

    if (next_prod_idx == cons_idx) {
        /*
         * TODO: this could cause deadlocks maybe since last_prod_frame_stamp
         * will be incorrect
         */
        fprintf(stderr, "WARNING: geo_buf DROPPED DUE TO RING OVERFLOW\n");
#ifdef INVARIANTS
        abort();
#endif
    } else {
        prod_idx = next_prod_idx;
    }

    ringbuf[prod_idx].frame_stamp = ++next_frame_stamp;
    ringbuf[prod_idx].n_verts = 0;
}

unsigned geo_buf_latest_frame_stamp(void) {
    return last_prod_frame_stamp;
}
