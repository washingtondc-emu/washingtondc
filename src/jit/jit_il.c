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

void jit_prepare_alt_jump(struct jit_inst *op, unsigned new_pc) {
    op->op = JIT_OP_PREPARE_ALT_JUMP;
    op->immed.prepare_alt_jump.new_pc = new_pc;
}

void jit_jump(struct jit_inst *op) {
    op->op = JIT_OP_JUMP;
}

void jit_mov_reg(struct jit_inst *op, unsigned reg_src, unsigned reg_dst) {
    op->op = JIT_OP_MOV_REG;
    op->immed.mov_reg.reg_src = reg_src;
    op->immed.mov_reg.reg_dst = reg_dst;
}

void jit_add_const_reg(struct jit_inst *op, unsigned const_val,
                       unsigned reg_dst) {
    op->op = JIT_ADD_CONST_REG;
    op->immed.add_const_reg.const_val = const_val;
    op->immed.add_const_reg.reg_dst = reg_dst;
}

void jit_set_cond_jump_based_on_t(struct jit_inst *op, unsigned t_val) {
    op->op = JIT_SET_COND_JUMP_BASED_ON_T;
    op->immed.set_cond_jump_based_on_t.t_flag = t_val;
}

void jit_jump_cond(struct jit_inst *op) {
    op->op = JIT_JUMP_COND;
}
