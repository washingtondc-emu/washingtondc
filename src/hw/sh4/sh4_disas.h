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

// ADD Rm, Rn
// 0011nnnnmmmm1100
bool sh4_disas_add_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// ADD #imm, Rn
// 0111nnnniiiiiiii
bool sh4_disas_add_imm_rn(struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, inst_t inst);

// XOR Rm, Rn
// 0010nnnnmmmm1010
bool sh4_disas_xor_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// MOV Rm, Rn
// 0110nnnnmmmm0011
bool sh4_disas_mov_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// AND Rm, Rn
// 0010nnnnmmmm1001
bool sh4_disas_and_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// OR Rm, Rn
// 0010nnnnmmmm1011
bool sh4_disas_or_rm_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// SUB Rm, Rn
// 0011nnnnmmmm1000
bool sh4_disas_sub_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// AND #imm, R0
// 11001001iiiiiiii
bool sh4_inst_binary_andb_imm_r0(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst);

// OR #imm, R0
// 11001011iiiiiiii
bool sh4_disas_or_imm8_r0(struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, inst_t inst);

// XOR #imm, R0
// 11001010iiiiiiii
bool sh4_disas_xor_imm8_r0(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst);

// TST Rm, Rn
// 0010nnnnmmmm1000
bool sh4_disas_tst_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
bool sh4_disas_movl_arm_rn(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst);
// LDS.L @Rm+, PR
// 0100mmmm00100110
bool sh4_disas_ldsl_armp_pr(struct il_code_block *block, unsigned pc,
                            struct InstOpcode const *op, inst_t inst);
// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
bool sh4_disas_movl_a_disp4_rm_rn(struct il_code_block *block, unsigned pc,
                                  struct InstOpcode const *op, inst_t inst);

// MOV #imm, Rn
// 1110nnnniiiiiiii
bool sh4_disas_mov_imm8_rn(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst);

// SHLL16 Rn
// 0100nnnn00101000
bool sh4_disas_shll16_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// SHLL2 Rn
// 0100nnnn00001000
bool sh4_disas_shll2_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// SHLL8 Rn
// 0100nnnn00011000
bool sh4_disas_shll8_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// SHAR Rn
// 0100nnnn00100001
bool sh4_disas_shar_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// SHLR Rn
// 0100nnnn00000001
bool sh4_disas_shlr_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// SHLR2 Rn
// 0100nnnn00001001
bool sh4_disas_shlr2_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// SHLR8 Rn
// 0100nnnn00011001
bool sh4_disas_shlr8_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst);

// SHLR16 Rn
// 0100nnnn00101001
bool sh4_disas_shlr16_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst);

// SHLL Rn
// 0100nnnn00000000
bool sh4_disas_shll_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// SHAL Rn
// 0100nnnn00100000
bool sh4_disas_shal_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst);

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
bool sh4_disas_swapw_rm_rn(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst);

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
bool sh4_disas_cmphi_rm_rn(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst);

#endif
