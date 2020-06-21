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

static void il_code_block_add_slot(struct il_code_block *block,
                                   enum washdc_jit_slot_tp tp) {
    block->n_slots++;
    struct il_slot *new_slot = block->slots + (block->n_slots - 1);
    new_slot->tp = tp;
}

unsigned alloc_slot(struct il_code_block *block, enum washdc_jit_slot_tp tp) {
    if (block->n_slots >= MAX_SLOTS)
        RAISE_ERROR(ERROR_OVERFLOW);

    il_code_block_add_slot(block, tp);

    return block->n_slots - 1;
}

void free_slot(struct il_code_block *block, unsigned slot_no) {
}

unsigned
jit_code_block_slot_lifespan(struct il_code_block const *blk,
                             unsigned slot_no, unsigned base) {
    unsigned last_ref = base;
    unsigned idx = base;
    while (idx < blk->inst_count) {
        struct jit_inst const *inst = blk->inst_list + idx;
        if (inst->op == JIT_OP_DISCARD_SLOT &&
            inst->immed.discard_slot.slot_no == slot_no)
            break;
        if (jit_inst_is_read_slot(inst, slot_no) ||
            jit_inst_is_write_slot(inst, slot_no))
            last_ref = idx;
        idx++;
    }
    return last_ref;
}
