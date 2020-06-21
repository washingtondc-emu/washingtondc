/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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

#include <string.h>
#include <stdbool.h>

#include "code_block.h"

static void jit_optimize_nop(struct il_code_block *blk);
static void jit_optimize_dead_write(struct il_code_block *blk);
static void jit_optimize_discard(struct il_code_block *blk);

static bool
check_for_reads_after(struct il_code_block *blk, unsigned inst_idx);

void jit_optimize(struct il_code_block *blk) {
    jit_optimize_nop(blk);
    jit_optimize_dead_write(blk);
    jit_optimize_discard(blk);
}

// remove IL instructions which don't actually do anything.
static void jit_optimize_nop(struct il_code_block *blk) {
    unsigned inst_no = 0;
    while (inst_no < blk->inst_count) {
        struct jit_inst *inst = blk->inst_list + inst_no;
        if (inst->op == JIT_OP_AND &&
            inst->immed.and.slot_src == inst->immed.and.slot_dst) {
            /*
             * ANDing a slot with itself.
             *
             * this tends to happen due to the way that the SH4 TST instruction
             * is implemented.  Programs will AND a register to itself to set
             * the C flag, and that causes a spurious IL instruction when the
             * same register is tested against itself because the AND operation
             * instruction in the IL is separate from the SLOT_TO_BOOL
             * operation.
             */
            il_code_block_strike_inst(blk, inst_no);
            continue;
        }
        inst_no++;
    }
}

// remove IL instructions which write to a slot which is not later read from
static void jit_optimize_dead_write(struct il_code_block *blk) {
    unsigned src_inst = 0;
    while (src_inst < blk->inst_count) {
        struct jit_inst *inst = blk->inst_list + src_inst;
        int write_slots[JIT_IL_MAX_WRITE_SLOTS];
        jit_inst_get_write_slots(inst, write_slots);

        // skip this instruction if it doesn't write to any slots
        unsigned write_count = 0;
        unsigned slot_no;
        for (slot_no = 0; slot_no < JIT_IL_MAX_WRITE_SLOTS; slot_no++)
            if (write_slots[slot_no] != -1)
                write_count++;
        if (!write_count) {
            src_inst++;
            continue;
        }

        if (!check_for_reads_after(blk, src_inst))
            il_code_block_strike_inst(blk, src_inst);
        else
            src_inst++;
    }
}

static bool
check_for_reads_after(struct il_code_block *blk, unsigned inst_idx) {
    struct jit_inst *inst = blk->inst_list + inst_idx;
    int write_slots[JIT_IL_MAX_WRITE_SLOTS];
    jit_inst_get_write_slots(inst, write_slots);

    unsigned check_idx;
    for (check_idx = inst_idx + 1; check_idx < blk->inst_count; check_idx++) {
        struct jit_inst *check_inst = blk->inst_list + check_idx;
        int slot_no;
        for (slot_no = 0; slot_no < JIT_IL_MAX_WRITE_SLOTS; slot_no++) {
            if (write_slots[slot_no] != -1) {
                if (jit_inst_is_read_slot(check_inst, write_slots[slot_no]))
                    return true;
                else if (jit_inst_is_write_slot(check_inst, write_slots[slot_no]))
                    write_slots[slot_no] = -1;
            }
        }
    }
    return false;
}

static void jit_optimize_discard(struct il_code_block *blk) {
    for (unsigned slot_no = 0; slot_no < blk->n_slots; slot_no++) {
        int inst_no;
        for (inst_no = blk->inst_count - 1; inst_no >= 0; inst_no--) {
            struct jit_inst const *inst = blk->inst_list + inst_no;
            if (jit_inst_is_read_slot(inst, slot_no) ||
                jit_inst_is_write_slot(inst, slot_no)) {
                struct jit_inst op;
                op.op = JIT_OP_DISCARD_SLOT;
                op.immed.discard_slot.slot_no = slot_no;
                il_code_block_insert_inst(blk, &op, inst_no + 1);
                break;
            }
        }
    }
}
