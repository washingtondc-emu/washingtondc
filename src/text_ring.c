/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <assert.h>
#include <stdio.h>

#include "log.h"

#include "text_ring.h"

void text_ring_init(struct text_ring *ring) {
    atomic_init(&ring->prod_idx, 0);
    atomic_init(&ring->cons_idx, 0);
}

bool text_ring_produce(struct text_ring *ring, char ch) {
    int prod_idx = atomic_load(&ring->prod_idx);
    int cons_idx = atomic_load(&ring->cons_idx);
    int next_prod_idx = (prod_idx + 1) & TEXT_RING_MASK;

    if (next_prod_idx == cons_idx) {
        LOG_WARN("WARNING: text_ring character dropped\n");
        return false;
    }

    ring->buf[next_prod_idx] = ch;
    atomic_store(&ring->prod_idx, next_prod_idx);

    return true;
}

/*
 * XXX I want this function to be safe for multiple consumers
 *
 * That said, this probably isn't good for multiple consumers because of the
 * unlikely-yet-possible case that some other consumer very quickly consumes
 * the entirety of the ringbuffer.  That would cause cons_idx to wrap around to
 * its initial position, meaning that the value of ch is invalid but we don't
 * know that.
 *
 * Single consumer is definitely safe, though.  Just be wary of multiple consumers.
 */
bool text_ring_consume(struct text_ring *ring, char *outp) {
    char ch;
    int prod_idx, cons_idx, next_cons_idx;

    do {
        prod_idx = atomic_load(&ring->prod_idx);
        cons_idx = atomic_load(&ring->cons_idx);
        next_cons_idx = (cons_idx + 1) & TEXT_RING_MASK;

        if (prod_idx == cons_idx)
            return false;

        ch = ring->buf[ring->cons_idx];
    } while (!atomic_compare_exchange_strong(&ring->cons_idx,
                                             &cons_idx,
                                             next_cons_idx));

    *outp = ch;
    return true;
}
