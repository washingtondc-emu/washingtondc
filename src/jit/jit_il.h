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

#ifndef JIT_IL_H_
#define JIT_IL_H_

// for union Sh4OpArgs
#include "hw/sh4/sh4_inst.h"

enum jit_opcode {
    // this opcode calls an interpreter function
    JIT_OP_FALLBACK,

    /*
     * this stores a given register plus a constant
     * offset as the branch destination
     */
    JIT_OP_PREPARE_JUMP,

    // this sets a given constant as the branch destination
    JIT_OP_PREPARE_JUMP_CONST,

    /*
     * This stores a given register plus a constant offset as a jump
     * destination for a failed conditional jump.
     */
    JIT_OP_PREPARE_ALT_JUMP,

    // This jumps to the jump destination address previously stored
    JIT_OP_JUMP,

    // this copies one register into another
    JIT_OP_MOV_REG,

    // this adds a constant value into a register
    JIT_ADD_CONST_REG,

    /*
     * This can be configured to set the conditional jump flag if t is set, or
     * it can be configured to set the conditional jump flag if t is not set.
     */
    JIT_SET_COND_JUMP_BASED_ON_T,

    // this will jump iff the conditional jump flag is set
    JIT_JUMP_COND,

    // this will set a register to the given constant value
    JIT_SET_REG,

    // this will copy SSR into SR and handle any state changes
    JIT_OP_RESTORE_SR
};

struct jit_fallback_immed {
    void(*fallback_fn)(Sh4*,Sh4OpArgs);
    Sh4OpArgs inst;
};

struct prepare_jump_immed {
    unsigned reg_idx;
    unsigned offs; // constant offset added to the register
};

struct prepare_jump_const_immed {
    unsigned new_pc;
};

struct prepare_alt_jump_immed {
    unsigned new_pc;
};

struct mov_reg_immed {
    unsigned reg_src, reg_dst;
};

struct add_const_reg_immed {
    unsigned const_val;
    unsigned reg_dst;
};

struct set_cond_jump_based_on_t_immed {
    unsigned t_flag;
};

struct set_reg_immed {
    unsigned reg_idx;
    uint32_t new_val;
};

union jit_immed {
    struct jit_fallback_immed fallback;
    struct prepare_jump_immed prepare_jump;
    struct prepare_jump_const_immed prepare_jump_const;
    struct prepare_alt_jump_immed prepare_alt_jump;
    struct mov_reg_immed mov_reg;
    struct add_const_reg_immed add_const_reg;
    struct set_cond_jump_based_on_t_immed set_cond_jump_based_on_t;
    struct set_reg_immed set_reg;
};

struct jit_inst {
    enum jit_opcode op;
    union jit_immed immed;
};

void jit_fallback(struct jit_inst *op,
                  void(*fallback_fn)(Sh4*,Sh4OpArgs), inst_t inst);
void jit_prepare_jump(struct jit_inst *op, unsigned sh4_reg_idx, unsigned offs);
void jit_prepare_jump_const(struct jit_inst *op, unsigned new_pc);
void jit_prepare_alt_jump(struct jit_inst *op, unsigned new_pc);
void jit_jump(struct jit_inst *op);
void jit_mov_reg(struct jit_inst *op, unsigned reg_src, unsigned reg_dst);
void jit_add_const_reg(struct jit_inst *op, unsigned const_val,
                       unsigned reg_dst);
void jit_set_cond_jump_based_on_t(struct jit_inst *op, unsigned t_val);
void jit_jump_cond(struct jit_inst *op);
void jit_set_reg(struct jit_inst *op, unsigned reg_idx, uint32_t new_val);
void jit_restore_sr(struct jit_inst *op);

#endif
