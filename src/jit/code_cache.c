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

#if defined(INVARIANTS)
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

void code_cache_init(void) {
    root = NULL;

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
    list_node->root = root;
    oldroot = list_node;

    root = NULL;
    memset(code_cache_tbl, 0, sizeof(code_cache_tbl));
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

#if defined(INVARIANTS)
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

    return new_node;
}

/*
 * Do a simple search down the tree for the given jump-address.  If no node is
 * found, an invalid one will be created and returned because any time the code
 * cache can't find a cache-entry, it will immediately want to create a new one.
 */
static struct cache_entry *do_code_cache_find(struct cache_entry *node,
                                              addr32_t addr) {
    for (;;) {
        if (addr < node->addr) {
            if (node->left) {
                node = node->left;
                continue;
            }
            return basic_insert(&node->left, node, addr);
        }

        if (addr > node->addr) {
            if (node->right) {
                node = node->right;
                continue;
            }
            return basic_insert(&node->right, node, addr);
        }

        return node;
    }
}

struct cache_entry *code_cache_find(addr32_t addr) {
    unsigned hash_idx = addr & CODE_CACHE_HASH_TBL_MASK;
    struct cache_entry *maybe = code_cache_tbl[hash_idx];
    if (maybe && maybe->addr == addr)
        return maybe;

    struct cache_entry *ret = code_cache_find_slow(addr);
    code_cache_tbl[hash_idx] = ret;
    return ret;
}

struct cache_entry *code_cache_find_slow(addr32_t addr) {
    if (root) {
        struct cache_entry *node = do_code_cache_find(root, addr);
        return node;
    } else {
        basic_insert(&root, NULL, addr);
        return root;
    }
}
