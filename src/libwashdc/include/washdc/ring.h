/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
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
