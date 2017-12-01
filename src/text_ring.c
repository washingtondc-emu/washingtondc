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

#include <assert.h>
#include <stdio.h>

#include "log.h"

#include "text_ring.h"

void text_ring_init(struct text_ring *ring) {
    ring->prod_idx = ring->cons_idx = 0;
}

void text_ring_produce(struct text_ring *ring, char ch) {
    unsigned next_prod_idx = (ring->prod_idx + 1) & (TEXT_RING_LEN - 1);

    if (next_prod_idx == ring->cons_idx) {
        LOG_WARN("WARNING: text_ring character dropped\n");
        return;
    }

    ring->buf[ring->prod_idx] = ch;
    ring->prod_idx = next_prod_idx;
}

char text_ring_consume(struct text_ring *ring) {
    assert(!text_ring_empty(ring));

    char ch = ring->buf[ring->cons_idx];

    ring->cons_idx = (ring->cons_idx + 1) & (TEXT_RING_LEN - 1);

    return ch;
}

bool text_ring_empty(struct text_ring *ring) {
    return ring->prod_idx == ring->cons_idx;
}

unsigned text_ring_len(struct text_ring *ring) {
    if (ring->prod_idx < ring->cons_idx)
        return TEXT_RING_LEN - ring->cons_idx + ring->prod_idx;
    return ring->prod_idx - ring->cons_idx;
}
