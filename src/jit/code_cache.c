/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include "error.h"
#include "hw/sh4/types.h"
#include "code_block.h"
#include "log.h"
#include "config.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/exec_mem.h"
#endif

#include "code_cache.h"

/*
 * This is a two-level cache.  The lower level is a binary search tree balanced
 * using the AVL algorithm.  The upper level is a hash-table.  Everything that
 * exists in the hash also exists in the tree, but not everything in the tree
 * exists in the hash.  When there is a collision in the hash, we discard
 * outdated values instead of trying to implement probing or chaining.
 */

#if defined(INVARIANTS) || defined(PERF_STATS)
static int node_height(struct cache_entry *node);
static int node_balance(struct cache_entry *node);
#endif

static void clear_cache(struct cache_entry *node);

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
    struct cache_entry *root;
    struct oldroot_node *next;
};
static struct oldroot_node *oldroot;

static struct cache_entry *root;

#define HASH_TBL_LEN 0x10000

static struct cache_entry* tbl[HASH_TBL_LEN];
static unsigned hashfn(addr32_t addr);

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

#ifdef PERF_STATS
static unsigned depth, max_depth;
static unsigned cache_sz;
static unsigned max_cache_sz;

/*
 * track the top ten most "popular" code blocks.
 * this array must always be sorted based on n_access in descending order
 *
 * One caveat to keep in mind is that this system only uses the guest-address
 * to uniquely identify code blocks.  If a code block is overwritten by another
 * code block that just happens to start at the exact same address, then
 * perf_stats_update_max_access will mistakenly assume that they are both the
 * same code_block.  I do not expect that this corner case will happen often
 * enough to matter, especially since PERF_STATS is only meant to be used on
 * development builds.
 */
#define MAX_ACCESS_LEN 10
static struct access_ent {
    unsigned n_access;
    addr32_t addr;
} max_access[MAX_ACCESS_LEN];

static void perf_stats_update_max_access(struct cache_entry *ent) {
    unsigned n_access = ent->n_access;
    addr32_t access_addr = ent->addr;
    int idx, idx_inner;

    // make sure this address isn't already in the list
    for (idx = 0; idx < MAX_ACCESS_LEN; idx++)
        if (max_access[idx].addr == access_addr) {
            max_access[idx].n_access = n_access;
            goto sort;
        }

    /*
     * We only need to check the end of the list because that's the element
     * that will get kicked out.
     */
    if (max_access[MAX_ACCESS_LEN - 1].n_access < n_access) {
        max_access[MAX_ACCESS_LEN - 1].n_access = n_access;
        max_access[MAX_ACCESS_LEN - 1].addr = access_addr;
    } else {
        return;
    }

sort:
    for (idx = 0; idx < MAX_ACCESS_LEN - 1; idx++)
        for (idx_inner = idx + 1; idx_inner < MAX_ACCESS_LEN; idx_inner++) {
            if (max_access[idx_inner].n_access > max_access[idx].n_access) {
                struct access_ent tmp = max_access[idx_inner];
                max_access[idx_inner] = max_access[idx];
                max_access[idx] = tmp;
            }
        }
}

// how many times something got kicked out of the second-level cache
static unsigned n_tbl_evictions;

/*
 * number of times we had to look in the tree for something because it wasn't
 * in the second-level cache.
 */
static unsigned n_tree_searches;

// total number of cache-reads
static unsigned total_access_count;
#endif

#ifdef ENABLE_JIT_X86_64
static bool native_mode = true;
#endif

static void perf_stats_reset(void) {
#ifdef PERF_STATS
    cache_sz = 0;
    max_cache_sz = 0;
    max_depth = 0;
#endif
}

static void perf_stats_add_node(void) {
#ifdef PERF_STATS
    cache_sz++;
    if (cache_sz > max_cache_sz)
        max_cache_sz = cache_sz;
#endif
}

static void perf_stats_reset_depth_count(void) {
#ifdef PERF_STATS
    depth = 0;
#endif
}

static void perf_stats_inc_depth_count(void) {
#ifdef PERF_STATS
    depth++;

    if (depth > max_depth)
        max_depth = depth;
#endif
}

static void perf_stats_print(void) {
#ifdef PERF_STATS
    LOG_INFO("==== Code Cache perf stats ====\n");
    LOG_INFO("JIT: %u total accesses\n", total_access_count);
    LOG_INFO("JIT: %u total tree searches\n", n_tree_searches);
    LOG_INFO("JIT: %u table evictions\n", n_tbl_evictions);
    LOG_INFO("JIT: max depth was %u\n", max_depth);
    LOG_INFO("JIT: max cache size was %u\n", cache_sz);
    LOG_INFO("JIT: height of root at shutdown is %d\n", node_height(root));
    LOG_INFO("JIT: balance of root at shutdown is %d\n", node_balance(root));
    LOG_INFO("JIT: The top %u most popular code blocks were accessed:\n",
             MAX_ACCESS_LEN);
    unsigned idx;
    for (idx = 0; idx < MAX_ACCESS_LEN; idx++)
        LOG_INFO("JIT: \t0x%08x - %u times\n",
                 max_access[idx].addr, max_access[idx].n_access);
    LOG_INFO("================================\n");
#endif
}

