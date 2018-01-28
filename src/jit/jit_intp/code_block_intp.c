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

#include <string.h>
#include <stdlib.h>

#include "error.h"
#include "dreamcast.h"
#include "jit/code_block.h"

#include "code_block_intp.h"

void code_block_intp_init(struct code_block_intp *block) {
    memset(block, 0, sizeof(*block));
}

void code_block_intp_cleanup(struct code_block_intp *block) {
    if (block->inst_list)
        free(block->inst_list);
}

void code_block_intp_compile(struct code_block_intp *out,
                             struct il_code_block const *il_blk) {
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
    out->cycle_count = il_blk->cycle_count;
    out->inst_count = inst_count;
}

void code_block_intp_exec(struct code_block_intp const *block) {
    unsigned inst_count = block->inst_count;
    struct jit_inst const* inst = block->inst_list;
    Sh4 *cpu = dreamcast_get_cpu();
    addr32_t jump_addr = 0;
    addr32_t alt_jump_addr = 0; // where a cond jump goes to if the jump fails
    bool cond_jump_flag = false;
    reg32_t old_sr;

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            inst->immed.fallback.fallback_fn(cpu, inst->immed.fallback.inst);
            inst++;
            break;
        case JIT_OP_PREPARE_JUMP:
            jump_addr = cpu->reg[inst->immed.prepare_jump.reg_idx] +
                inst->immed.prepare_jump.offs;
            inst++;
            break;
        case JIT_OP_PREPARE_JUMP_CONST:
            jump_addr = inst->immed.prepare_jump_const.new_pc;
            inst++;
            break;
        case JIT_OP_PREPARE_ALT_JUMP:
            alt_jump_addr = inst->immed.prepare_alt_jump.new_pc;
            inst++;
            break;
        case JIT_OP_JUMP:
            cpu->reg[SH4_REG_PC] = jump_addr;
            return;
        case JIT_SET_COND_JUMP_BASED_ON_T:
            /*
             * set conditional jump flag if t_flag == the sh4's t flag.
             */
            cond_jump_flag =
                ((bool)(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)) ==
                inst->immed.set_cond_jump_based_on_t.t_flag;
            inst++;
            break;
        case JIT_JUMP_COND:
            /*
             * This ends the current block even if the jump was not executed.
             * Otherwise, there would have to be multiple exit points for each
             * block; this would not be impossible to implement but it would
             * mess with the cycle-counting since a given block would not
             * complete in the same number of cycles every time.
             */
            if (cond_jump_flag)
                cpu->reg[SH4_REG_PC] = jump_addr;
            else
                cpu->reg[SH4_REG_PC] = alt_jump_addr;
            return;
        case JIT_SET_REG:
            cpu->reg[inst->immed.set_reg.reg_idx] = inst->immed.set_reg.new_val;
            inst++;
            break;
        case JIT_OP_RESTORE_SR:
            old_sr = cpu->reg[SH4_REG_SR];
            cpu->reg[SH4_REG_SR] = cpu->reg[SH4_REG_SSR];
            sh4_on_sr_change(cpu, old_sr);
            inst++;
            break;
        case JIT_OP_READ_16_REG:
            cpu->reg[inst->immed.read_16_reg.reg_no] =
                sh4_read_mem_16(cpu, inst->immed.read_16_reg.addr);
            inst++;
            break;
        case JIT_OP_SIGN_EXTEND_16:
            cpu->reg[inst->immed.sign_extend_16.reg_no] =
                (int32_t)(int16_t)cpu->reg[inst->immed.sign_extend_16.reg_no];
            inst++;
            break;
        case JIT_OP_READ_32_REG:
            cpu->reg[inst->immed.read_32_reg.reg_no] =
                sh4_read_mem_32(cpu, inst->immed.read_32_reg.addr);
            inst++;
            break;
        }
    }

    // all blocks should end by jumping out
    LOG_ERROR("ERROR: %u-len block does not jump out\n", block->inst_count);
    RAISE_ERROR(ERROR_INTEGRITY);
}
