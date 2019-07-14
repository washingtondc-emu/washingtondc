/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#include <string.h>

#include "code_block.h"

static void jit_optimize_nop(struct il_code_block *blk);
static void strike_inst(struct il_code_block *blk, unsigned slot_no);

void jit_optimize(struct il_code_block *blk) {
    jit_optimize_nop(blk);
}

static void strike_inst(struct il_code_block *blk, unsigned inst_idx) {
    unsigned insts_after = blk->inst_count - 1 - inst_idx;
    if (insts_after) {
        memmove(blk->inst_list + inst_idx, blk->inst_list + inst_idx + 1,
                sizeof(struct jit_inst) * insts_after);
    }
    --blk->inst_count;
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
            strike_inst(blk, inst_no);
            continue;
        }
        inst_no++;
    }
}
