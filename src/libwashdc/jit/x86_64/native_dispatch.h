/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef NATIVE_DISPATCH_H_
#define NATIVE_DISPATCH_H_

#include <stdint.h>

#include "washdc/types.h"
#include "dc_sched.h"
#include "abi.h"

#ifdef JIT_PROFILE
#include "jit/jit_profile.h"
#endif

void native_dispatch_init(struct dc_clock *clk);
void native_dispatch_cleanup(void);

typedef uint32_t(*native_dispatch_entry_func)(uint32_t);

struct jit_code_block;
typedef void(*native_dispatch_compile_func)(void*,struct jit_code_block*,addr32_t);
#ifdef JIT_PROFILE
typedef
void(*native_dispatch_profile_notify_func)(void*,
                                           struct jit_profile_per_block *blk);
#endif

struct native_dispatch_meta {
#ifdef JIT_PROFILE
    native_dispatch_profile_notify_func profile_notify;
#endif
    native_dispatch_compile_func on_compile;
};

/*
 * native_dispatch_check_cycles is a function which updates the cycle counter
 * and returns if it's time to execute an event handler.  Since all the JIT
 * code-blocks tail-call each other, this means that the function it returns to
 * is native_dispatch_entry.
 *
 * This function's first argument is the cycle-count of the code block which was
 * just executed (in EDI)
 *
 * This function's second argument is the new PC (in ESI).
 *
 * This function should not be called from C code.
 */
void
native_check_cycles_emit(void *ctx_ptr, struct native_dispatch_meta const *funcs);

/*
 * native_dispatch_entry is a generated function which saves all call-stack
 * registers which ought to be saved, calls native_dispatch, and then returns
 * after restoring the saved register state.  It is intended to be called from
 * C code.
 */
native_dispatch_entry_func
native_dispatch_entry_create(void *ctx_ptr, struct native_dispatch_meta const *funcs);

#define NATIVE_CHECK_CYCLES_CYCLE_COUNT_REG REG_ARG1
#define NATIVE_CHECK_CYCLES_JUMP_REG REG_ARG0

#endif
