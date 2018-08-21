/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017. 2018 snickerbockers
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

#ifndef TEXT_RING_H_
#define TEXT_RING_H_

#include <stdbool.h>
#include <stdatomic.h>

/*
 * This is a ringbuffer designed to buffer text between threads.
 * In the event of an overflow, this buffer will drop incoming data at the
 * producer-side.
 *
 * This ringbuffer is SINGLE CONSUMER, SINGLE PRODUCER ONLY!
 */

#define TEXT_RING_LEN_SHIFT 10
#define TEXT_RING_LEN (1 << TEXT_RING_LEN_SHIFT)
#define TEXT_RING_MASK (TEXT_RING_LEN - 1)

struct text_ring {
    atomic_int prod_idx, cons_idx;

    char buf[TEXT_RING_LEN];
};

#define TEXT_RING_INITIALIZER                                           \
    { .prod_idx = ATOMIC_VAR_INIT(0), .cons_idx = ATOMIC_VAR_INIT(0) }

void text_ring_init(struct text_ring *ring);

// these functions return true if the operation succeeded, false if it failed.
bool text_ring_produce(struct text_ring *ring, char ch);
bool text_ring_consume(struct text_ring *ring, char *outp);

#endif
