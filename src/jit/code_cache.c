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
    LOG_INFO("JIT: max depth ws %u\n", max_depth);
    LOG_INFO("JIT: max cache size was %u\n", cache_sz);
    LOG_INFO("================================\n");
#endif
}

static void clear_cache(struct cache_entry *node);

// TODO: don't use a linear cache, use a tree of some sort.
static struct cache_entry *root;
static bool nuke;

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
            node->left =
                (struct cache_entry*)calloc(1, sizeof(struct cache_entry));
            if (!node->left)
                RAISE_ERROR(ERROR_FAILED_ALLOC);
            node->left->addr = addr;
            jit_code_block_init(&node->left->blk);

            perf_stats_add_node();

            return node->left;
        }

        if (addr > node->addr) {
            if (node->right) {
                perf_stats_inc_depth_count();
                node = node->right;
                continue;
            }
            node->right = (struct cache_entry*)calloc(1, sizeof(struct cache_entry));
            if (!node->right)
                RAISE_ERROR(ERROR_FAILED_ALLOC);
            node->right->addr = addr;
            jit_code_block_init(&node->right->blk);

            perf_stats_add_node();

            return node->right;
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

    perf_stats_add_node();

    root = (struct cache_entry*)calloc(1, sizeof(struct cache_entry));
    root->addr = addr;
    jit_code_block_init(&root->blk);
    return root;
}
