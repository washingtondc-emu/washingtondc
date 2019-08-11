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

#include "code_block.h"
#include "dreamcast.h"

#define DEFAULT_BLOCK_LEN 32
#define BLOCK_GROW_LEN 1

void il_code_block_init(struct il_code_block *block) {
    memset(block, 0, sizeof(*block));
    block->inst_count = 0;
    block->inst_alloc = DEFAULT_BLOCK_LEN;
    block->inst_list = (struct jit_inst*)malloc(DEFAULT_BLOCK_LEN *
                                                sizeof(struct jit_inst));
}

void il_code_block_cleanup(struct il_code_block *block) {
    free(block->inst_list);
    memset(block, 0, sizeof(*block));
}

void il_code_block_push_inst(struct il_code_block *block,
                              struct jit_inst const *inst) {
    if (block->inst_count >= block->inst_alloc) {
        unsigned new_alloc = block->inst_alloc + BLOCK_GROW_LEN;
        struct jit_inst *new_list =
            (struct jit_inst*)realloc(block->inst_list,
                                      new_alloc * sizeof(struct jit_inst));
        if (!new_list)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        block->inst_list = new_list;
        block->inst_alloc = new_alloc;
    }

    block->inst_list[block->inst_count++] = *inst;
}

void il_code_block_strike_inst(struct il_code_block *blk, unsigned inst_idx) {
    unsigned insts_after = blk->inst_count - 1 - inst_idx;
    if (insts_after) {
        memmove(blk->inst_list + inst_idx, blk->inst_list + inst_idx + 1,
                sizeof(struct jit_inst) * insts_after);
    }
    --blk->inst_count;
}

void il_code_block_insert_inst(struct il_code_block *blk,
                               struct jit_inst const *inst, unsigned idx) {
    if (idx == blk->inst_count) {
        il_code_block_push_inst(blk, inst);
        return;
    }

    if (blk->inst_count >= blk->inst_alloc) {
        unsigned new_alloc = blk->inst_alloc + BLOCK_GROW_LEN;
        struct jit_inst *new_list =
            (struct jit_inst*)realloc(blk->inst_list,
                                      new_alloc * sizeof(struct jit_inst));
        if (!new_list)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        blk->inst_list = new_list;
        blk->inst_alloc = new_alloc;
    }

    unsigned n_insts_after = blk->inst_count - idx;
    if (n_insts_after)
        memmove(blk->inst_list + idx + 1, blk->inst_list + idx,
                n_insts_after * sizeof(struct jit_inst));
    blk->inst_list[idx] = *inst;
    blk->inst_count++;
}

static void il_code_block_add_slot(struct il_code_block *block) {
    block->n_slots++;
    struct il_slot *new_slot = block->slots + (block->n_slots - 1);
    memset(new_slot, 0, sizeof(*new_slot));
}

unsigned alloc_slot(struct il_code_block *block) {
    if (block->n_slots >= MAX_SLOTS)
        RAISE_ERROR(ERROR_OVERFLOW);

    unsigned slot_no = block->n_slots++;
    il_code_block_add_slot(block);

    return slot_no;
}

void free_slot(struct il_code_block *block, unsigned slot_no) {
}
