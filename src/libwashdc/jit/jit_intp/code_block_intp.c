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

#include <string.h>
#include <stdlib.h>

#include "washdc/error.h"
#include "dreamcast.h"
#include "jit/code_block.h"

#include "code_block_intp.h"

void code_block_intp_init(struct code_block_intp *block) {
    memset(block, 0, sizeof(*block));
}

void code_block_intp_cleanup(struct code_block_intp *block) {
    if (block->inst_list)
        free(block->inst_list);
    if (block->slots)
        free(block->slots);
}

void code_block_intp_compile(void *cpu,
                             struct code_block_intp *out,
                             struct il_code_block const *il_blk,
                             unsigned cycle_count) {
    /*
     * TODO: consider shallow-copying the il_blk into out, and "stealing" its
     * ownership of inst_list.  This is a little messy and would necessitate
     * removing the const attribute from il_blk, but the deep-copy I'm currently
     * doing is really suboptimal from a performance standpoint.
     */
    unsigned inst_count = il_blk->inst_count;
    size_t n_bytes = sizeof(struct jit_inst) * inst_count;
    out->inst_list = (struct jit_inst*)malloc(n_bytes);
    memcpy(out->inst_list, il_blk->inst_list, n_bytes);
    out->cycle_count = cycle_count;
    out->inst_count = inst_count;
    out->n_slots = il_blk->n_slots;
    out->slots = (uint32_t*)malloc(out->n_slots * sizeof(uint32_t));
}

