/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
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
