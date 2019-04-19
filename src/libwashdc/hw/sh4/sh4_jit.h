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

#ifndef SH4_JIT_H_
#define SH4_JIT_H_

#include <stdbool.h>

#include "washdc/cpu.h"
#include "washdc/types.h"
#include "sh4_inst.h"
#include "jit/jit_il.h"
#include "jit/code_block.h"

#ifdef ENABLE_JIT_X86_64
#include "jit/x86_64/code_block_x86_64.h"
#endif

struct InstOpcode;
struct il_code_block;
struct Sh4;

/*
 * call this at the beginning of every new block to reset the disassembler's
 * state to its default configuration.
 */
void sh4_jit_new_block(void);

struct sh4_jit_compile_ctx {
    unsigned last_inst_type;
    unsigned cycle_count;
};

bool
sh4_jit_compile_inst(struct Sh4 *sh4, struct sh4_jit_compile_ctx *ctx,
                     struct il_code_block *block, unsigned pc);

static inline void
sh4_jit_il_code_block_compile(struct Sh4 *sh4, struct sh4_jit_compile_ctx *ctx,
                              struct il_code_block *block, addr32_t addr) {
    bool do_continue;

    sh4_jit_new_block();

    do {
        do_continue = sh4_jit_compile_inst(sh4, ctx, block, addr);
        addr += 2;
    } while (do_continue);
}

#ifdef ENABLE_JIT_X86_64
static inline void
sh4_jit_compile_native(void *cpu, void *blk_ptr, uint32_t pc) {
    struct il_code_block il_blk;
    struct code_block_x86_64 *blk = (struct code_block_x86_64*)blk_ptr;
    struct sh4_jit_compile_ctx ctx = { .last_inst_type = SH4_GROUP_NONE,
                                       .cycle_count = 0 };

    il_code_block_init(&il_blk);
    sh4_jit_il_code_block_compile(cpu, &ctx, &il_blk, pc);
#ifdef JIT_OPTIMIZE
    jit_determ_pass(&il_blk);
#endif
    code_block_x86_64_compile(cpu, blk, &il_blk, sh4_jit_compile_native,
                              ctx.cycle_count * SH4_CLOCK_SCALE);
    il_code_block_cleanup(&il_blk);
}
#endif

static inline void
sh4_jit_compile_intp(void *cpu, void *blk_ptr, uint32_t pc) {
    struct il_code_block il_blk;
    struct code_block_intp *blk = (struct code_block_intp*)blk_ptr;
    struct sh4_jit_compile_ctx ctx = { .last_inst_type = SH4_GROUP_NONE,
                                       .cycle_count = 0 };

    il_code_block_init(&il_blk);
    sh4_jit_il_code_block_compile(cpu, &ctx, &il_blk, pc);
#ifdef JIT_OPTIMIZE
    jit_determ_pass(&il_blk);
#endif
    code_block_intp_compile(cpu, blk, &il_blk, ctx.cycle_count * SH4_CLOCK_SCALE);
    il_code_block_cleanup(&il_blk);
}

/*
 * disassembly function that emits a function call to the instruction's
 * interpreter implementation.
 */
bool sh4_jit_fallback(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// disassemble the rts instruction
bool sh4_jit_rts(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassemble the rte instruction
bool sh4_jit_rte(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassemble the "braf rn" instruction.
bool sh4_jit_braf_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bsrf rn" instruction"
bool sh4_jit_bsrf_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bf" instruction
bool sh4_jit_bf(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                struct il_code_block *block, unsigned pc,
                struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bt" instruction
bool sh4_jit_bt(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                struct il_code_block *block, unsigned pc,
                struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bf/s" instruction
bool sh4_jit_bfs(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bt/s" instruction
bool sh4_jit_bts(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bra" instruction
bool sh4_jit_bra(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "bsr" instruction
bool sh4_jit_bsr(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "jmp @rn" instruction
bool sh4_jit_jmp_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "jsr @rn" instruction
bool sh4_jit_jsr_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "mov.w @(disp, pc), rn" instruction
bool
sh4_jit_movw_a_disp_pc_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);

// disassembles the "mov.l @(disp, pc), rn" instruction
bool
sh4_jit_movl_a_disp_pc_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);
bool
sh4_jit_mova_a_disp_pc_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);

bool sh4_jit_nop(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst);
bool sh4_jit_ocbi_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);
bool sh4_jit_ocbp_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);
bool sh4_jit_ocbwb_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// ADD Rm, Rn
// 0011nnnnmmmm1100
bool sh4_jit_add_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// ADD #imm, Rn
// 0111nnnniiiiiiii
bool sh4_jit_add_imm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst);

// XOR Rm, Rn
// 0010nnnnmmmm1010
bool sh4_jit_xor_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// MOV Rm, Rn
// 0110nnnnmmmm0011
bool sh4_jit_mov_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// AND Rm, Rn
// 0010nnnnmmmm1001
bool sh4_jit_and_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// OR Rm, Rn
// 0010nnnnmmmm1011
bool sh4_jit_or_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// SUB Rm, Rn
// 0011nnnnmmmm1000
bool sh4_jit_sub_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// AND #imm, R0
// 11001001iiiiiiii
bool
sh4_inst_binary_andb_imm_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                            struct il_code_block *block, unsigned pc,
                            struct InstOpcode const *op, cpu_inst_param inst);

// OR #imm, R0
// 11001011iiiiiiii
bool sh4_jit_or_imm8_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst);

// XOR #imm, R0
// 11001010iiiiiiii
bool sh4_jit_xor_imm8_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// TST Rm, Rn
// 0010nnnnmmmm1000
bool sh4_jit_tst_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// TST #imm, R0
// 11001000iiiiiiii
bool sh4_jit_tst_imm8_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
bool sh4_jit_movl_arm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// MOV.L @Rm+, Rn
// 0110nnnnmmmm0110
bool sh4_jit_movl_armp_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);