void code_cache_init(void) {
    root = NULL;
    perf_stats_reset();

#ifdef ENABLE_JIT_X86_64
    native_mode = config_get_native_jit();
#endif
}

void code_cache_cleanup(void) {
    perf_stats_print();

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
    list_node->root = root;
    oldroot = list_node;

    root = NULL;
    memset(tbl, 0, sizeof(tbl));
}

void code_cache_gc(void) {
    while (oldroot) {
        struct oldroot_node *next = oldroot->next;
        clear_cache(oldroot->root);
        free(oldroot);
        oldroot = next;
    }

#ifdef INVARIANTS
    exec_mem_check_integrity();
#endif
}

static void clear_cache(struct cache_entry *node) {
    n_entries = 0;
#ifdef PERF_STATS
    cache_sz = 0;
#endif

    if (node) {
        if (node->left)
            clear_cache(node->left);
        if (node->right)
            clear_cache(node->right);
#ifdef ENABLE_JIT_X86_64
        if (native_mode)
            code_block_x86_64_cleanup(&node->blk.x86_64);
        else
#endif
            code_block_intp_cleanup(&node->blk.intp);
        free(node);
    }
}

#if defined(INVARIANTS) || defined(PERF_STATS)
static int node_height(struct cache_entry *node) {
    int max_height = 0;
    if (node->left) {
        int left_height = node_height(node->left) + 1;
        if (left_height > max_height)
            max_height = left_height;
    }
    if (node->right) {
        int right_height = node_height(node->right) + 1;
        if (right_height > max_height)
            max_height = right_height;
    }
    return max_height;
}

static int node_balance(struct cache_entry *node) {
    int left_height = 0, right_height = 0;

    if (node->right)
        right_height = 1 + node_height(node->right);
    if (node->left)
        left_height = 1 + node_height(node->left);

    int bal = right_height - left_height;

    return bal;
}
#endif

