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

#include "jit_il.h"

void jit_fallback(struct jit_inst *op,
                  void(*fallback_fn)(Sh4*,Sh4OpArgs), inst_t inst) {
    op->op = JIT_OP_FALLBACK;
    op->immed.fallback.fallback_fn = fallback_fn;
    op->immed.fallback.inst.inst = inst;
}

void jit_prepare_jump(struct jit_inst *op, unsigned sh4_reg_idx,
                      unsigned offs) {
    op->op = JIT_OP_PREPARE_JUMP;
    op->immed.prepare_jump.reg_idx = sh4_reg_idx;
    op->immed.prepare_jump.offs = offs;
}

void jit_prepare_jump_const(struct jit_inst *op, unsigned new_pc) {
    op->op = JIT_OP_PREPARE_JUMP_CONST;
    op->immed.prepare_jump_const.new_pc = new_pc;
}

void jit_prepare_alt_jump(struct jit_inst *op, unsigned new_pc) {
    op->op = JIT_OP_PREPARE_ALT_JUMP;
    op->immed.prepare_alt_jump.new_pc = new_pc;
}

void jit_jump(struct jit_inst *op) {
    op->op = JIT_OP_JUMP;
}

void jit_set_cond_jump_based_on_t(struct jit_inst *op, unsigned t_val) {
    op->op = JIT_SET_COND_JUMP_BASED_ON_T;
    op->immed.set_cond_jump_based_on_t.t_flag = t_val;
}

void jit_jump_cond(struct jit_inst *op) {
    op->op = JIT_JUMP_COND;
}

void jit_set_reg(struct jit_inst *op, unsigned reg_idx, uint32_t new_val) {
    op->op = JIT_SET_REG;
    op->immed.set_reg.new_val = new_val;
    op->immed.set_reg.reg_idx = reg_idx;
}

void jit_restore_sr(struct jit_inst *op) {
    op->op = JIT_OP_RESTORE_SR;
}

void jit_read_16_reg(struct jit_inst *op, addr32_t addr, unsigned reg_no) {
    op->op = JIT_OP_READ_16_REG;
    op->immed.read_16_reg.addr = addr;
    op->immed.read_16_reg.reg_no = reg_no;
}

void jit_sign_extend_16(struct jit_inst *op, unsigned reg_no) {
    op->op = JIT_OP_SIGN_EXTEND_16;
    op->immed.sign_extend_16.reg_no = reg_no;
}

void jit_read_32_reg(struct jit_inst *op, addr32_t addr, unsigned reg_no) {
    op->op = JIT_OP_READ_32_REG;
    op->immed.read_32_reg.addr = addr;
    op->immed.read_32_reg.reg_no = reg_no;
}
