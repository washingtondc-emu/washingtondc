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

#ifndef CODE_CACHE_H_
#define CODE_CACHE_H_

#include "avl.h"
#include "code_block.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/code_block_x86_64.h"
#endif

#include "washdc/types.h"

/*
 * TODO: need to include FPU state in code cache, not just address.
 * Otherwise, this code will trip over anything that tries to switch
 * between single-precision and double-precision floating-point.
 */
struct cache_entry {
    struct avl_node node;

    uint8_t valid;
    union jit_code_block blk;
};

/*
 * this might return a pointer to an invalid cache_entry.  If so, that means
 * the cache entry needs to be filled in by the callee.  This function will
 * allocate a new invalid cache entry if there is no entry for addr.
 *
 * That said, blk will already be init'd no matter what, even if valid is
 * false.
 */
struct cache_entry *code_cache_find(addr32_t addr);

/*
 * This is like code_cache_find, but it skips the second-level hash table.
 * This function is intended for JIT code which handles that itself
 */
struct cache_entry *code_cache_find_slow(addr32_t addr);

void code_cache_invalidate_all(void);

void code_cache_init(void);
void code_cache_cleanup(void);

/*
 * call this periodically from outside of CPU context to clear
 * out old cache entries.
 */
void code_cache_gc(void);

#define CODE_CACHE_HASH_TBL_SHIFT 16
#define CODE_CACHE_HASH_TBL_LEN (1 << CODE_CACHE_HASH_TBL_SHIFT)
#define CODE_CACHE_HASH_TBL_MASK (CODE_CACHE_HASH_TBL_LEN - 1)
extern struct cache_entry* code_cache_tbl[CODE_CACHE_HASH_TBL_LEN];

#endif