#ifdef INVARIANTS
static void cache_invariant(struct cache_entry *node) {
    int bal = node_balance(node);
    if (abs(bal) > 1) {
        LOG_ERROR("node balance is %d\n", bal);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (node->left)
        cache_invariant(node->left);
    if (node->right)
        cache_invariant(node->right);
}
#endif

/*
 * rotate the subtree right-wards so that the left child is now the root-node.
 * The original root-node will become the right node.
 *
 * The onus is on the caller to make sure the left child exists before calling
 * this function.
 *
 * This function DOES NOT update the balance factors; it is entirely on the
 * caller to do that.
 */
static void rot_right(struct cache_entry *old_root) {
    struct cache_entry *parent = old_root->parent;
    struct cache_entry *new_root = old_root->left;
    struct cache_entry *new_left_subtree = new_root->right;

    if (old_root != root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);

    // update the parent's view of this subtree
    if (parent) {
        if (parent->left == old_root)
            parent->left = new_root;
        else
            parent->right = new_root;
    }

    new_root->parent = parent;
    old_root->parent = new_root;
    if (new_left_subtree)
        new_left_subtree->parent = old_root;

    old_root->left = new_left_subtree;
    new_root->right = old_root;

    if (root == old_root)
        root = new_root;
}

/*
 * rotate the subtree left-wards so that the right child is now the root-node.
 * The original root-node will become the left node.
 *
 * The onus is on the caller to make sure the right child exists before calling
 * this function.
 *
 * This function DOES NOT update the balance factors; it is entirely on the
 * caller to do that.
 */
static void rot_left(struct cache_entry *old_root) {
    struct cache_entry *parent = old_root->parent;
    struct cache_entry *new_root = old_root->right;
    struct cache_entry *new_right_subtree = new_root->left;

    if (old_root != root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);

    // update the parent's view of this subtree
    if (parent) {
        if (parent->left == old_root)
            parent->left = new_root;
        else
            parent->right = new_root;
    }

    new_root->parent = parent;
    old_root->parent = new_root;
    if (new_right_subtree)
        new_right_subtree->parent = old_root;

    old_root->right = new_right_subtree;
    new_root->left = old_root;

    if (root == old_root)
        root = new_root;
}

static struct cache_entry *
basic_insert(struct cache_entry **node_p, struct cache_entry *parent,
             addr32_t addr) {
    struct cache_entry *new_node =
        (struct cache_entry*)calloc(1, sizeof(struct cache_entry));
    if (!new_node)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    *node_p = new_node;
    if (node_p != &root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);
    new_node->parent = parent;
    new_node->addr = addr;

#ifdef ENABLE_JIT_X86_64
    if (native_mode)
        code_block_x86_64_init(&new_node->blk.x86_64);
    else
#endif
        code_block_intp_init(&new_node->blk.intp);

    n_entries++;
    if (n_entries >= MAX_ENTRIES)
        RAISE_ERROR(ERROR_INTEGRITY);

    /*
     * now retrace back up to the root using a the AVL rebalancing algorithm
     * to ensure that the heights of each node's subtrees differ by no more
     * than 1.
     */
    struct cache_entry *cur_node = new_node;
    while (cur_node != root) {
        struct cache_entry *parent = cur_node->parent;
        if (cur_node == parent->left) {
            switch (parent->bal) {
            case 1:
                // parent-node height is unchanged
                parent->bal = 0;
                goto the_end;
            case 0:
                /*
                 * the parent-node does not need to be rebalanced, but its
                 * height has changed.
                 */
                parent->bal = -1;
                break;
            case -1:
                /*
                 * the parent-node is completely imbalanced and needs to be
                 * rotated.
                 */
                if (cur_node->bal <= 0) {
                    rot_right(parent);
                    parent->bal = 0;
                    cur_node->bal = 0;
                } else {
                    int child_bal = cur_node->right->bal;
                    rot_left(cur_node);
                    rot_right(parent);
                    if (child_bal < 0) {
                        cur_node->bal = 0;
                        parent->bal = 1;
                    } else if (child_bal > 0) {
                        cur_node->bal = -1;
                        parent->bal = 0;
                    } else {
                        cur_node->bal = 0;
                        parent->bal = 0;
                    }
                    cur_node->parent->bal = 0;
                }
                goto the_end;
            default:
                // should be impossible
                RAISE_ERROR(ERROR_INTEGRITY);
            }
        } else {
            switch (parent->bal) {
            case -1:
                // parent-node height is unchanged
                parent->bal = 0;
                goto the_end;
            case 0:
                /*
                 * the parent-node does not need to be rebalanced, but its
                 * height has changed.
                 */
                parent->bal = 1;
                break;
            case 1:
                /*
                 * the parent-node is completely imbalanced and needs to be
                 * rotated.
                 */
                if (cur_node->bal >= 0) {
                    rot_left(parent);
                    parent->bal = 0;
                    cur_node->bal = 0;
                } else {
                    int child_bal = cur_node->left->bal;
                    rot_right(cur_node);
                    rot_left(parent);
                    if (child_bal < 0) {
                        parent->bal = 0;
                        cur_node->bal = 1;
                    } else if (child_bal > 0) {
                        cur_node->bal = 0;
                        parent->bal = -1;
                    } else {
                        cur_node->bal = 0;
                        parent->bal = 0;
                    }
                    cur_node->parent->bal = 0;
                }
                goto the_end;
            default:
                // should be impossible
                RAISE_ERROR(ERROR_INTEGRITY);
            }
        }
        cur_node = parent;
    }

 the_end:
#ifdef INVARIANTS
    cache_invariant(root);
#endif

    perf_stats_add_node();

    return new_node;
}

/*
 * Do a simple search down the tree for the given jump-address.  If no node is
 * found, an invalid one will be created and returned because any time the code
 * cache can't find a cache-entry, it will immediately want to create a new one.
 */
static struct cache_entry *do_code_cache_find(struct cache_entry *node,
                                              addr32_t addr) {
    perf_stats_reset_depth_count();

    for (;;) {
        if (addr < node->addr) {
            if (node->left) {
                perf_stats_inc_depth_count();
                node = node->left;
                continue;
            }
            return basic_insert(&node->left, node, addr);
        }

        if (addr > node->addr) {
            if (node->right) {
                perf_stats_inc_depth_count();
                node = node->right;
                continue;
            }
            return basic_insert(&node->right, node, addr);
        }

        return node;
    }
}

struct cache_entry *code_cache_find(addr32_t addr) {
#ifdef PERF_STATS
    total_access_count++;
#endif

    unsigned hash_idx = hashfn(addr) % HASH_TBL_LEN;
    struct cache_entry *maybe = tbl[hash_idx];
    if (maybe && maybe->addr == addr)
        return maybe;

    if (root) {
        struct cache_entry *node = do_code_cache_find(root, addr);

#ifdef PERF_STATS
        n_tree_searches++;
        if (tbl[hash_idx])
            n_tbl_evictions++;

        node->n_access++;
        perf_stats_update_max_access(node);
#endif

        tbl[hash_idx] = node;
        return node;
    }

    basic_insert(&root, NULL, addr);
    tbl[hash_idx] = root;

#ifdef PERF_STATS
    root->n_access = 1;
    perf_stats_update_max_access(root);
#endif

    return root;
}

static unsigned hashfn(addr32_t addr) {
    return addr;
}