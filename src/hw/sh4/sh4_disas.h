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

#ifndef SH4_DISAS_H_
#define SH4_DISAS_H_

#include <stdbool.h>

#include "types.h"

struct InstOpcode;
struct il_code_block;

/*
 * call this at the beginning of every new block to reset the disassembler's
 * state to its default configuration.
 */
void sh4_disas_new_block(void);

bool sh4_disas_inst(struct il_code_block *block, unsigned pc);

/*
 * these functions return true if the disassembler should keep going, or false
 * if the dissassembler should end the current block.
 */
typedef bool(*sh4_disas_fn)(struct il_code_block*,unsigned,
                            struct InstOpcode const*,inst_t);

/*
 * disassembly function that emits a function call to the instruction's
 * interpreter implementation.
 */
bool sh4_disas_fallback(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// disassemble the rts instruction
bool sh4_disas_rts(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassemble the rte instruction
bool sh4_disas_rte(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassemble the "braf rn" instruction.
bool sh4_disas_braf_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// disassembles the "bsrf rn" instruction"
bool sh4_disas_bsrf_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// disassembles the "bf" instruction
bool sh4_disas_bf(struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, inst_t inst);

// disassembles the "bt" instruction
bool sh4_disas_bt(struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, inst_t inst);

// disassembles the "bf/s" instruction
bool sh4_disas_bfs(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassembles the "bt/s" instruction
bool sh4_disas_bts(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassembles the "bra" instruction
bool sh4_disas_bra(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassembles the "bsr" instruction
bool sh4_disas_bsr(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);

// disassembles the "jmp @rn" instruction
bool sh4_disas_jmp_arn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// disassembles the "jsr @rn" instruction
bool sh4_disas_jsr_arn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// disassembles the "mov.w @(disp, pc), rn" instruction
bool sh4_disas_movw_a_disp_pc_rn(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst);

// disassembles the "mov.l @(disp, pc), rn" instruction
bool sh4_disas_movl_a_disp_pc_rn(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst);
bool sh4_disas_mova_a_disp_pc_r0(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst);

bool sh4_disas_nop(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst);
bool sh4_disas_ocbi_arn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);
bool sh4_disas_ocbp_arn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);
bool sh4_disas_ocbwb_arn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

#endif
