/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018, 2020 snickerbockers
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

#include "atomics.h"
#include "log.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is a ringbuffer designed to buffer text between threads.
 * In the event of an overflow, this buffer will drop incoming data at the
 * producer-side.
 *
 * This ringbuffer is SINGLE CONSUMER, SINGLE PRODUCER ONLY!
 */


#define RING_INITIALIZER                                                \
    { .prod_idx = WASHDC_ATOMIC_INT_INIT(0), .cons_idx = WASHDC_ATOMIC_INT_INIT(0) }


#define DEF_RING(name, tp, log)                                         \
    struct name {                                                       \
        washdc_atomic_int prod_idx, cons_idx;                           \
                                                                        \
        tp buf[1 << (log)];                                             \
    };                                                                  \
                                                                        \
    static inline void name##_init(struct name *ring) {                 \
        ring->prod_idx = WASHDC_ATOMIC_INT_INIT(0);                     \
        ring->cons_idx = WASHDC_ATOMIC_INT_INIT(0);                     \
    }                                                                   \
                                                                        \
    /* return true if the operation succeeded, false if it failed. */   \
    static inline bool                                                  \
    name##_produce(struct name *ring, tp val) {                         \
        int prod_idx = washdc_atomic_int_load(&ring->prod_idx);         \
        /*memory_order_acquire); */                                     \
        int cons_idx = washdc_atomic_int_load(&ring->cons_idx);         \
        /* memory_order_acquire); */                                    \
        int next_prod_idx = (prod_idx + 1) & ((1 << (log)) - 1);        \
                                                                        \
        if (next_prod_idx == cons_idx) {                                \
            washdc_log_warn("WARNING: text_ring character dropped\n");  \
            return false;                                               \
        }                                                               \
                                                                        \
        ring->buf[prod_idx] = val;                                      \
        if (!washdc_atomic_int_compare_exchange(&ring->prod_idx,        \
                                                &prod_idx,              \
                                                next_prod_idx)) {       \
            washdc_log_error("%s failed to update ring - THIS IS FOR "  \
                             "SINGLE-PRODUCER ONLY YOU DOOFUS\n",       \
                             __func__);                                 \
            return false;                                               \
        }                                                               \
                                                                        \
        return true;                                                    \
    }                                                                   \
                                                                        \
    /* return true if the operation succeeded, false if it failed. */   \
    static inline bool                                                  \
    name##_consume(struct name *ring, tp *outp) {                       \
        int prod_idx, cons_idx, next_cons_idx;                          \
                                                                        \
        /* TODO: can I use memory_order_relaxed to load cons_idx ?*/    \
        cons_idx = washdc_atomic_int_load(&ring->cons_idx);             \
        /* memory_order_acquire); */                                    \
        prod_idx = washdc_atomic_int_load(&ring->prod_idx);             \
        /* memory_order_acquire); */                                    \
        next_cons_idx = (cons_idx + 1) & ((1 << (log)) - 1);            \
                                                                        \
        if (prod_idx == cons_idx)                                       \
            return false;                                               \
                                                                        \
        *outp = ring->buf[ring->cons_idx];                              \
        if (!washdc_atomic_int_compare_exchange(&ring->cons_idx,        \
                                                &cons_idx,              \
                                                next_cons_idx)) {       \
            washdc_log_error("%s failed to update ring - THIS IS FOR "  \
                             "SINGLE-CONSUMER ONLY YOU DOOFUS\n",       \
                             __func__);                                 \
            return false;                                               \
        }                                                               \
                                                                        \
        return true;                                                    \
    }                                                                   \

DEF_RING(text_ring, char, 10)

#ifdef __cplusplus
}
#endif

#endif
