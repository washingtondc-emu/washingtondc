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

#include "code_cache.h"

// uncomment for basic performance stats
// #define PERF_STATS

static int node_height(struct cache_entry *node);
static int node_balance(struct cache_entry *node);

static void clear_cache(struct cache_entry *node);

static struct cache_entry *root;
static bool nuke;

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
    LOG_INFO("JIT: max depth was %u\n", max_depth);
    LOG_INFO("JIT: max cache size was %u\n", cache_sz);
    LOG_INFO("JIT: height of root at shutdown is %d\n", node_height(root));
    LOG_INFO("JIT: balance of root at shutdown is %d\n", node_balance(root));
    LOG_INFO("================================\n");
#endif
}

void code_cache_init(void) {
    nuke = false;
    root = NULL;
    perf_stats_reset();
}

void code_cache_cleanup(void) {
    perf_stats_print();

    clear_cache(root);
}

void code_cache_invalidate_all(void) {
    /*
     * this function gets called whenever something writes to the sh4 CCR.
     * Since we don't want to trash the block currently executing, we instead
     * set a flag to be set next time code_cache_find is called.
     */
    nuke = true;
    LOG_DBG("%s called - nuking cache\n", __func__);
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
        jit_code_block_cleanup(&node->blk);
        free(node);
    }
}

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
    jit_code_block_init(&new_node->blk);

    n_entries++;
    if (n_entries >= MAX_ENTRIES)
        RAISE_ERROR(ERROR_INTEGRITY);

    /*
     * now retrace back up to the root using a simple AVL-like rebalancing
     * algorithm to ensure that the heights of each node's subtrees differ by no
     * more than 1.
     */
    struct cache_entry *cur_node = new_node;
    while (cur_node != root) {
        struct cache_entry *parent = cur_node->parent;
        int cur_node_bal = node_balance(cur_node);
        int par_node_bal = node_balance(parent);

        if (par_node_bal == -2) {
            if (cur_node_bal < 0) {
                // parent leans to the left and cur_node leans to the left
                rot_right(parent);
            } else {
                // parent leans to the right and cur_node leans to the right
                rot_left(cur_node);
                rot_right(parent);
            }
        } else if (par_node_bal == 2) {
            if (cur_node_bal > 0) {
                // parent leans to the right and cur_node leans to the right
                rot_left(parent);
            } else {
                // parent leans to the right and cur_node leans to the left
                rot_right(cur_node);
                rot_left(parent);
            }
        } else if (par_node_bal == 0) {
            /*
             * do an early exit here.  this is safe because the presence of a
             * balanced node means that the most recent leaf did not increase
             * the height
             */
            goto the_end;
        } else {
            /*
             * move up the tree, there are no rotations needed for this node
             * because it only leans slightly.  It's parent might still need to
             * be rebalanced since the height of this subtree has increased.
             */
            cur_node = parent;
        }
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
    if (nuke) {
        nuke = false;
        clear_cache(root);
        root = NULL;
    }

    if (root)
        return do_code_cache_find(root, addr);

    basic_insert(&root, NULL, addr);
    return root;
}
