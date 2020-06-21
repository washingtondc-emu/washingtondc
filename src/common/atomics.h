/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
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

#ifndef WASHDC_ATOMICS_H_
#define WASHDC_ATOMICS_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
/*
 * UGGGGGGGGGGHHHHHH
 *
 * C11 introduced platform-independent atomic variables to the C programming
 * language but MSVC doesn't implement C11 so I have to come up with a dumb
 * wrapper anyways.
 */

#include "i_hate_windows.h"

#define WASHDC_ATOMIC_FLAG_INIT 0

typedef LONG volatile washdc_atomic_flag;

static inline bool washdc_atomic_flag_test_and_set(washdc_atomic_flag *flag) {
    return InterlockedCompareExchange(flag, 1, 0);
}

static inline void washdc_atomic_flag_clear(washdc_atomic_flag *flag) {
    InterlockedCompareExchange(flag, 0, 1);
}

#define WASHDC_ATOMIC_INT_INIT(val) val
typedef LONG volatile washdc_atomic_int;
static inline bool
washdc_atomic_int_compare_exchange(washdc_atomic_int *atom,
                                   int *expect, int new_val) {
    LONG expect_orig = *expect;
    LONG init_val = InterlockedCompareExchange(atom, new_val, expect_orig);

    if (init_val == expect_orig) {
        return true;
    } else {
        *expect = init_val;
        return false;
    }
}

static inline int
washdc_atomic_int_load(washdc_atomic_int *atom) {
    return InterlockedCompareExchange(atom, 0, 0);
}

static inline void washdc_atomic_int_init(washdc_atomic_int *atom, int val) {
    *atom = val;
}

#else
/*
 * Here we foolishly assume that any compiler which isn't MSVC will support C11
 * because that's how standards are supposed to work.
 */
#include <stdatomic.h>

#define WASHDC_ATOMIC_FLAG_INIT ATOMIC_FLAG_INIT

typedef atomic_flag washdc_atomic_flag;

static inline bool washdc_atomic_flag_test_and_set(washdc_atomic_flag *flag) {
    return atomic_flag_test_and_set(flag);
}

static inline void washdc_atomic_flag_clear(washdc_atomic_flag *flag) {
    atomic_flag_clear(flag);
}

#define WASHDC_ATOMIC_INT_INIT(val) ATOMIC_VAR_INIT(val)

typedef atomic_int washdc_atomic_int;
static inline bool
washdc_atomic_int_compare_exchange(washdc_atomic_int *atom,
                                   int *expect, int new_val) {
    return atomic_compare_exchange_strong(atom, expect, new_val);
}

static inline int
washdc_atomic_int_load(washdc_atomic_int *atom) {
    return atomic_load(atom);
}

static inline void washdc_atomic_int_init(washdc_atomic_int *atom, int val) {
    atomic_init(atom, val);
}

#endif

#ifdef __cplusplus
}
#endif

#endif
