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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "washdc/error.h"
#include "code_block.h"
#include "log.h"
#include "config.h"
#include "avl.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/exec_mem.h"
#endif

#include "code_cache.h"

#define CODE_CACHE_HASH_TBL_SHIFT 16
#define CODE_CACHE_HASH_TBL_LEN (1 << CODE_CACHE_HASH_TBL_SHIFT)
#define CODE_CACHE_HASH_TBL_MASK (CODE_CACHE_HASH_TBL_LEN - 1)

/*
 * This is a two-level cache.  The lower level is a binary search tree balanced
 * using the AVL algorithm.  The upper level is a hash-table.  Everything that
 * exists in the hash also exists in the tree, but not everything in the tree
 * exists in the hash.  When there is a collision in the hash, we discard
 * outdated values instead of trying to implement probing or chaining.
 */

/*
 * oldroot points to a list of trees invalid nodes.
 *
 * When code_cache_invalidate_all gets called from within CPU context
 * (typically due to a write to the SH4 CCR), all nodes need to be deleted.
 * This is not possible to due within CPU context because that would delete the
 * node which is currently executed.  As a workaround, the entire tree is
 * relocated to the oldroot pointer so that its nodes can be freed later when
 * the emulator exits CPU context.
 */
struct oldroot_node {
    struct avl_tree tree;
    struct oldroot_node *next;
};
static struct oldroot_node *oldroot;

static struct avl_tree tree;

struct cache_entry* code_cache_tbl[CODE_CACHE_HASH_TBL_LEN];

/*
 * the maximum number of code-cache entries that can be created before the
 * cache assumes something is wrong.  This is completely arbitrary, and it may
 * need to be raised, lowered or removed entirely in the future.
 *
 * The reason it is here is that my laptop doesn't have much memory, and when
 * the cache gets too big then my latop will thrash and become unresponsive.
 *
 * Under normal operation, I don't think the cache should get this big.  This
 * typically only happens when there's a bug in the cache that causes it to
 * keep making more and more cache entries because it is unable to find the
 * ones it has already created.  Dreamcast only has 16MB of memory, so it's
 * very unlikely (albeit not impossible) that this cache would hit 16-million
 * different jump-in points without getting reset via a write to the SH4's CCR
 * register.
 */
#define MAX_ENTRIES (1024*1024)
static unsigned n_entries;

#ifdef ENABLE_JIT_X86_64
static bool native_mode = true;
#endif

static struct avl_node*
cache_entry_ctor(void) {
    struct cache_entry *ent = calloc(1, sizeof(struct cache_entry));

#ifdef ENABLE_JIT_X86_64
    if (native_mode)
        code_block_x86_64_init(&ent->blk.x86_64);
    else
#endif
        code_block_intp_init(&ent->blk.intp);

    n_entries++;
    if (n_entries >= MAX_ENTRIES)
        RAISE_ERROR(ERROR_INTEGRITY);
    return &ent->node;
}

static void
cache_entry_dtor(struct avl_node *node) {
    struct cache_entry *ent = &AVL_DEREF(node, struct cache_entry, node);
#ifdef ENABLE_JIT_X86_64
    if (native_mode)
        code_block_x86_64_cleanup(&ent->blk.x86_64);
    else
#endif
        code_block_intp_cleanup(&ent->blk.intp);
    free(ent);
}

static void reinit_tree(void) {
    avl_init(&tree, cache_entry_ctor, cache_entry_dtor);
}

void code_cache_init(void) {
    reinit_tree();

#ifdef ENABLE_JIT_X86_64
    native_mode = config_get_native_jit();
#endif
}

void code_cache_cleanup(void) {
    code_cache_invalidate_all();
    code_cache_gc();
}

void code_cache_invalidate_all(void) {
    /*
     * this function gets called whenever something writes to the sh4 CCR.
     * Since we don't want to trash the block currently executing, we instead
     * set a flag to be set next time code_cache_find is called.
     */
    LOG_DBG("%s called - nuking cache\n", __func__);

    /*
     * Throw root onto the oldroot list to be cleared later.  It's not safe to
     * clear out oldroot now because the current code block might be part of it.
     * Also keep in mind that the current code block might be part of a
     * pre-existing oldroot if this function got called more than once by the
     * current code block.
     */
    struct oldroot_node *list_node =
        (struct oldroot_node*)malloc(sizeof(struct oldroot_node));
    if (!list_node)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    list_node->next = oldroot;
    list_node->tree = tree;
    oldroot = list_node;

    reinit_tree();
    memset(code_cache_tbl, 0, sizeof(code_cache_tbl));

    n_entries = 0;
}

void code_cache_gc(void) {
    while (oldroot) {
        struct oldroot_node *next = oldroot->next;
        avl_cleanup(&oldroot->tree);
        free(oldroot);
        oldroot = next;
    }

#ifdef INVARIANTS
    exec_mem_check_integrity();
#endif
}

struct cache_entry *code_cache_find(addr32_t addr) {
    unsigned hash_idx = addr & CODE_CACHE_HASH_TBL_MASK;
    struct cache_entry *maybe = code_cache_tbl[hash_idx];
    if (maybe && maybe->node.key == addr)
        return maybe;

    struct cache_entry *ret = code_cache_find_slow(addr);
    code_cache_tbl[hash_idx] = ret;
    return ret;
}

struct cache_entry *code_cache_find_slow(addr32_t addr) {
    struct avl_node *node = avl_find(&tree, addr);
    return &AVL_DEREF(node, struct cache_entry, node);
}
