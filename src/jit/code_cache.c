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

// TODO: don't use a linear cache, use a tree of some sort.
unsigned cache_len;
struct cache_entry *cache;
bool nuke;

void code_cache_invalidate_all(void) {
    /*
     * this function gets called whenever something writes to the sh4 CCR.
     * Since we don't want to trash the block currently executing, we instead
     * set a flag to be set next time code_cache_find is called.
     */
    nuke = true;
    LOG_DBG("%s called - nuking cache\n", __func__);
}

struct cache_entry *code_cache_find(addr32_t addr) {
    if (nuke) {
        nuke = false;
        free(cache);
        cache = NULL;
        cache_len = 0;
    }

    unsigned idx;
    for (idx = 0; idx < cache_len; idx++)
        if (cache[idx].addr == addr) {
            return cache + idx;
        }

    unsigned new_cache_len = cache_len + 1;
    struct cache_entry *new_cache =
        realloc(cache, sizeof(struct cache_entry) * new_cache_len);

    if (!new_cache)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    cache = new_cache;
    cache_len = new_cache_len;

    struct cache_entry *new_entry = cache + (cache_len - 1);
    memset(new_entry, 0, sizeof(struct cache_entry));
    new_entry->addr = addr;

    jit_code_block_init(&new_entry->blk);

    return new_entry;
}