reg32_t code_block_intp_exec(void *cpu, struct code_block_intp const *block) {
    unsigned inst_count = block->inst_count;
    struct jit_inst const* inst = block->inst_list;

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            inst->immed.fallback.fallback_fn(cpu, inst->immed.fallback.inst);
            inst++;
            break;
        case JIT_OP_JUMP:
            return block->slots[inst->immed.jump.slot_no];
        case JIT_JUMP_COND:
            /*
             * This ends the current block even if the jump was not executed.
             * Otherwise, there would have to be multiple exit points for each
             * block; this would not be impossible to implement but it would
             * mess with the cycle-counting since a given block would not
             * complete in the same number of cycles every time.
             */
            if ((block->slots[inst->immed.jump_cond.slot_no] & 1) ==
                inst->immed.jump_cond.t_flag) {
                return block->slots[inst->immed.jump_cond.jmp_addr_slot];
            }
            return block->slots[inst->immed.jump_cond.alt_jmp_addr_slot];
        case JIT_SET_SLOT:
            block->slots[inst->immed.set_slot.slot_idx] =
                inst->immed.set_slot.new_val;
            inst++;
            break;
        case JIT_OP_CALL_FUNC:
            inst->immed.call_func.func(cpu,
                                       block->slots[
                                           inst->immed.call_func.slot_no
                                           ]);
            inst++;
            break;
        case JIT_OP_READ_16_CONSTADDR:
            block->slots[inst->immed.read_16_constaddr.slot_no] =
                memory_map_read_16(inst->immed.read_16_constaddr.map,
                                   inst->immed.read_16_constaddr.addr);
            inst++;
            break;
        case JIT_OP_SIGN_EXTEND_16:
            block->slots[inst->immed.sign_extend_16.slot_no] =
                (int32_t)(int16_t)block->slots[inst->immed.sign_extend_16.slot_no];
            inst++;
            break;
        case JIT_OP_READ_32_CONSTADDR:
            block->slots[inst->immed.read_32_constaddr.slot_no] =
                memory_map_read_32(inst->immed.read_32_constaddr.map,
                                   inst->immed.read_32_constaddr.addr);
            inst++;
            break;
        case JIT_OP_READ_32_SLOT:
            block->slots[inst->immed.read_32_slot.dst_slot] =
                memory_map_read_32(inst->immed.read_32_slot.map,
                                   block->slots[
                                       inst->immed.read_32_slot.addr_slot
                                       ]);
            inst++;
            break;
        case JIT_OP_WRITE_32_SLOT:
            memory_map_write_32(inst->immed.write_32_slot.map,
                                block->slots[inst->immed.write_32_slot.addr_slot],
                                block->slots[inst->immed.write_32_slot.src_slot]);
            inst++;
            break;
        case JIT_OP_LOAD_SLOT16:
            block->slots[inst->immed.load_slot16.slot_no] =
                *inst->immed.load_slot16.src;
            inst++;
            break;
        case JIT_OP_LOAD_SLOT:
            block->slots[inst->immed.load_slot.slot_no] =
                *inst->immed.load_slot.src;
            inst++;
            break;
        case JIT_OP_STORE_SLOT:
            *inst->immed.store_slot.dst =
                block->slots[inst->immed.store_slot.slot_no];
            inst++;
            break;
        case JIT_OP_ADD:
            block->slots[inst->immed.add.slot_dst] +=
                block->slots[inst->immed.add.slot_src];
            inst++;
            break;
        case JIT_OP_SUB:
            block->slots[inst->immed.add.slot_dst] -=
                block->slots[inst->immed.add.slot_src];
            inst++;
            break;
        case JIT_OP_ADD_CONST32:
            block->slots[inst->immed.add_const32.slot_dst] +=
                inst->immed.add_const32.const32;
            inst++;
            break;
        case JIT_OP_XOR:
            block->slots[inst->immed.xor.slot_dst] ^=
                block->slots[inst->immed.xor.slot_src];
            inst++;
            break;
        case JIT_OP_XOR_CONST32:
            block->slots[inst->immed.xor_const32.slot_no] ^=
                inst->immed.xor_const32.const32;
            inst++;
            break;
        case JIT_OP_MOV:
            block->slots[inst->immed.mov.slot_dst] =
                block->slots[inst->immed.mov.slot_src];
            inst++;
            break;
        case JIT_OP_AND:
            block->slots[inst->immed.and.slot_dst] &=
                block->slots[inst->immed.and.slot_src];
            inst++;
            break;
        case JIT_OP_AND_CONST32:
            block->slots[inst->immed.and_const32.slot_no] &=
                inst->immed.and_const32.const32;
            inst++;
            break;
        case JIT_OP_OR:
            block->slots[inst->immed.or.slot_dst] |=
                block->slots[inst->immed.or.slot_src];
            inst++;
            break;
        case JIT_OP_OR_CONST32:
            block->slots[inst->immed.or_const32.slot_no] |=
                inst->immed.or_const32.const32;
            inst++;
            break;
        case JIT_OP_DISCARD_SLOT:
            // nothing to do here
            inst++;
            break;
        case JIT_OP_SLOT_TO_BOOL:
            block->slots[inst->immed.slot_to_bool.slot_no] =
                (block->slots[inst->immed.slot_to_bool.slot_no] ? 1 : 0);
            inst++;
            break;
        case JIT_OP_NOT:
            block->slots[inst->immed.not.slot_no] =
                ~block->slots[inst->immed.not.slot_no];
            inst++;
            break;
        case JIT_OP_SHLL:
            block->slots[inst->immed.shll.slot_no] <<=
                inst->immed.shll.shift_amt;
            inst++;
            break;
        case JIT_OP_SHAR:
            block->slots[inst->immed.shar.slot_no] =
                ((int32_t)block->slots[inst->immed.shar.slot_no]) >>
                inst->immed.shar.shift_amt;
            inst++;
            break;
        case JIT_OP_SHLR:
            block->slots[inst->immed.shlr.slot_no] >>=
                inst->immed.shlr.shift_amt;
            inst++;
            break;
        case JIT_OP_SET_GT_UNSIGNED:
            if (block->slots[inst->immed.set_gt_unsigned.slot_lhs] >
                block->slots[inst->immed.set_gt_unsigned.slot_rhs])
                block->slots[inst->immed.set_gt_unsigned.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_GT_SIGNED:
            if ((int32_t)block->slots[inst->immed.set_gt_signed.slot_lhs] >
                (int32_t)block->slots[inst->immed.set_gt_signed.slot_rhs])
                block->slots[inst->immed.set_gt_signed.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_GT_SIGNED_CONST:
            if ((int32_t)block->slots[inst->immed.set_gt_signed_const.slot_lhs] >
                inst->immed.set_gt_signed_const.imm_rhs)
                block->slots[inst->immed.set_gt_signed_const.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_EQ:
            if (block->slots[inst->immed.set_eq.slot_lhs] ==
                block->slots[inst->immed.set_eq.slot_rhs])
                block->slots[inst->immed.set_eq.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_GE_UNSIGNED:
            if (block->slots[inst->immed.set_ge_unsigned.slot_lhs] >=
                block->slots[inst->immed.set_ge_unsigned.slot_rhs])
                block->slots[inst->immed.set_ge_unsigned.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_GE_SIGNED:
            if ((int32_t)block->slots[inst->immed.set_ge_signed.slot_lhs] >=
                (int32_t)block->slots[inst->immed.set_ge_signed.slot_rhs])
                block->slots[inst->immed.set_ge_unsigned.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_SET_GE_SIGNED_CONST:
            if ((int32_t)block->slots[inst->immed.set_ge_signed_const.slot_lhs] >=
                inst->immed.set_ge_signed_const.imm_rhs)
                block->slots[inst->immed.set_ge_signed_const.slot_dst] |= 1;
            inst++;
            break;
        case JIT_OP_MUL_U32:
            block->slots[inst->immed.mul_u32.slot_dst] =
                block->slots[inst->immed.mul_u32.slot_lhs] *
                block->slots[inst->immed.mul_u32.slot_rhs];
            inst++;
            break;
        case JIT_OP_SHAD:
            if ((int32_t)block->slots[inst->immed.shad.slot_shift_amt] >= 0) {
                block->slots[inst->immed.shad.slot_val] <<=
                    block->slots[inst->immed.shad.slot_shift_amt];
            } else {
                block->slots[inst->immed.shad.slot_val] =
                    ((int32_t)block->slots[inst->immed.shad.slot_val]) >>
                    -(int32_t)block->slots[inst->immed.shad.slot_shift_amt];
            }
            inst++;
            break;
        }
    }

    // all blocks should end by jumping out
    LOG_ERROR("ERROR: %u-len block does not jump out\n", block->inst_count);
    RAISE_ERROR(ERROR_INTEGRITY);
}
