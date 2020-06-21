/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
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

#ifndef NATIVE_DISPATCH_H_
#define NATIVE_DISPATCH_H_

#include <stdint.h>

#include "washdc/types.h"
#include "dc_sched.h"
#include "abi.h"
#include "jit/defs.h"

#ifdef JIT_PROFILE
#include "jit/jit_profile.h"
#endif

#include "jit/code_cache.h"

struct native_dispatch_meta;

void native_dispatch_init(struct native_dispatch_meta *meta, void *ctx_ptr);
void native_dispatch_cleanup(struct native_dispatch_meta *meta);

#define NATIVE_DISPATCH_PC_REG REG_ARG0
#define NATIVE_DISPATCH_HASH_REG REG_ARG1
#define NATIVE_DISPATCH_CYCLE_COUNT_REG REG_ARG2

// the first uint32_t parameter is supposed to be the PC, second is the hash
typedef uint32_t(*native_dispatch_entry_func)(uint32_t, uint32_t);

struct jit_code_block;
typedef
void(*native_dispatch_compile_func)(void*,struct native_dispatch_meta const*,
                                    struct jit_code_block*,addr32_t);

typedef jit_hash(*native_dispatch_hash_func)(void*,uint32_t);

#ifdef JIT_PROFILE
typedef
void(*native_dispatch_profile_notify_func)(void*,
                                           struct jit_profile_per_block *blk);
#endif

struct native_dispatch_meta {
    /*
     * Only fields which are explicitly marked as "user-specified" are to be
     * filled-in by the user.  All other fields are initialized by
     * native_dispatch_init and cleaned up by native_dispatch_cleanup.
     */

    dc_cycle_stamp_t *clock_vals;

    struct dc_clock *clk;
    void *return_fn;
    void *dispatch_slow_path;
#ifdef JIT_PROFILE
    void *profile_code;
#endif

    void *ctx_ptr;

#ifdef JIT_PROFILE
    native_dispatch_profile_notify_func profile_notify; // user-specified
#endif
    native_dispatch_compile_func on_compile; // user-specified

    /*
     * entry is a generated function which saves all call-stack registers which
     * ought to be saved, calls native_dispatch, and then returns after
     * restoring the saved register state.  It is intended to be called from C
     * code.
     */
    native_dispatch_entry_func entry;

    /*
     * This is the default "invalid" code block that we fill out the
     * code_cache_tbl with whenever the cache gets nuked.  The idea is that
     * this will point to a fake code block which is equivalent to the slow-path
     * from the native_dispatch code.  The point of all this is to avoid
     * needing to check for NULL pointers in the code created by
     * native_dispatch_emit; otherwise there needs to be an additional branch to
     * make sure that whatever we grab from the code_cache_tbl actually points
     * to a real code block.
     */
    void *trampoline;

    // cache_entry that points to trampoline
    struct cache_entry fake_cache_entry;

    native_dispatch_hash_func hash_func;
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
native_check_cycles_emit(struct native_dispatch_meta const *meta);

#endif