// LDS.L @Rm+, PR
// 0100mmmm00100110
bool sh4_jit_ldsl_armp_pr(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);
// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
bool
sh4_jit_movl_a_disp4_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                           struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, cpu_inst_param inst);

// MOV #imm, Rn
// 1110nnnniiiiiiii
bool sh4_jit_mov_imm8_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// SHLL16 Rn
// 0100nnnn00101000
bool sh4_jit_shll16_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// SHLL2 Rn
// 0100nnnn00001000
bool sh4_jit_shll2_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// SHLL8 Rn
// 0100nnnn00011000
bool sh4_jit_shll8_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// SHAR Rn
// 0100nnnn00100001
bool sh4_jit_shar_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// SHLR Rn
// 0100nnnn00000001
bool sh4_jit_shlr_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// SHAD Rm, Rn
// 0100nnnnmmmm1100
bool sh4_jit_shad_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst);

// SHLR2 Rn
// 0100nnnn00001001
bool sh4_jit_shlr2_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// SHLR8 Rn
// 0100nnnn00011001
bool sh4_jit_shlr8_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// SHLR16 Rn
// 0100nnnn00101001
bool sh4_jit_shlr16_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// SHLL Rn
// 0100nnnn00000000
bool sh4_jit_shll_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// SHAL Rn
// 0100nnnn00100000
bool sh4_jit_shal_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst);

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
bool sh4_jit_swapw_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
bool sh4_jit_cmphi_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// MULU.W Rm, Rn
// 0010nnnnmmmm1110
bool sh4_jit_muluw_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// STS MACL, Rn
// 0000nnnn00011010
bool sh4_jit_sts_macl_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// STS MACL, Rn
// 0000nnnn00011010
bool sh4_jit_sts_macl_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// MOV.L Rm, @-Rn
// 0010nnnnmmmm0110
bool sh4_jit_movl_rm_amrn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst);

// MOV.L Rm, @Rn
// 0010nnnnmmmm0010
bool sh4_jit_movl_rm_arn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/EQ Rm, Rn
// 0011nnnnmmmm0000
bool sh4_jit_cmpeq_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/HS Rm, Rn
// 0011nnnnmmmm0010
bool sh4_jit_cmphs_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/GT Rm, Rn
// 0011nnnnmmmm0111
bool sh4_jit_cmpgt_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/GE Rm, Rn
// 0011nnnnmmmm0011
bool sh4_jit_cmpge_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst);

// CMP/PL Rn
// 0100nnnn00010101
bool sh4_jit_cmppl_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// CMP/PZ Rn
// 0100nnnn00010001
bool sh4_jit_cmppz_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst);

// NOT Rm, Rn
// 0110nnnnmmmm0111
bool sh4_jit_not_rm_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst);

// DT Rn
// 0100nnnn00010000
bool sh4_jit_dt_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                   struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, cpu_inst_param inst);

// CLRT
// 0000000000001000
bool sh4_jit_clrt(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst);

// SETT
// 0000000000011000
bool sh4_jit_sett(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst);

// MOVT Rn
// 0000nnnn00101001
bool sh4_jit_movt(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst);

// MOV.L @(disp, GBR), R0
// 11000110dddddddd
bool
sh4_jit_movl_a_disp8_gbr_r0(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                            struct il_code_block *block, unsigned pc,
                            struct InstOpcode const *op, cpu_inst_param inst);

#endif
