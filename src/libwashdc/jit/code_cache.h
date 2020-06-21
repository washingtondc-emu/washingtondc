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

#ifndef CODE_CACHE_H_
#define CODE_CACHE_H_

#include "avl.h"
#include "code_block.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/code_block_x86_64.h"
#endif

#include "washdc/types.h"

#include "defs.h"

/*
 * TODO: need to include FPU state in code cache, not just address.
 * Otherwise, this code will trip over anything that tries to switch
 * between single-precision and double-precision floating-point.
 */
struct cache_entry {
    struct avl_node node;

    uint8_t valid;
    struct jit_code_block blk;
};

/*
 * this might return a pointer to an invalid cache_entry.  If so, that means
 * the cache entry needs to be filled in by the callee.  This function will
 * allocate a new invalid cache entry if there is no entry for addr.
 *
 * That said, blk will already be init'd no matter what, even if valid is
 * false.
 */
struct cache_entry *code_cache_find(jit_hash hash);

/*
 * This is like code_cache_find, but it skips the second-level hash table.
 * This function is intended for JIT code which handles that itself
 */
struct cache_entry *code_cache_find_slow(jit_hash hash);

void code_cache_invalidate_all(void);

void code_cache_init(void);
void code_cache_cleanup(void);

/*
 * call this periodically from outside of CPU context to clear
 * out old cache entries.
 */
void code_cache_gc(void);

/*
 * set the value that the code_cache_tbl gets overwritten with whenever there's
 * a nuke
 */
void code_cache_set_default(void *dflt);

#define CODE_CACHE_HASH_TBL_SHIFT 16
#define CODE_CACHE_HASH_TBL_LEN (1 << CODE_CACHE_HASH_TBL_SHIFT)
#define CODE_CACHE_HASH_TBL_MASK (CODE_CACHE_HASH_TBL_LEN - 1)
extern struct cache_entry* code_cache_tbl[CODE_CACHE_HASH_TBL_LEN];

#endif
