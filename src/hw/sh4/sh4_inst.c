/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#include <fenv.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "error.h"
#include "dreamcast.h"
#include "sh4_ocache.h"
#include "sh4_mmu.h"
#include "sh4.h"
#include "sh4_excp.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

#include "sh4_inst.h"

DEF_ERROR_STRING_ATTR(opcode_format)
DEF_ERROR_STRING_ATTR(opcode_name)

static InstOpcode const* sh4_decode_inst_slow(inst_t inst);

static struct InstOpcode opcode_list[] = {
    // RTS
    { "0000000000001011", &sh4_inst_rts, true, SH4_GROUP_CO, 2 },

    // CLRMAC
    { "0000000000101000", &sh4_inst_clrmac, false, SH4_GROUP_CO, 1 },

    // CLRS
    { "0000000001001000", &sh4_inst_clrs, false, SH4_GROUP_CO, 1 },

    // CLRT
    { "0000000000001000", &sh4_inst_clrt, false, SH4_GROUP_MT, 1 },

    // LDTLB
    { "0000000000111000", &sh4_inst_ldtlb, false, SH4_GROUP_CO, 1 },

    // NOP
    { "0000000000001001", &sh4_inst_nop, false, SH4_GROUP_MT, 1 },

    // RTE
    { "0000000000101011", &sh4_inst_rte, false, SH4_GROUP_CO, 5 },

    // SETS
    { "0000000001011000", &sh4_inst_sets, false, SH4_GROUP_CO, 1 },

    // SETT
    { "0000000000011000", &sh4_inst_sett, false, SH4_GROUP_MT, 1 },

    // SLEEP
    { "0000000000011011", &sh4_inst_sleep, false, SH4_GROUP_CO, 4 },

    // FRCHG
    { "1111101111111101", &sh4_inst_frchg, false, SH4_GROUP_FE, 1 },

    // FSCHG
    { "1111001111111101", &sh4_inst_fschg, false, SH4_GROUP_FE, 1 },

    // MOVT Rn
    { "0000nnnn00101001", &sh4_inst_unary_movt_gen, false,
      SH4_GROUP_EX, 1 },

    // CMP/PZ
    { "0100nnnn00010001", &sh4_inst_unary_cmppz_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/PL
    { "0100nnnn00010101", &sh4_inst_unary_cmppl_gen, false,
      SH4_GROUP_MT, 1 },

    // DT
    { "0100nnnn00010000", &sh4_inst_unary_dt_gen, false,
      SH4_GROUP_EX, 1 },

    // ROTL Rn
    { "0100nnnn00000100", &sh4_inst_unary_rotl_gen, false,
      SH4_GROUP_EX, 1 },

    // ROTR Rn
    { "0100nnnn00000101", &sh4_inst_unary_rotr_gen, false,
      SH4_GROUP_EX, 1 },

    // ROTCL Rn
    { "0100nnnn00100100", &sh4_inst_unary_rotcl_gen, false,
      SH4_GROUP_EX, 1 },

    // ROTCR Rn
    { "0100nnnn00100101", &sh4_inst_unary_rotcr_gen, false,
      SH4_GROUP_EX, 1 },

    // SHAL Rn
    { "0100nnnn00200000", &sh4_inst_unary_shal_gen, false,
      SH4_GROUP_EX, 1 },

    // SHAR Rn
    { "0100nnnn00100001", &sh4_inst_unary_shar_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLL Rn
    { "0100nnnn00000000", &sh4_inst_unary_shll_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLR Rn
    { "0100nnnn00000001", &sh4_inst_unary_shlr_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLL2 Rn
    { "0100nnnn00001000", &sh4_inst_unary_shll2_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLR2 Rn
    { "0100nnnn00001001", &sh4_inst_unary_shlr2_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLL8 Rn
    { "0100nnnn00011000", &sh4_inst_unary_shll8_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLR8 Rn
    { "0100nnnn00011001", &sh4_inst_unary_shlr8_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLL16 Rn
    { "0100nnnn00101000", &sh4_inst_unary_shll16_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLR16 Rn
    { "0100nnnn00101001", &sh4_inst_unary_shlr16_gen, false,
      SH4_GROUP_EX, 1 },

    // BRAF Rn
    { "0000nnnn00100011", &sh4_inst_unary_braf_gen, true,
      SH4_GROUP_CO, 2 },

    // BSRF Rn
    { "0000nnnn00000011", &sh4_inst_unary_bsrf_gen, true,
      SH4_GROUP_CO, 2 },

    // CMP/EQ #imm, R0
    { "10001000iiiiiiii", &sh4_inst_binary_cmpeq_imm_r0, false,
      SH4_GROUP_MT, 1 },

    // AND.B #imm, @(R0, GBR)
    { "11001101iiiiiiii", &sh4_inst_binary_andb_imm_r0_gbr, false,
      SH4_GROUP_CO, 4 },

    // AND #imm, R0
    { "11001001iiiiiiii", &sh4_inst_binary_and_imm_r0, false,
      SH4_GROUP_EX, 1 },

    // OR.B #imm, @(R0, GBR)
    { "11001111iiiiiiii", &sh4_inst_binary_orb_imm_r0_gbr, false,
      SH4_GROUP_CO, 4 },

    // OR #imm, R0
    { "11001011iiiiiiii", &sh4_inst_binary_or_imm_r0, false,
      SH4_GROUP_EX, 1 },

    // TST #imm, R0
    { "11001000iiiiiiii", &sh4_inst_binary_tst_imm_r0, false,
      SH4_GROUP_MT, 1 },

    // TST.B #imm, @(R0, GBR)
    { "11001100iiiiiiii", &sh4_inst_binary_tstb_imm_r0_gbr, false,
      SH4_GROUP_CO, 3 },

    // XOR #imm, R0
    { "11001010iiiiiiii", &sh4_inst_binary_xor_imm_r0, false,
      SH4_GROUP_EX, 1 },

    // XOR.B #imm, @(R0, GBR)
    { "11001110iiiiiiii", &sh4_inst_binary_xorb_imm_r0_gbr, false,
      SH4_GROUP_CO, 4 },

    // BF label
    { "10001011dddddddd", &sh4_inst_unary_bf_disp, true,
      SH4_GROUP_BR, 1 },

    // BF/S label
    { "10001111dddddddd", &sh4_inst_unary_bfs_disp, true,
      SH4_GROUP_BR, 1 },

    // BT label
    { "10001001dddddddd", &sh4_inst_unary_bt_disp, true,
      SH4_GROUP_BR, 1 },

    // BT/S label
    { "10001101dddddddd", &sh4_inst_unary_bts_disp, true,
      SH4_GROUP_BR, 1 },

    // BRA label
    { "1010dddddddddddd", &sh4_inst_unary_bra_disp, true,
      SH4_GROUP_BR, 1 },

    // BSR label
    { "1011dddddddddddd", &sh4_inst_unary_bsr_disp, true,
      SH4_GROUP_BR, 1 },

    // TRAPA #immed
    { "11000011iiiiiiii", &sh4_inst_unary_trapa_disp, false,
      SH4_GROUP_CO, 7 },

    // TAS.B @Rn
    { "0100nnnn00011011", &sh4_inst_unary_tasb_gen, false,
      SH4_GROUP_CO, 5 },

    // OCBI @Rn
    { "0000nnnn10100011", &sh4_inst_unary_ocbi_indgen, false,
      SH4_GROUP_LS, 1 },

    // OCBP @Rn
    { "0000nnnn10100011", &sh4_inst_unary_ocbp_indgen, false,
      SH4_GROUP_LS, 1 },

    // PREF @Rn
    { "0000nnnn10000011", &sh4_inst_unary_pref_indgen, false,
      SH4_GROUP_LS, 1 },

    // JMP @Rn
    { "0100nnnn00101011", &sh4_inst_unary_jmp_indgen, true,
      SH4_GROUP_CO, 2 },

    // JSR @Rn
    { "0100nnnn00001011", &sh4_inst_unary_jsr_indgen, true,
      SH4_GROUP_CO, 2 },

    // LDC Rm, SR
    { "0100mmmm00001110", &sh4_inst_binary_ldc_gen_sr, false,
      SH4_GROUP_CO, 4 },

    // LDC Rm, GBR
    { "0100mmmm00011110", &sh4_inst_binary_ldc_gen_gbr, false,
      SH4_GROUP_CO, 3 },

    // LDC Rm, VBR
    { "0100mmmm00101110", &sh4_inst_binary_ldc_gen_vbr, false,
      SH4_GROUP_CO, 1 },

    // LDC Rm, SSR
    { "0100mmmm00111110", &sh4_inst_binary_ldc_gen_ssr, false,
      SH4_GROUP_CO, 1 },

    // LDC Rm, SPC
    { "0100mmmm01001110", &sh4_inst_binary_ldc_gen_spc, false,
      SH4_GROUP_CO, 1 },

    // LDC Rm, DBR
    { "0100mmmm11111010", &sh4_inst_binary_ldc_gen_dbr, false,
      SH4_GROUP_CO, 1 },

    // STC SR, Rn
    { "0000nnnn00000010", &sh4_inst_binary_stc_sr_gen, false,
      SH4_GROUP_CO, 2 },

    // STC GBR, Rn
    { "0000nnnn00010010", &sh4_inst_binary_stc_gbr_gen, false,
      SH4_GROUP_CO, 2 },

    // STC VBR, Rn
    { "0000nnnn00100010", &sh4_inst_binary_stc_vbr_gen, false,
      SH4_GROUP_CO, 2 },

    // STC SSR, Rn
    { "0000nnnn00110010", &sh4_inst_binary_stc_ssr_gen, false,
      SH4_GROUP_CO, 2 },

    // STC SPC, Rn
    { "0000nnnn01000010", &sh4_inst_binary_stc_spc_gen, false,
      SH4_GROUP_CO, 2 },

    // STC SGR, Rn
    { "0000nnnn00111010", &sh4_inst_binary_stc_sgr_gen, false,
      SH4_GROUP_CO, 3 },

    // STC DBR, Rn
    { "0000nnnn11111010", &sh4_inst_binary_stc_dbr_gen, false,
      SH4_GROUP_CO, 2 },

    // LDC.L @Rm+, SR
    { "0100mmmm00000111", &sh4_inst_binary_ldcl_indgeninc_sr, false,
      SH4_GROUP_CO, 4 },

    // LDC.L @Rm+, GBR
    { "0100mmmm00010111", &sh4_inst_binary_ldcl_indgeninc_gbr, false,
      SH4_GROUP_CO, 3 },

    // LDC.L @Rm+, VBR
    { "0100mmmm00100111", &sh4_inst_binary_ldcl_indgeninc_vbr, false,
      SH4_GROUP_CO, 1 },

    // LDC.L @Rm+, SSR
    { "0100mmmm00110111", &sh4_inst_binary_ldcl_indgenic_ssr, false,
      SH4_GROUP_CO, 1 },

    // LDC.L @Rm+, SPC
    { "0100mmmm01000111", &sh4_inst_binary_ldcl_indgeninc_spc, false,
      SH4_GROUP_CO, 1 },

    // LDC.L @Rm+, DBR
    { "0100mmmm11110110", &sh4_inst_binary_ldcl_indgeninc_dbr, false,
      SH4_GROUP_CO, 1 },

    // STC.L SR, @-Rn
    { "0100nnnn00000011", &sh4_inst_binary_stcl_sr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // STC.L GBR, @-Rn
    { "0100nnnn00010011", &sh4_inst_binary_stcl_gbr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // STC.L VBR, @-Rn
    { "0100nnnn00100011", &sh4_inst_binary_stcl_vbr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // STC.L SSR, @-Rn
    { "0100nnnn00110011", &sh4_inst_binary_stcl_ssr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // STC.L SPC, @-Rn
    { "0100nnnn01000011", &sh4_inst_binary_stcl_spc_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // STC.L SGR, @-Rn
    { "0100nnnn00110010", &sh4_inst_binary_stcl_sgr_inddecgen, false,
      SH4_GROUP_CO, 3 },

    // STC.L DBR, @-Rn
    { "0100nnnn11110010", &sh4_inst_binary_stcl_dbr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // MOV #imm, Rn
    { "1110nnnniiiiiiii", &sh4_inst_binary_mov_imm_gen, false,
      SH4_GROUP_EX, 1 },

    // ADD #imm, Rn
    { "0111nnnniiiiiiii", &sh4_inst_binary_add_imm_gen, false,
      SH4_GROUP_EX, 1 },

    // MOV.W @(disp, PC), Rn
    { "1001nnnndddddddd", &sh4_inst_binary_movw_binind_disp_pc_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.L @(disp, PC), Rn
    { "1101nnnndddddddd", &sh4_inst_binary_movl_binind_disp_pc_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV Rm, Rn
    { "0110nnnnmmmm0011", &sh4_inst_binary_movw_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // SWAP.B Rm, Rn
    { "0110nnnnmmmm1000", &sh4_inst_binary_swapb_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SWAP.W Rm, Rn
    { "0110nnnnmmmm1001", &sh4_inst_binary_swapw_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // XTRCT Rm, Rn
    { "0010nnnnmmmm1101", &sh4_inst_binary_xtrct_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // ADD Rm, Rn
    { "0011nnnnmmmm1100", &sh4_inst_binary_add_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // ADDC Rm, Rn
    { "0011nnnnmmmm1110", &sh4_inst_binary_addc_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // ADDV Rm, Rn
    { "0011nnnnmmmm1111", &sh4_inst_binary_addv_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // CMP/EQ Rm, Rn
    { "0011nnnnmmmm0000", &sh4_inst_binary_cmpeq_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/HS Rm, Rn
    { "0011nnnnmmmm0010", &sh4_inst_binary_cmphs_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/GE Rm, Rn
    { "0011nnnnmmmm0011", &sh4_inst_binary_cmpge_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/HI Rm, Rn
    { "0011nnnnmmmm0110", &sh4_inst_binary_cmphi_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/GT Rm, Rn
    { "0011nnnnmmmm0111", &sh4_inst_binary_cmpgt_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // CMP/STR Rm, Rn
    { "0010nnnnmmmm1100", &sh4_inst_binary_cmpstr_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // DIV1 Rm, Rn
    { "0011nnnnmmmm0100", &sh4_inst_binary_div1_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // DIV0S Rm, Rn
    { "0010nnnnmmmm0111", &sh4_inst_binary_div0s_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // DIV0U
    { "0000000000011001", &sh4_inst_noarg_div0u, false, SH4_GROUP_EX, 1 },

    // DMULS.L Rm, Rn
    { "0011nnnnmmmm1101", &sh4_inst_binary_dmulsl_gen_gen, false,
      SH4_GROUP_CO, 2 },

    // DMULU.L Rm, Rn
    { "0011nnnnmmmm0101", &sh4_inst_binary_dmulul_gen_gen, false,
      SH4_GROUP_CO, 2 },

    // EXTS.B Rm, Rn
    { "0110nnnnmmmm1110", &sh4_inst_binary_extsb_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // EXTS.W Rm, Rn
    { "0110nnnnmmmm1111", &sh4_inst_binary_extsw_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // EXTU.B Rm, Rn
    { "0110nnnnmmmm1100", &sh4_inst_binary_extub_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // EXTU.W Rm, Rn
    { "0110nnnnmmmm1101", &sh4_inst_binary_extuw_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // MUL.L Rm, Rn
    { "0000nnnnmmmm0111", &sh4_inst_binary_mull_gen_gen, false,
      SH4_GROUP_CO, 2 },

    // MULS.W Rm, Rn
    { "0010nnnnmmmm1111", &sh4_inst_binary_mulsw_gen_gen, false,
      SH4_GROUP_CO, 2 },

    // MULU.W Rm, Rn
    { "0010nnnnmmmm1110", &sh4_inst_binary_muluw_gen_gen, false,
      SH4_GROUP_CO, 2 },

    // NEG Rm, Rn
    { "0110nnnnmmmm1011", &sh4_inst_binary_neg_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // NEGC Rm, Rn
    { "0110nnnnmmmm1010", &sh4_inst_binary_negc_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SUB Rm, Rn
    { "0011nnnnmmmm1000", &sh4_inst_binary_sub_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SUBC Rm, Rn
    { "0011nnnnmmmm1010", &sh4_inst_binary_subc_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SUBV Rm, Rn
    { "0011nnnnmmmm1011", &sh4_inst_binary_subv_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // AND Rm, Rn
    { "0010nnnnmmmm1001", &sh4_inst_binary_and_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // NOT Rm, Rn
    { "0110nnnnmmmm0111", &sh4_inst_binary_not_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // OR Rm, Rn
    { "0010nnnnmmmm1011", &sh4_inst_binary_or_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // TST Rm, Rn
    { "0010nnnnmmmm1000", &sh4_inst_binary_tst_gen_gen, false,
      SH4_GROUP_MT, 1 },

    // XOR Rm, Rn
    { "0010nnnnmmmm1010", &sh4_inst_binary_xor_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SHAD Rm, Rn
    { "0100nnnnmmmm1100", &sh4_inst_binary_shad_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // SHLD Rm, Rn
    { "0100nnnnmmmm1101", &sh4_inst_binary_shld_gen_gen, false,
      SH4_GROUP_EX, 1 },

    // LDC Rm, Rn_BANK
    { "0100mmmm1nnn1110", &sh4_inst_binary_ldc_gen_bank, false,
      SH4_GROUP_CO, 1 },

    // LDC.L @Rm+, Rn_BANK
    { "0100mmmm1nnn0111", &sh4_inst_binary_ldcl_indgeninc_bank, false,
      SH4_GROUP_CO, 1 },

    // STC Rm_BANK, Rn
    { "0000nnnn1mmm0010", &sh4_inst_binary_stc_bank_gen, false,
      SH4_GROUP_CO, 2 },

    // STC.L Rm_BANK, @-Rn
    { "0100nnnn1mmm0011", &sh4_inst_binary_stcl_bank_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // LDS Rm, MACH
    { "0100mmmm00001010", &sh4_inst_binary_lds_gen_mach, false,
      SH4_GROUP_CO, 1 },

    // LDS Rm, MACL
    { "0100mmmm00011010", &sh4_inst_binary_lds_gen_macl, false,
      SH4_GROUP_CO, 1 },

    // STS MACH, Rn
    { "0000nnnn00001010", &sh4_inst_binary_sts_mach_gen, false,
      SH4_GROUP_CO, 1 },

    // STS MACL, Rn
    { "0000nnnn00011010", &sh4_inst_binary_sts_macl_gen, false,
      SH4_GROUP_CO, 1 },

    // LDS Rm, PR
    { "0100mmmm00101010", &sh4_inst_binary_lds_gen_pr, false,
      SH4_GROUP_CO, 2 },

    // STS PR, Rn
    { "0000nnnn00101010", &sh4_inst_binary_sts_pr_gen, false,
      SH4_GROUP_CO, 2 },

    // LDS.L @Rm+, MACH
    { "0100mmmm00000110", &sh4_inst_binary_ldsl_indgeninc_mach, false,
      SH4_GROUP_CO, 1 },

    // LDS.L @Rm+, MACL
    { "0100mmmm00010110", &sh4_inst_binary_ldsl_indgeninc_macl, false,
      SH4_GROUP_CO, 1 },

    // STS.L MACH, @-Rn
    { "0100mmmm00000010", &sh4_inst_binary_stsl_mach_inddecgen, false,
      SH4_GROUP_CO, 1 },

    // STS.L MACL, @-Rn
    { "0100mmmm00010010", &sh4_inst_binary_stsl_macl_inddecgen, false,
      SH4_GROUP_CO, 1 },

    // LDS.L @Rm+, PR
    { "0100mmmm00100110", &sh4_inst_binary_ldsl_indgeninc_pr, false,
      SH4_GROUP_CO, 2 },

    // STS.L PR, @-Rn
    { "0100nnnn00100010", &sh4_inst_binary_stsl_pr_inddecgen, false,
      SH4_GROUP_CO, 2 },

    // MOV.B Rm, @Rn
    { "0010nnnnmmmm0000", &sh4_inst_binary_movb_gen_indgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.W Rm, @Rn
    { "0010nnnnmmmm0001", &sh4_inst_binary_movw_gen_indgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.L Rm, @Rn
    { "0010nnnnmmmm0010", &sh4_inst_binary_movl_gen_indgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.B @Rm, Rn
    { "0110nnnnmmmm0000", &sh4_inst_binary_movb_indgen_gen, false,
      SH4_GROUP_LS, 1 },

    // MOV.W @Rm, Rn
    { "0110nnnnmmmm0001", &sh4_inst_binary_movw_indgen_gen, false,
      SH4_GROUP_LS, 1 },

    // MOV.L @Rm, Rn
    { "0110nnnnmmmm0010", &sh4_inst_binary_movl_indgen_gen, false,
      SH4_GROUP_LS, 1 },

    // MOV.B Rm, @-Rn
    { "0010nnnnmmmm0100", &sh4_inst_binary_movb_gen_inddecgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.W Rm, @-Rn
    { "0010nnnnmmmm0101", &sh4_inst_binary_movw_gen_inddecgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.L Rm, @-Rn
    { "0010nnnnmmmm0110", &sh4_inst_binary_movl_gen_inddecgen, false,
      SH4_GROUP_LS, 1 },

    // MOV.B @Rm+, Rn
    { "0110nnnnmmmm0100", &sh4_inst_binary_movb_indgeninc_gen, false,
      SH4_GROUP_LS, 1 },

    // MOV.W @Rm+, Rn
    { "0110nnnnmmmm0101", &sh4_inst_binary_movw_indgeninc_gen, false,
      SH4_GROUP_LS, 1 },

    // MOV.L @Rm+, Rn
    { "0110nnnnmmmm0110", &sh4_inst_binary_movl_indgeninc_gen, false,
      SH4_GROUP_LS, 1 },

    // MAC.L @Rm+, @Rn+
    { "0000nnnnmmmm1111", &sh4_inst_binary_macl_indgeninc_indgeninc,
      false, SH4_GROUP_CO, 2 },

    // MAC.W @Rm+, @Rn+
    { "0100nnnnmmmm1111", &sh4_inst_binary_macw_indgeninc_indgeninc,
      false, SH4_GROUP_CO, 2 },

    // MOV.B R0, @(disp, Rn)
    { "10000000nnnndddd", &sh4_inst_binary_movb_r0_binind_disp_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.W R0, @(disp, Rn)
    { "10000001nnnndddd", &sh4_inst_binary_movw_r0_binind_disp_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.L Rm, @(disp, Rn)
    { "0001nnnnmmmmdddd", &sh4_inst_binary_movl_gen_binind_disp_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.B @(disp, Rm), R0
    { "10000100mmmmdddd", &sh4_inst_binary_movb_binind_disp_gen_r0,
      false, SH4_GROUP_LS, 1 },

    // MOV.W @(disp, Rm), R0
    { "10000101mmmmdddd", &sh4_inst_binary_movw_binind_disp_gen_r0,
      false, SH4_GROUP_LS, 1 },

    // MOV.L @(disp, Rm), Rn
    { "0101nnnnmmmmdddd", &sh4_inst_binary_movl_binind_disp_gen_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.B Rm, @(R0, Rn)
    { "0000nnnnmmmm0100", &sh4_inst_binary_movb_gen_binind_r0_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.W Rm, @(R0, Rn)
    { "0000nnnnmmmm0101", &sh4_inst_binary_movw_gen_binind_r0_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.L Rm, @(R0, Rn)
    { "0000nnnnmmmm0110", &sh4_inst_binary_movl_gen_binind_r0_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.B @(R0, Rm), Rn
    { "0000nnnnmmmm1100", &sh4_inst_binary_movb_binind_r0_gen_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.W @(R0, Rm), Rn
    { "0000nnnnmmmm1101", &sh4_inst_binary_movw_binind_r0_gen_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.L @(R0, Rm), Rn
    { "0000nnnnmmmm1110", &sh4_inst_binary_movl_binind_r0_gen_gen,
      false, SH4_GROUP_LS, 1 },

    // MOV.B R0, @(disp, GBR)
    { "11000000dddddddd", &sh4_inst_binary_movb_r0_binind_disp_gbr,
      false, SH4_GROUP_LS, 1 },

    // MOV.W R0, @(disp, GBR)
    { "11000001dddddddd", &sh4_inst_binary_movw_r0_binind_disp_gbr,
      false, SH4_GROUP_LS, 1 },

    // MOV.L R0, @(disp, GBR)
    { "11000010dddddddd", &sh4_inst_binary_movl_r0_binind_disp_gbr,
      false, SH4_GROUP_LS, 1 },

    // MOV.B @(disp, GBR), R0
    { "11000100dddddddd", &sh4_inst_binary_movb_binind_disp_gbr_r0,
      false, SH4_GROUP_LS, 1 },

    // MOV.W @(disp, GBR), R0
    { "11000101dddddddd", &sh4_inst_binary_movw_binind_disp_gbr_r0,
      false, SH4_GROUP_LS, 1 },

    // MOV.L @(disp, GBR), R0
    { "11000110dddddddd", &sh4_inst_binary_movl_binind_disp_gbr_r0,
      false, SH4_GROUP_LS, 1 },

    // MOVA @(disp, PC), R0
    { "11000111dddddddd", &sh4_inst_binary_mova_binind_disp_pc_r0,
      false, SH4_GROUP_EX, 1 },

    // MOVCA.L R0, @Rn
    { "0000nnnn11000011", &sh4_inst_binary_movcal_r0_indgen,
      false, SH4_GROUP_LS, 1 },

    // FLDI0 FRn
    { "1111nnnn10001101", FPU_HANDLER(fldi0),
      false, SH4_GROUP_LS, 1 },

    // FLDI1 Frn
    { "1111nnnn10011101", FPU_HANDLER(fldi1),
      false, SH4_GROUP_LS, 1 },

    // FMOV FRm, FRn
    // 1111nnnnmmmm1100
    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    { "1111nnnnmmmm1100", FPU_HANDLER(fmov_gen),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    { "1111nnnnmmmm1000", FPU_HANDLER(fmovs_ind_gen),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S @(R0, Rm), FRn
    // 1111nnnnmmmm0110
    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    { "1111nnnnmmmm0110", FPU_HANDLER(fmov_binind_r0_gen_fpu),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    { "1111nnnnmmmm1001", FPU_HANDLER(fmov_indgeninc_fpu),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    { "1111nnnnmmmm1010", FPU_HANDLER(fmov_fpu_indgen),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    { "1111nnnnmmmm1011", FPU_HANDLER(fmov_fpu_inddecgen),
      false, SH4_GROUP_LS, 1 },

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    // FMOV DRm, @(R0, Rn)
    // 1111nnnnmmm00111
    { "1111nnnnmmmm0111", FPU_HANDLER(fmov_fpu_binind_r0_gen),
      false, SH4_GROUP_LS, 1 },

    // FLDS FRm, FPUL
    // XXX Should this check the SZ or PR bits of FPSCR ?
    { "1111mmmm00011101", &sh4_inst_binary_flds_fr_fpul, false,
      SH4_GROUP_LS, 1 },

    // FSTS FPUL, FRn
    // XXX Should this check the SZ or PR bits of FPSCR ?
    { "1111nnnn00001101", &sh4_inst_binary_fsts_fpul_fr, false,
      SH4_GROUP_LS, 1 },

    // FABS FRn
    // 1111nnnn01011101
    // FABS DRn
    // 1111nnn001011101
    { "1111nnnn01011101", FPU_HANDLER(fabs_fpu), false,
      SH4_GROUP_LS, 1 },

    // FADD FRm, FRn
    // 1111nnnnmmmm0000
    // FADD DRm, DRn
    // 1111nnn0mmm00000
    { "1111nnnnmmmm0000", FPU_HANDLER(fadd_fpu),
      false, SH4_GROUP_FE, 1 },

    // FCMP/EQ FRm, FRn
    // 1111nnnnmmmm0100
    // FCMP/EQ DRm, DRn
    // 1111nnn0mmm00100
    { "1111nnnnmmmm0100", FPU_HANDLER(fcmpeq_fpu), false, SH4_GROUP_FE, 1 },

    // FCMP/GT FRm, FRn
    // 1111nnnnmmmm0101
    // FCMP/GT DRm, DRn
    // 1111nnn0mmm00101
    { "1111nnnnmmmm0101", FPU_HANDLER(fcmpgt_fpu), false, SH4_GROUP_FE, 1 },

    // FDIV FRm, FRn
    // 1111nnnnmmmm0011
    // FDIV DRm, DRn
    // 1111nnn0mmm00011
    { "1111nnnnmmmm0011", FPU_HANDLER(fdiv_fpu), false, SH4_GROUP_FE, 1 },

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    // FLOAT FPUL, DRn
    // 1111nnn000101101
    { "1111nnnn00101101", FPU_HANDLER(float_fpu), false, SH4_GROUP_FE, 1 },

    // FMAC FR0, FRm, FRn
    // 1111nnnnmmmm1110
    { "1111nnnnmmmm1110", FPU_HANDLER(fmac_fpu), false, SH4_GROUP_FE, 1 },

    // FMUL FRm, FRn
    // 1111nnnnmmmm0010
    // FMUL DRm, DRn
    // 1111nnn0mmm00010
    { "1111nnnnmmmm0010", FPU_HANDLER(fmul_fpu), false, SH4_GROUP_FE, 1 },

    // FNEG FRn
    // 1111nnnn01001101
    // FNEG DRn
    // 1111nnn001001101
    { "1111nnnn01001101", FPU_HANDLER(fneg_fpu), false, SH4_GROUP_LS, 1 },

    // FSQRT FRn
    // 1111nnnn01101101
    // FSQRT DRn
    // 1111nnn001101101
    { "1111nnnn01101101", FPU_HANDLER(fsqrt_fpu), false, SH4_GROUP_FE, 1 },

    // FSUB FRm, FRn
    // 1111nnnnmmmm0001
    // FSUB DRm, DRn
    // 1111nnn0mmm00001
    { "1111nnnnmmmm0001", FPU_HANDLER(fsub_fpu), false, SH4_GROUP_FE, 1 },

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    // FTRC DRm, FPUL
    // 1111mmm000111101
    { "1111mmmm00111101", FPU_HANDLER(ftrc_fpu), false, SH4_GROUP_FE, 1 },

    // FCNVDS DRm, FPUL
    // 1111mmm010111101
    { "1111mmm010111101", FPU_HANDLER(fcnvds_fpu), false, SH4_GROUP_FE, 1 },

    // FCNVSD FPUL, DRn
    // 1111nnn010101101
    { "1111nnn010101101", FPU_HANDLER(fcnvsd_fpu), false, SH4_GROUP_FE, 1 },

    // LDS Rm, FPSCR
    { "0100mmmm01101010", &sh4_inst_binary_lds_gen_fpscr, false,
      SH4_GROUP_CO, 1 },

    // LDS Rm, FPUL
    { "0100mmmm01011010", &sh4_inst_binary_gen_fpul, false,
      SH4_GROUP_LS, 1 },

    // LDS.L @Rm+, FPSCR
    { "0100mmmm01100110", &sh4_inst_binary_ldsl_indgeninc_fpscr, false,
      SH4_GROUP_CO, 1 },

    // LDS.L @Rm+, FPUL
    { "0100mmmm01010110", &sh4_inst_binary_ldsl_indgeninc_fpul, false,
      SH4_GROUP_CO, 1 },

    // STS FPSCR, Rn
    { "0000nnnn01101010", &sh4_inst_binary_sts_fpscr_gen, false,
      SH4_GROUP_CO, 1 },

    // STS FPUL, Rn
    { "0000nnnn01011010", &sh4_inst_binary_sts_fpul_gen, false,
      SH4_GROUP_LS, 1 },

    // STS.L FPSCR, @-Rn
    { "0100nnnn01100010", &sh4_inst_binary_stsl_fpscr_inddecgen, false,
      SH4_GROUP_CO, 1 },

    // STS.L FPUL, @-Rn
    { "0100nnnn01010010", &sh4_inst_binary_stsl_fpul_inddecgen, false,
      SH4_GROUP_CO, 1 },

    // FMOV DRm, XDn
    { "1111nnn1mmm01100", &sh4_inst_binary_fmove_dr_xd, false,
      SH4_GROUP_LS, 1 },

    // FMOV XDm, DRn
    { "1111nnn0mmm11100", &sh4_inst_binary_fmov_xd_dr, false,
      SH4_GROUP_LS, 1 },

    // FMOV XDm, XDn
    { "1111nnn1mmm11100", &sh4_inst_binary_fmov_xd_xd, false,
      SH4_GROUP_LS, 1 },

    // FMOV @Rm, XDn
    { "1111nnn1mmmm1000", &sh4_inst_binary_fmov_indgen_xd, false,
      SH4_GROUP_LS, 1 },

    // FMOV @Rm+, XDn
    { "1111nnn1mmmm1001", &sh4_inst_binary_fmov_indgeninc_xd, false,
      SH4_GROUP_LS, 1 },

    // FMOV @(R0, Rm), XDn
    { "1111nnn1mmmm0110", &sh4_inst_binary_fmov_binind_r0_gen_xd,
      false, SH4_GROUP_LS, 1 },

    // FMOV XDm, @Rn
    { "1111nnnnmmm11010", &sh4_inst_binary_fmov_xd_indgen, false,
      SH4_GROUP_LS, 1 },

    // FMOV XDm, @-Rn
    { "1111nnnnmmm11011", &sh4_inst_binary_fmov_xd_inddecgen, false,
      SH4_GROUP_LS, 1 },

    // FMOV XDm, @(R0, Rn)
    { "1111nnnnmmm10111", &sh4_inst_binary_fmov_xs_binind_r0_gen,
      false, SH4_GROUP_LS, 1 },

    // FIPR FVm, FVn - vector dot product
    { "1111nnmm11101101", &sh4_inst_binary_fipr_fv_fv, false,
      SH4_GROUP_FE, 1 },

    // FTRV XMTRX, FVn - multiple vector by matrix
    { "1111nn0111111101", &sh4_inst_binary_fitrv_mxtrx_fv, false,
      SH4_GROUP_FE, 1 },

    { NULL }
};

static InstOpcode invalid_opcode = {
    "0000000000000000", &sh4_inst_invalid, false,
    (sh4_inst_group_t)0, 0, 0, 0
};

#define SH4_INST_RAISE_ERROR(sh4, error_tp)     \
    do {                                        \
        RAISE_ERROR(error_tp);                  \
    } while (0)

InstOpcode const *sh4_inst_lut[1 << 16];

void sh4_init_inst_lut() {
    unsigned inst;
    for (inst = 0; inst < (1 << 16); inst++)
        sh4_inst_lut[inst] = sh4_decode_inst_slow((inst_t)inst);
}

void sh4_exec_inst(Sh4 *sh4) {
    inst_t inst;
    int exc_pending;

    if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
        // fuck it, i'll commit now and figure what to do here later
        error_set_feature("SH4 CPU exceptions/traps");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }

    sh4_do_exec_inst(sh4, inst, sh4_inst_lut[inst]);
}

// used to initialize the sh4_inst_lut
static InstOpcode const* sh4_decode_inst_slow(inst_t inst) {
    InstOpcode const *op = opcode_list;

    while (op->fmt) {
        if ((op->mask & inst) == op->val) {
            return op;
        }
        op++;
    }

    return &invalid_opcode;
}

void sh4_do_exec_inst(Sh4 *sh4, inst_t inst, InstOpcode const *op) {
    Sh4OpArgs oa;
    oa.inst = inst;

    if (!(sh4->delayed_branch && op->is_branch)) {
        opcode_func_t op_func = op->func;
        bool delayed_branch_tmp = sh4->delayed_branch;
        addr32_t delayed_branch_addr_tmp = sh4->delayed_branch_addr;

        op_func(sh4, oa);

#ifdef ENABLE_DEBUGGER
        if (!sh4->aborted_operation) {
            if (delayed_branch_tmp) {
                sh4->reg[SH4_REG_PC] = delayed_branch_addr_tmp;
                sh4->delayed_branch = false;
            }
        } else {
            sh4->aborted_operation = false;
        }
#else
        if (delayed_branch_tmp) {
            sh4->reg[SH4_REG_PC] = delayed_branch_addr_tmp;
            sh4->delayed_branch = false;
        }
#endif
    } else {
        // raise exception for illegal slot instruction
        sh4_set_exception(sh4, SH4_EXCP_SLOT_ILLEGAL_INST);
    }
}

void sh4_compile_instructions(Sh4 *sh4) {
    InstOpcode *op = opcode_list;

    while (op->fmt) {
        sh4_compile_instruction(sh4, op);
        op++;
    }
}

void sh4_compile_instruction(Sh4 *sh4, struct InstOpcode *op) {
    char const *fmt = op->fmt;
    inst_t mask = 0, val = 0;

    if (strlen(fmt) != 16) {
        error_set_param_name("instruction opcode format");
        error_set_opcode_format(fmt);
        SH4_INST_RAISE_ERROR(sh4, ERROR_INVALID_PARAM);
    }

    for (int idx = 0; idx < 16; idx++) {
        val <<= 1;
        mask <<= 1;

        if (fmt[idx] == '1' || fmt[idx] == '0') {
            mask |= 1;
        }

        if (fmt[idx] == '1')
            val |= 1;
    }

    op->mask = mask;
    op->val = val;
}

// RTS
// 0000000000001011
void sh4_inst_rts(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PR];

    sh4_next_inst(sh4);
}


// CLRMAC
// 0000000000101000
void sh4_inst_clrmac(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_MACL] = sh4->reg[SH4_REG_MACH] = 0;

    sh4_next_inst(sh4);
}


// CLRS
// 0000000001001000
void sh4_inst_clrs(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_S_MASK;

    sh4_next_inst(sh4);
}


// CLRT
// 0000000000001000
void sh4_inst_clrt(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;

    sh4_next_inst(sh4);
}

// LDTLB
// 0000000000111000
void sh4_inst_ldtlb(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("0000000000111000");
    error_set_opcode_name("LDTLB");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// NOP
// 0000000000001001
void sh4_inst_nop(Sh4 *sh4, Sh4OpArgs inst) {
    // do nothing

    sh4_next_inst(sh4);
}

// RTE
// 0000000000101011
void sh4_inst_rte(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch = true;

    /*
     * TODO: this, along with all other delayed branch instructions, may have
     * an inaccuracy involving the way the the PC is set to its new value after
     * the delay slot instead of before it.  The SH4 software manual makes it
     * seem like the PC should be set to its new value before the delay slot.
     * I've been acting under the assumption that the software manual is
     * incorrect because that seems like a really weird way to implement it
     * whether in hardware or in software.  Also, the sh4 software manual adds
     * 2 to the PC at the end of every instruction instead of implying that the
     * CPU does that automatically.  This is significant because if the SH4
     * software manual is interpreted literally, then it should skip the
     * instruction pointed to by PR every time there's a delayed branch since
     * the instruction in the delay slot would move the PC forward
     * uncondtionally.
     *
     * The only way to know for sure is to write a hardware test, and I plan on
     * doing that someday, just not today.
     *
     * ANYWAYS, the reason I bring this up now is that this opcode restores SR
     * from SSR before the delay slot gets executed, which is inconsistent with
     * the way I handle the PC.  This means that either way you interpret this
     * ambiguity, I'm getting something wrong.  This is something that should be
     * cleared up, but right now I don't have the bandwidth to write a hardware
     * test, and I'm hoping that the low-level boot programs in the bios and
     * IP.BIN do not rely on the correct implementation of this idiosyncracy
     * (why would anybody need to read back the SR or the PC right after they
     * just set it?).  Obviously I will get this fixed after the system is
     * booting since any one of 600+ dreamcast games could have something weird
     * that needs this to work right.
     */
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_SPC];
    sh4->reg[SH4_REG_SR] = sh4->reg[SH4_REG_SSR];

    sh4_next_inst(sh4);
}

// SETS
// 0000000001011000
void sh4_inst_sets(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_S_MASK;

    sh4_next_inst(sh4);
}

// SETT
// 0000000000011000
void sh4_inst_sett(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;

    sh4_next_inst(sh4);
}

// SLEEP
// 0000000000011011
void sh4_inst_sleep(Sh4 *sh4, Sh4OpArgs inst) {
    if (sh4->exec_state == SH4_EXEC_STATE_NORM) {
        if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
            sh4_set_exception(sh4, SH4_EXCP_GEN_ILLEGAL_INST);
            return;
        }

        /*
         * TODO: There are supposed to be four standby modes, not just two.
         * I didn't implement Deep Sleep and module standby because I don't
         * think I have everything I need for those yet.
         */
        if (sh4->reg[SH4_REG_STBCR] & SH4_STBCR_STBY_MASK)
            sh4->exec_state = SH4_EXEC_STATE_STANDBY;
        else
            sh4->exec_state = SH4_EXEC_STATE_SLEEP;
    }
}

// FRCHG
// 1111101111111101
void sh4_inst_frchg(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: the software manual says the behavior is undefined if the PR bit
     * is not set in FPSCR.  This means I need to figure out what the acutal
     * hardware does when the PR bit is not set and mimc that here.  For now I
     * just let the operation go through so I can avoid branching.
     */

    sh4->fpu.fpscr ^= SH4_FPSCR_FR_MASK;
    sh4_next_inst(sh4);
}

// FSCHG
// 1111001111111101
void sh4_inst_fschg(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: the software manual says the behavior is undefined if the PR bit
     * is not set in FPSCR.  This means I need to figure out what the acutal
     * hardware does when the PR bit is not set and mimc that here.  For now I
     * just let the operation go through so I can avoid branching.
     */

    sh4->fpu.fpscr ^= SH4_FPSCR_SZ_MASK;
    sh4_next_inst(sh4);
}

// MOVT Rn
// 0000nnnn00101001
void sh4_inst_unary_movt_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) =
        (reg32_t)((sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/PZ Rn
// 0100nnnn00010001
void sh4_inst_unary_cmppz_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t flag = ((int32_t)*sh4_gen_reg(sh4, inst.gen_reg)) >= 0;

    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;

    sh4_next_inst(sh4);
}

// CMP/PL Rn
// 0100nnnn00010101
void sh4_inst_unary_cmppl_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t flag = ((int32_t)*sh4_gen_reg(sh4, inst.gen_reg)) > 0;

    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;

    sh4_next_inst(sh4);
}

// DT Rn
// 0100nnnn00010000
void sh4_inst_unary_dt_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *valp = sh4_gen_reg(sh4, inst.gen_reg);
    (*valp)--;
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (!*valp) << SH4_SR_FLAG_T_SHIFT;

    sh4_next_inst(sh4);
}

// ROTL Rn
// 0100nnnn00000100
void sh4_inst_unary_rotl_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val = (val << 1) | shift_out;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// ROTR Rn
// 0100nnnn00000101
void sh4_inst_unary_rotr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = val & 1;

    val = (val >> 1) | (shift_out << 31);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// ROTCL Rn
// 0100nnnn00100100
void sh4_inst_unary_rotcl_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;
    reg32_t shift_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT;

    val = (val << 1) | shift_in;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// ROTCR Rn
// 0100nnnn00100101
void sh4_inst_unary_rotcr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = val & 1;
    reg32_t shift_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT;

    val = (val >> 1) | (shift_in << 31);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// SHAL Rn
// 0100nnnn00100000
void sh4_inst_unary_shal_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val <<= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// SHAR Rn
// 0100nnnn00100001
void sh4_inst_unary_shar_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    int32_t val = *regp;
    reg32_t shift_out = val & 1;

    val >>= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// SHLL Rn
// 0100nnnn00000000
void sh4_inst_unary_shll_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val <<= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// SHLR Rn
// 0100nnnn00000001
void sh4_inst_unary_shlr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    uint32_t val = *regp;
    reg32_t shift_out = val & 1;

    val >>= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;

    sh4_next_inst(sh4);
}

// SHLL2 Rn
// 0100nnnn00001000
void sh4_inst_unary_shll2_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val <<= 2;
    *regp = val;

    sh4_next_inst(sh4);
}

// SHLR2 Rn
// 0100nnnn00001001
void sh4_inst_unary_shlr2_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val >>= 2;
    *regp = val;

    sh4_next_inst(sh4);
}

// SHLL8 Rn
// 0100nnnn00011000
void sh4_inst_unary_shll8_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val <<= 8;
    *regp = val;

    sh4_next_inst(sh4);
}

// SHLR8 Rn
// 0100nnnn00011001
void sh4_inst_unary_shlr8_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val >>= 8;
    *regp = val;

    sh4_next_inst(sh4);
}

// SHLL16 Rn
// 0100nnnn00101000
void sh4_inst_unary_shll16_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val <<= 16;
    *regp = val;

    sh4_next_inst(sh4);
}

// SHLR16 Rn
// 0100nnnn00101001
void sh4_inst_unary_shlr16_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t val = *regp;

    val >>= 16;
    *regp = val;

    sh4_next_inst(sh4);
}

// BRAF Rn
// 0000nnnn00100011
void sh4_inst_unary_braf_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + *sh4_gen_reg(sh4, inst.gen_reg) + 4;

    sh4_next_inst(sh4);
}

// BSRF Rn
// 0000nnnn00000011
void sh4_inst_unary_bsrf_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch = true;
    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + *sh4_gen_reg(sh4, inst.gen_reg) + 4;

    sh4_next_inst(sh4);
}

// CMP/EQ #imm, R0
// 10001000iiiiiiii
void sh4_inst_binary_cmpeq_imm_r0(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t imm_val = (int32_t)((int8_t)inst.imm8);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= ((*sh4_gen_reg(sh4, 0) == imm_val) << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// AND.B #imm, @(R0, GBR)
// 11001101iiiiiiii
void sh4_inst_binary_andb_imm_r0_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    val &= inst.imm8;

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// AND #imm, R0
// 11001001iiiiiiii
void sh4_inst_binary_and_imm_r0(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, 0) &= inst.imm8;

    sh4_next_inst(sh4);
}

// OR.B #imm, @(R0, GBR)
// 11001111iiiiiiii
void sh4_inst_binary_orb_imm_r0_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    val |= inst.imm8;

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// OR #imm, R0
// 11001011iiiiiiii
void sh4_inst_binary_or_imm_r0(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, 0) |= inst.imm8;

    sh4_next_inst(sh4);
}

// TST #imm, R0
// 11001000iiiiiiii
void sh4_inst_binary_tst_imm_r0(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(inst.imm8 & *sh4_gen_reg(sh4, 0)) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;

    sh4_next_inst(sh4);
}

// TST.B #imm, @(R0, GBR)
// 11001100iiiiiiii
void sh4_inst_binary_tstb_imm_r0_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(inst.imm8 & val) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;

    sh4_next_inst(sh4);
}

// XOR #imm, R0
// 11001010iiiiiiii
void sh4_inst_binary_xor_imm_r0(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, 0) ^= inst.imm8;

    sh4_next_inst(sh4);
}

// XOR.B #imm, @(R0, GBR)
// 11001110iiiiiiii
void sh4_inst_binary_xorb_imm_r0_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    val ^= inst.imm8;

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// BF label
// 10001011dddddddd
void sh4_inst_unary_bf_disp(Sh4 *sh4, Sh4OpArgs inst) {
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK))
        sh4->reg[SH4_REG_PC] += (((int32_t)inst.simm8) << 1) + 4;
    else
        sh4_next_inst(sh4);
}

// BF/S label
// 10001111dddddddd
void sh4_inst_unary_bfs_disp(Sh4 *sh4, Sh4OpArgs inst) {
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)) {
        sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst.simm8) << 1) + 4;
        sh4->delayed_branch = true;
    }

    sh4_next_inst(sh4);
}

// BT label
// 10001001dddddddd
void sh4_inst_unary_bt_disp(Sh4 *sh4, Sh4OpArgs inst) {
    if (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)
        sh4->reg[SH4_REG_PC] += (((int32_t)inst.simm8) << 1) + 4;
    else
        sh4_next_inst(sh4);
}

// BT/S label
// 10001101dddddddd
void sh4_inst_unary_bts_disp(Sh4 *sh4, Sh4OpArgs inst) {
    if (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) {
        sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst.simm8) << 1) + 4;
        sh4->delayed_branch = true;
    }

    sh4_next_inst(sh4);
}

// BRA label
// 1010dddddddddddd
void sh4_inst_unary_bra_disp(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst.simm12) << 1) + 4;

    sh4_next_inst(sh4);
}

// BSR label
// 1011dddddddddddd
void sh4_inst_unary_bsr_disp(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst.simm12) << 1) + 4;
    sh4->delayed_branch = true;

    sh4_next_inst(sh4);
}

// TRAPA #immed
// 11000011iiiiiiii
void sh4_inst_unary_trapa_disp(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_DEBUGGER
    /*
     * Send this to the gdb backend if it's running.  else, fall through to the
     * next case, which would jump to exception handling code if I had bothered
     * to implement it.
     */
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg) {
        debug_on_softbreak(dbg, inst.inst, sh4->reg[SH4_REG_PC]);
        return;
    }
#endif /* ifdef ENABLE_DEBUGGER */

    error_set_feature("opcode implementation");
    error_set_opcode_format("11000011iiiiiiii");
    error_set_opcode_name("TRAPA #immed");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// TAS.B @Rn
// 0100nnnn00011011
void sh4_inst_unary_tasb_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.gen_reg);
    uint8_t val_new, val_old;
    reg32_t mask;

    if (sh4_read_mem(sh4, &val_old, addr, sizeof(val_old)) != 0)
        return;
    val_new = val_old | 0x80;
    if (sh4_write_mem(sh4, &val_new, addr, sizeof(val_new)) != 0)
        return;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    mask = (!val_old) << SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= mask;

    sh4_next_inst(sh4);
}

// OCBI @Rn
// 0000nnnn10100011
void sh4_inst_unary_ocbi_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    /* TODO: if mmu is enabled, this inst can generate exceptions */

    sh4_next_inst(sh4);
}

// OCBP @Rn
// 0000nnnn10100011
void sh4_inst_unary_ocbp_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    /* TODO: if mmu is enabled, this inst can generate exceptions */

    sh4_next_inst(sh4);
}

// PREF @Rn
// 0000nnnn10000011
void sh4_inst_unary_pref_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    unsigned reg_no = inst.gen_reg;
    addr32_t addr = *sh4_gen_reg(sh4, reg_no);

    if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL)
        sh4_sq_pref(sh4, addr);

    sh4_next_inst(sh4);
}

// JMP @Rn
// 0100nnnn00101011
void sh4_inst_unary_jmp_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->delayed_branch_addr = *sh4_gen_reg(sh4, inst.gen_reg);
    sh4->delayed_branch = true;

    sh4_next_inst(sh4);
}

// JSR @Rn
// 0100nnnn00001011
void sh4_inst_unary_jsr_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = *sh4_gen_reg(sh4, inst.gen_reg);
    sh4->delayed_branch = true;

    sh4_next_inst(sh4);
}

// LDC Rm, SR
// 0100mmmm00001110
void sh4_inst_binary_ldc_gen_sr(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_SR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC Rm, GBR
// 0100mmmm00011110
void sh4_inst_binary_ldc_gen_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_GBR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC Rm, VBR
// 0100mmmm00101110
void sh4_inst_binary_ldc_gen_vbr(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_VBR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC Rm, SSR
// 0100mmmm00111110
void sh4_inst_binary_ldc_gen_ssr(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_SSR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC Rm, SPC
// 0100mmmm01001110
void sh4_inst_binary_ldc_gen_spc(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_SPC] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC Rm, DBR
// 0100mmmm11111010
void sh4_inst_binary_ldc_gen_dbr(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_DBR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// STC SR, Rn
// 0000nnnn00000010
void sh4_inst_binary_stc_sr_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_SR];

    sh4_next_inst(sh4);
}

// STC GBR, Rn
// 0000nnnn00010010
void sh4_inst_binary_stc_gbr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_GBR];

    sh4_next_inst(sh4);
}

// STC VBR, Rn
// 0000nnnn00100010
void sh4_inst_binary_stc_vbr_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_VBR];

    sh4_next_inst(sh4);
}

// STC SSR, Rn
// 0000nnnn00110010
void sh4_inst_binary_stc_ssr_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_SSR];

    sh4_next_inst(sh4);
}

// STC SPC, Rn
// 0000nnnn01000010
void sh4_inst_binary_stc_spc_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_SPC];

    sh4_next_inst(sh4);
}

// STC SGR, Rn
// 0000nnnn00111010
void sh4_inst_binary_stc_sgr_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_SGR];

    sh4_next_inst(sh4);
}

// STC DBR, Rn
// 0000nnnn11111010
void sh4_inst_binary_stc_dbr_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_DBR];

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, SR
// 0100mmmm00000111
void sh4_inst_binary_ldcl_indgeninc_sr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_SR] = val;

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, GBR
// 0100mmmm00010111
void sh4_inst_binary_ldcl_indgeninc_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_GBR] = val;

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, VBR
// 0100mmmm00100111
void sh4_inst_binary_ldcl_indgeninc_vbr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_VBR] = val;

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, SSR
// 0100mmmm00110111
void sh4_inst_binary_ldcl_indgenic_ssr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_SSR] = val;

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, SPC
// 0100mmmm01000111
void sh4_inst_binary_ldcl_indgeninc_spc(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_SPC] = val;

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, DBR
// 0100mmmm11110110
void sh4_inst_binary_ldcl_indgeninc_dbr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    sh4->reg[SH4_REG_DBR] = val;

    sh4_next_inst(sh4);
}

// STC.L SR, @-Rn
// 0100nnnn00000011
void sh4_inst_binary_stcl_sr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_SR], addr,
                       sizeof(sh4->reg[SH4_REG_SR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L GBR, @-Rn
// 0100nnnn00010011
void sh4_inst_binary_stcl_gbr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_GBR], addr,
                       sizeof(sh4->reg[SH4_REG_GBR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L VBR, @-Rn
// 0100nnnn00100011
void sh4_inst_binary_stcl_vbr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_VBR], addr,
                       sizeof(sh4->reg[SH4_REG_VBR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L SSR, @-Rn
// 0100nnnn00110011
void sh4_inst_binary_stcl_ssr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_SSR], addr,
                       sizeof(sh4->reg[SH4_REG_SSR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L SPC, @-Rn
// 0100nnnn01000011
void sh4_inst_binary_stcl_spc_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_SPC], addr,
                       sizeof(sh4->reg[SH4_REG_SPC])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L SGR, @-Rn
// 0100nnnn00110010
void sh4_inst_binary_stcl_sgr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_SGR], addr,
                       sizeof(sh4->reg[SH4_REG_SGR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// STC.L DBR, @-Rn
// 0100nnnn11110010
void sh4_inst_binary_stcl_dbr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_DBR], addr,
                       sizeof(sh4->reg[SH4_REG_DBR])) != 0)
        return;

    *regp = addr;

    sh4_next_inst(sh4);
}

// MOV #imm, Rn
// 1110nnnniiiiiiii
void sh4_inst_binary_mov_imm_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = (int32_t)((int8_t)inst.imm8);

    sh4_next_inst(sh4);
}

// ADD #imm, Rn
// 0111nnnniiiiiiii
void sh4_inst_binary_add_imm_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) += (int32_t)((int8_t)(inst.imm8));

    sh4_next_inst(sh4);
}

// MOV.W @(disp, PC), Rn
// 1001nnnndddddddd
void sh4_inst_binary_movw_binind_disp_pc_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + sh4->reg[SH4_REG_PC] + 4;
    int reg_no = inst.gen_reg;
    int16_t mem_in;

    if (sh4_read_mem(sh4, &mem_in, addr, sizeof(mem_in)) != 0)
        return;
    *sh4_gen_reg(sh4, reg_no) = (int32_t)mem_in;

    sh4_next_inst(sh4);
}

// MOV.L @(disp, PC), Rn
// 1101nnnndddddddd
void sh4_inst_binary_movl_binind_disp_pc_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + (sh4->reg[SH4_REG_PC] & ~3) + 4;
    int reg_no = inst.gen_reg;
    int32_t mem_in;

    if (sh4_read_mem(sh4, &mem_in, addr, sizeof(mem_in)) != 0)
        return;
    *sh4_gen_reg(sh4, reg_no) = mem_in;

    sh4_next_inst(sh4);
}

// MOV Rm, Rn
// 0110nnnnmmmm0011
void sh4_inst_binary_movw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) = *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// SWAP.B Rm, Rn
// 0110nnnnmmmm1000
void sh4_inst_binary_swapb_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    unsigned byte0, byte1;
    reg32_t *reg_src = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t val_src = *reg_src;

    byte0 = val_src & 0x00ff;
    byte1 = (val_src & 0xff00) >> 8;

    val_src &= ~0xffff;
    val_src |= byte1 | (byte0 << 8);
    *sh4_gen_reg(sh4, inst.dst_reg) = val_src;

    sh4_next_inst(sh4);
}

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
void sh4_inst_binary_swapw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    unsigned word0, word1;
    uint32_t *reg_src = sh4_gen_reg(sh4, inst.src_reg);
    uint32_t val_src = *reg_src;

    word0 = val_src & 0xffff;
    word1 = val_src >> 16;

    val_src = word1 | (word0 << 16);
    *sh4_gen_reg(sh4, inst.dst_reg) = val_src;

    sh4_next_inst(sh4);
}

// XTRCT Rm, Rn
// 0110nnnnmmmm1101
void sh4_inst_binary_xtrct_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *reg_dst = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *reg_src = sh4_gen_reg(sh4, inst.src_reg);

    *reg_dst = (((*reg_dst) & 0xffff0000) >> 16) |
        (((*reg_src) & 0x0000ffff) << 16);

    sh4_next_inst(sh4);
}

// ADD Rm, Rn
// 0011nnnnmmmm1100
void sh4_inst_binary_add_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) += *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// ADDC Rm, Rn
// 0011nnnnmmmm1110
void sh4_inst_binary_addc_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    // detect carry by doing 64-bit math
    uint64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = sh4_gen_reg(sh4, inst.src_reg);
    dst_reg = sh4_gen_reg(sh4, inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst += in_src + ((sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT);

    unsigned carry_bit = ((in_dst & 0x100000000) >> 32) << SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= carry_bit;

    *dst_reg = in_dst;

    sh4_next_inst(sh4);
}

// ADDV Rm, Rn
// 0011nnnnmmmm1111
void sh4_inst_binary_addv_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    // detect overflow using 64-bit math
    int64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = sh4_gen_reg(sh4, inst.src_reg);
    dst_reg = sh4_gen_reg(sh4, inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst += in_src;

    unsigned overflow_bit = (in_dst != (int32_t)in_dst) << SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= overflow_bit;

    *dst_reg = in_dst;

    sh4_next_inst(sh4);
}

// CMP/EQ Rm, Rn
// 0011nnnnmmmm0000
void sh4_inst_binary_cmpeq_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= ((*sh4_gen_reg(sh4, inst.src_reg) == *sh4_gen_reg(sh4, inst.dst_reg)) <<
               SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/HS Rm, Rn
// 0011nnnnmmmm0010
void sh4_inst_binary_cmphs_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    uint32_t rhs = *sh4_gen_reg(sh4, inst.src_reg);
    sh4->reg[SH4_REG_SR] |= ((lhs >= rhs) << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/GE Rm, Rn
// 0011nnnnmmmm0011
void sh4_inst_binary_cmpge_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    int32_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    int32_t rhs = *sh4_gen_reg(sh4, inst.src_reg);
    sh4->reg[SH4_REG_SR] |= ((lhs >= rhs) << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
void sh4_inst_binary_cmphi_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    uint32_t rhs = *sh4_gen_reg(sh4, inst.src_reg);
    sh4->reg[SH4_REG_SR] |= ((lhs > rhs) << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/GT Rm, Rn
// 0011nnnnmmmm0111
void sh4_inst_binary_cmpgt_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    int32_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    int32_t rhs = *sh4_gen_reg(sh4, inst.src_reg);
    sh4->reg[SH4_REG_SR] |= ((lhs > rhs) << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// CMP/STR Rm, Rn
// 0010nnnnmmmm1100
void sh4_inst_binary_cmpstr_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    uint32_t rhs = *sh4_gen_reg(sh4, inst.src_reg);
    uint32_t flag;

    flag = !!(((lhs & 0x000000ff) == (rhs & 0x000000ff)) ||
              ((lhs & 0x0000ff00) == (rhs & 0x0000ff00)) ||
              ((lhs & 0x00ff0000) == (rhs & 0x00ff0000)) ||
              ((lhs & 0xff000000) == (rhs & 0xff000000)));

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;

    sh4_next_inst(sh4);
}

// DIV1 Rm, Rn
// 0011nnnnmmmm0100
void sh4_inst_binary_div1_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *dividend_p = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *divisor_p  = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t dividend = *dividend_p;
    reg32_t divisor = *divisor_p;

    reg32_t carry_flag = dividend & 0x80000000;
    reg32_t t_flag = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >>
        SH4_SR_FLAG_T_SHIFT;
    reg32_t q_flag = (sh4->reg[SH4_REG_SR] & SH4_SR_Q_MASK) >> SH4_SR_Q_SHIFT;
    reg32_t m_flag = (sh4->reg[SH4_REG_SR] & SH4_SR_M_MASK) >> SH4_SR_M_SHIFT;

    /* shift in the T-val from the last invocation */
    dividend = (dividend << 1) | t_flag;

    /* q_flag is the carry-bit from the previous iteration of DIV1 */
    if (q_flag) {
        if (m_flag) {
            /*
             * the previous iteration's subtraction was less than zero.
             * the divisor is negative, so subtracting it will actually
             * add to the quotient and bring it closer to zero
             */
            reg32_t dividend_orig = dividend;
            dividend -= divisor;
            bool sub_carry = (dividend > dividend_orig);

            if (carry_flag)
                carry_flag = sub_carry;
            else
                carry_flag = !sub_carry;
        } else {
            /*
             * the previous iteration's subtraction yielded a negative result.
             * divisor is positive, so add it to bring the dividend closer to
             * zero
             */
            reg32_t dividend_orig = dividend;
            dividend += divisor;
            bool add_carry = (dividend < dividend_orig);

            if (carry_flag)
                carry_flag = !add_carry;
            else
                carry_flag = add_carry;
        }
    } else {
        if (m_flag) {
            /*
             * the previous iteration yielded a positive result.  The divisor
             * is negative, so adding it will bring the dividend closer to zero
             */
            reg32_t dividend_orig = dividend;
            dividend += divisor;
            bool add_carry = (dividend < dividend_orig);

            if (carry_flag)
                carry_flag = add_carry;
            else
                carry_flag = !add_carry;
        } else {
            /*
             * The previous iteration yielded a positive result.  The divisor is
             * positive, so subtracting it will bring the dividend closer to
             * zero
             */
            reg32_t dividend_orig = dividend;
            dividend -= divisor;
            bool sub_carry = (dividend > dividend_orig);

            if (carry_flag)
                carry_flag = !sub_carry;
            else
                carry_flag = sub_carry;
        }
    }

    q_flag = carry_flag;
    t_flag = (q_flag == m_flag);

    sh4->reg[SH4_REG_SR] &= ~(SH4_SR_Q_MASK | SH4_SR_FLAG_T_MASK);
    sh4->reg[SH4_REG_SR] |= ((t_flag << SH4_SR_FLAG_T_SHIFT) |
                             (q_flag << SH4_SR_Q_SHIFT));

    *dividend_p = dividend;

    sh4_next_inst(sh4);
}

// DIV0S Rm, Rn
// 0010nnnnmmmm0111
void sh4_inst_binary_div0s_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t divisor = *sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t dividend = *sh4_gen_reg(sh4, inst.src_reg);

    reg32_t new_q = (divisor & 0x80000000) >> 31;
    reg32_t new_m = (dividend & 0x80000000) >> 31;
    reg32_t new_t = new_q ^ new_m;

    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_Q_MASK) |
        (new_q << SH4_SR_Q_SHIFT);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_M_MASK) |
        (new_m << SH4_SR_M_SHIFT);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) |
        (new_t << SH4_SR_FLAG_T_SHIFT);

    sh4_next_inst(sh4);
}

// DIV0U
// 0000000000011001
void sh4_inst_noarg_div0u(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &=
        ~(SH4_SR_M_MASK | SH4_SR_Q_MASK | SH4_SR_FLAG_T_MASK);

    sh4_next_inst(sh4);
}

// DMULS.L Rm, Rn
// 0011nnnnmmmm1101
void sh4_inst_binary_dmulsl_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    int64_t val1 = *sh4_gen_reg(sh4, inst.dst_reg);
    int64_t val2 = *sh4_gen_reg(sh4, inst.src_reg);
    int64_t res = (int64_t)val1 * (int64_t)val2;

    sh4->reg[SH4_REG_MACH] = ((uint64_t)res) >> 32;
    sh4->reg[SH4_REG_MACL] = ((uint64_t)res) & 0xffffffff;

    sh4_next_inst(sh4);
}

// DMULU.L Rm, Rn
// 0011nnnnmmmm0101
void sh4_inst_binary_dmulul_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    uint64_t val1 = *sh4_gen_reg(sh4, inst.dst_reg);
    uint64_t val2 = *sh4_gen_reg(sh4, inst.src_reg);
    uint64_t res = (uint64_t)val1 * (uint64_t)val2;

    sh4->reg[SH4_REG_MACH] = res >> 32;
    sh4->reg[SH4_REG_MACL] = res & 0xffffffff;

    sh4_next_inst(sh4);
}

// EXTS.B Rm, Rn
// 0110nnnnmmmm1110
void sh4_inst_binary_extsb_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t src_val = *sh4_gen_reg(sh4, inst.src_reg);
    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)((int8_t)(src_val & 0xff));

    sh4_next_inst(sh4);
}

// EXTS.W Rm, Rnn
// 0110nnnnmmmm1111
void sh4_inst_binary_extsw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t src_val = *sh4_gen_reg(sh4, inst.src_reg);
    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)((int16_t)(src_val & 0xffff));

    sh4_next_inst(sh4);
}

// EXTU.B Rm, Rn
// 0110nnnnmmmm1100
void sh4_inst_binary_extub_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t src_val = *sh4_gen_reg(sh4, inst.src_reg);
    *sh4_gen_reg(sh4, inst.dst_reg) = src_val & 0xff;

    sh4_next_inst(sh4);
}

// EXTU.W Rm, Rn
// 0110nnnnmmmm1101
void sh4_inst_binary_extuw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t src_val = *sh4_gen_reg(sh4, inst.src_reg);
    *sh4_gen_reg(sh4, inst.dst_reg) = src_val & 0xffff;

    sh4_next_inst(sh4);
}

// MUL.L Rm, Rn
// 0000nnnnmmmm0111
void sh4_inst_binary_mull_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_MACL] = *sh4_gen_reg(sh4, inst.dst_reg) * *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// MULS.W Rm, Rn
// 0010nnnnmmmm1111
void sh4_inst_binary_mulsw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    int16_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    int16_t rhs = *sh4_gen_reg(sh4, inst.src_reg);

    sh4->reg[SH4_REG_MACL] = (int32_t)lhs * (int32_t)rhs;

    sh4_next_inst(sh4);
}

// MULU.W Rm, Rn
// 0010nnnnmmmm1110
void sh4_inst_binary_muluw_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    uint16_t lhs = *sh4_gen_reg(sh4, inst.dst_reg);
    uint16_t rhs = *sh4_gen_reg(sh4, inst.src_reg);

    sh4->reg[SH4_REG_MACL] = (uint32_t)lhs * (uint32_t)rhs;

    sh4_next_inst(sh4);
}

// NEG Rm, Rn
// 0110nnnnmmmm1011
void sh4_inst_binary_neg_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) = -*sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// NEGC Rm, Rn
// 0110nnnnmmmm1010
void sh4_inst_binary_negc_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    int64_t val = -(int64_t)((int32_t)(*sh4_gen_reg(sh4, inst.src_reg)));
    unsigned carry_bit = ((val & 0x100000000) >> 32) << SH4_SR_FLAG_T_SHIFT;
    reg32_t flag_t_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >>
        SH4_SR_FLAG_T_SHIFT;

    *sh4_gen_reg(sh4, inst.dst_reg) = val - flag_t_in;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= carry_bit;

    sh4_next_inst(sh4);
}

// SUB Rm, Rn
// 0011nnnnmmmm1000
void sh4_inst_binary_sub_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) -= *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// SUBC Rm, Rn
// 0011nnnnmmmm1010
void sh4_inst_binary_subc_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    // detect carry by doing 64-bit math
    uint64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = sh4_gen_reg(sh4, inst.src_reg);
    dst_reg = sh4_gen_reg(sh4, inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst -= in_src + ((sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT);

    unsigned carry_bit = ((in_dst & 0x100000000) >> 32) << SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= carry_bit;

    *dst_reg = in_dst;

    sh4_next_inst(sh4);
}

// SUBV Rm, Rn
// 0011nnnnmmmm1011
void sh4_inst_binary_subv_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    // detect overflow using 64-bit math
    int64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = sh4_gen_reg(sh4, inst.src_reg);
    dst_reg = sh4_gen_reg(sh4, inst.dst_reg);

    // cast to int32_t instead of int64_t so it gets sign-extended
    // instead of zero-extended.
    in_src = (int32_t)*src_reg;
    in_dst = (int32_t)*dst_reg;

    in_dst -= in_src;

    unsigned overflow_bit = (in_dst > INT32_MAX) || (in_dst < INT32_MIN);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= overflow_bit;

    *dst_reg = in_dst;

    sh4_next_inst(sh4);
}

// AND Rm, Rn
// 0010nnnnmmmm1001
void sh4_inst_binary_and_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) &= *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// NOT Rm, Rn
// 0110nnnnmmmm0111
void sh4_inst_binary_not_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) = ~(*sh4_gen_reg(sh4, inst.src_reg));

    sh4_next_inst(sh4);
}

// OR Rm, Rn
// 0010nnnnmmmm1011
void sh4_inst_binary_or_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) |= *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// TST Rm, Rn
// 0010nnnnmmmm1000
void sh4_inst_binary_tst_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(*sh4_gen_reg(sh4, inst.src_reg) & *sh4_gen_reg(sh4, inst.dst_reg)) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;

    sh4_next_inst(sh4);
}

// XOR Rm, Rn
// 0010nnnnmmmm1010
void sh4_inst_binary_xor_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.dst_reg) ^= *sh4_gen_reg(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// SHAD Rm, Rn
// 0100nnnnmmmm1100
void sh4_inst_binary_shad_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *srcp = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t *dstp = sh4_gen_reg(sh4, inst.dst_reg);
    int32_t src = (int32_t)*srcp;
    int32_t dst = (int32_t)*dstp;

    if (src >= 0) {
        dst <<= src;
    } else {
        dst >>= -src;
    }

    *dstp = dst;

    sh4_next_inst(sh4);
}

// SHLD Rm, Rn
// 0100nnnnmmmm1101
void sh4_inst_binary_shld_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *srcp = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t *dstp = sh4_gen_reg(sh4, inst.dst_reg);
    int32_t src = (int32_t)*srcp;
    uint32_t dst = (int32_t)*dstp;

    if (src >= 0) {
        dst <<= src;
    } else {
        dst >>= -src;
    }

    *dstp = dst;

    sh4_next_inst(sh4);
}

// LDC Rm, Rn_BANK
// 0100mmmm1nnn1110
void sh4_inst_binary_ldc_gen_bank(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_bank_reg(sh4, inst.bank_reg) = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDC.L @Rm+, Rn_BANK
// 0100mmmm1nnn0111
void sh4_inst_binary_ldcl_indgeninc_bank(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    src_reg = sh4_gen_reg(sh4, inst.gen_reg);
    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    *sh4_bank_reg(sh4, inst.bank_reg) = val;

    sh4_next_inst(sh4);
}

// STC Rm_BANK, Rn
// 0000nnnn1mmm0010
void sh4_inst_binary_stc_bank_gen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, inst.gen_reg) = *sh4_bank_reg(sh4, inst.bank_reg);

    sh4_next_inst(sh4);
}

// STC.L Rm_BANK, @-Rn
// 0100nnnn1mmm0011
void sh4_inst_binary_stcl_bank_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    reg32_t src_val = *sh4_bank_reg(sh4, inst.bank_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &src_val, addr, sizeof(src_val)) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// LDS Rm, MACH
// 0100mmmm00001010
void sh4_inst_binary_lds_gen_mach(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_MACH] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// LDS Rm, MACL
// 0100mmmm00011010
void sh4_inst_binary_lds_gen_macl(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_MACL] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// STS MACH, Rn
// 0000nnnn00001010
void sh4_inst_binary_sts_mach_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_MACH];

    sh4_next_inst(sh4);
}

// STS MACL, Rn
// 0000nnnn00011010
void sh4_inst_binary_sts_macl_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_MACL];

    sh4_next_inst(sh4);
}

// LDS Rm, PR
// 0100mmmm00101010
void sh4_inst_binary_lds_gen_pr(Sh4 *sh4, Sh4OpArgs inst) {
    sh4->reg[SH4_REG_PR] = *sh4_gen_reg(sh4, inst.gen_reg);

    sh4_next_inst(sh4);
}

// STS PR, Rn
// 0000nnnn00101010
void sh4_inst_binary_sts_pr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->reg[SH4_REG_PR];

    sh4_next_inst(sh4);
}

// LDS.L @Rm+, MACH
// 0100mmmm00000110
void sh4_inst_binary_ldsl_indgeninc_mach(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);

    if (sh4_read_mem(sh4, &val, *addr_reg, sizeof(val)) != 0)
        return;

    sh4->reg[SH4_REG_MACH] = val;

    *addr_reg += 4;

    sh4_next_inst(sh4);
}

// LDS.L @Rm+, MACL
// 0100mmmm00010110
void sh4_inst_binary_ldsl_indgeninc_macl(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);

    if (sh4_read_mem(sh4, &val, *addr_reg, sizeof(val)) != 0)
        return;

    sh4->reg[SH4_REG_MACL] = val;

    *addr_reg += 4;

    sh4_next_inst(sh4);
}

// STS.L MACH, @-Rn
// 0100mmmm00000010
void sh4_inst_binary_stsl_mach_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_MACH], addr, sizeof(sh4->reg[SH4_REG_MACH])) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// STS.L MACL, @-Rn
// 0100mmmm00010010
void sh4_inst_binary_stsl_macl_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_MACL], addr, sizeof(sh4->reg[SH4_REG_MACL])) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// LDS.L @Rm+, PR
// 0100mmmm00100110
void sh4_inst_binary_ldsl_indgeninc_pr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);

    if (sh4_read_mem(sh4, &val, *addr_reg, sizeof(val)) != 0)
        return;

    sh4->reg[SH4_REG_PR] = val;

    *addr_reg += 4;

    sh4_next_inst(sh4);
}

// STS.L PR, @-Rn
// 0100nnnn00100010
void sh4_inst_binary_stsl_pr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &sh4->reg[SH4_REG_PR], addr, sizeof(sh4->reg[SH4_REG_PR])) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// MOV.B Rm, @Rn
// 0010nnnnmmmm0000
void sh4_inst_binary_movb_gen_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.dst_reg);
    uint8_t mem_val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.W Rm, @Rn
// 0010nnnnmmmm0001
void sh4_inst_binary_movw_gen_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.dst_reg);
    uint16_t mem_val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.L Rm, @Rn
// 0010nnnnmmmm0010
void sh4_inst_binary_movl_gen_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.dst_reg);
    uint32_t mem_val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.B @Rm, Rn
// 0110nnnnmmmm0000
void sh4_inst_binary_movb_indgen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.src_reg);
    int8_t mem_val;

    if (sh4_read_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)mem_val;

    sh4_next_inst(sh4);
}

// MOV.W @Rm, Rn
// 0110nnnnmmmm0001
void sh4_inst_binary_movw_indgen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.src_reg);
    int16_t mem_val;

    if (sh4_read_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)mem_val;

    sh4_next_inst(sh4);
}

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
void sh4_inst_binary_movl_indgen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, inst.src_reg);
    int32_t mem_val;

    if (sh4_read_mem(sh4, &mem_val, addr, sizeof(mem_val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = mem_val;

    sh4_next_inst(sh4);
}

// MOV.B Rm, @-Rn
// 0010nnnnmmmm0100
void sh4_inst_binary_movb_gen_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    int8_t val;

    reg32_t dst_reg_val = (*dst_reg) - 1;
    val = *src_reg;

    if (sh4_write_mem(sh4, &val, dst_reg_val, sizeof(val)) != 0)
        return;

    (*dst_reg) = dst_reg_val;

    sh4_next_inst(sh4);
}

// MOV.W Rm, @-Rn
// 0010nnnnmmmm0101
void sh4_inst_binary_movw_gen_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    int16_t val;

    reg32_t dst_reg_val = *dst_reg;
    dst_reg_val -= 2;
    val = *src_reg;

    if (sh4_write_mem(sh4, &val, dst_reg_val, sizeof(val)) != 0)
        return;

    *dst_reg = dst_reg_val;

    sh4_next_inst(sh4);
}

// MOV.L Rm, @-Rn
// 0010nnnnmmmm0110
void sh4_inst_binary_movl_gen_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    int32_t val;

    reg32_t dst_reg_val = *dst_reg;
    dst_reg_val -= 4;
    val = *src_reg;

    if (sh4_write_mem(sh4, &val, dst_reg_val, sizeof(val)) != 0)
        return;

    *dst_reg = dst_reg_val;

    sh4_next_inst(sh4);
}

// MOV.B @Rm+, Rn
// 0110nnnnmmmm0100
void sh4_inst_binary_movb_indgeninc_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    int8_t val;

    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0)
        return;

    *dst_reg = (int32_t)val;

    (*src_reg)++;

    sh4_next_inst(sh4);
}

// MOV.W @Rm+, Rn
// 0110nnnnmmmm0101
void sh4_inst_binary_movw_indgeninc_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    int16_t val;

    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0)
        return;

    *dst_reg = (int32_t)val;

    (*src_reg) += 2;

    sh4_next_inst(sh4);
}

// MOV.L @Rm+, Rn
// 0110nnnnmmmm0110
void sh4_inst_binary_movl_indgeninc_gen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *src_reg = sh4_gen_reg(sh4, inst.src_reg);
    reg32_t *dst_reg = sh4_gen_reg(sh4, inst.dst_reg);
    int32_t val;

    if (sh4_read_mem(sh4, &val, *src_reg, sizeof(val)) != 0)
        return;

    *dst_reg = (int32_t)val;

    (*src_reg) += 4;

    sh4_next_inst(sh4);
}

// MAC.L @Rm+, @Rn+
// 0000nnnnmmmm1111
void sh4_inst_binary_macl_indgeninc_indgeninc(Sh4 *sh4, Sh4OpArgs inst) {
    static const int64_t MAX48 = 0x7fffffffffff;
    static const int64_t MIN48 = 0xffff800000000000;
    reg32_t *dst_addrp = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *src_addrp = sh4_gen_reg(sh4, inst.src_reg);

    reg32_t lhs, rhs;
    if (sh4_read_mem(sh4, &lhs, *dst_addrp, sizeof(lhs)) != 0 ||
        sh4_read_mem(sh4, &rhs, *src_addrp, sizeof(rhs)) != 0)
        return;

    int64_t product = (int64_t)((int32_t)lhs) * (int64_t)((int32_t)rhs);
    int64_t sum;

    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_S_MASK)) {
        sum = product +
            (int64_t)(((uint64_t)sh4->reg[SH4_REG_MACL]) | (((uint64_t)sh4->reg[SH4_REG_MACH]) << 32));
    } else {
        // 48-bit saturation addition
        int64_t mac = (int64_t)(((uint64_t)sh4->reg[SH4_REG_MACL]) | (((uint64_t)sh4->reg[SH4_REG_MACH]) << 32));
        sum = mac + product;
        if (sum < 0) {
            if (mac >= 0 && product >= 0) {
                // overflow positive to negative
                sum = MAX48;
            } else if (sum < MIN48) {
                sum = MIN48;
            }
        } else {
            if (mac < 0 && product < 0) {
                // overflow negative to positive
                sum = MIN48;
            } else if (sum > MAX48) {
                sum = MAX48;
            }
        }
    }

    sh4->reg[SH4_REG_MACL] = ((uint64_t)sum) & 0xffffffff;
    sh4->reg[SH4_REG_MACH] = ((uint64_t)sum) >> 32;

    (*dst_addrp) += 4;
    (*src_addrp) += 4;

    sh4_next_inst(sh4);
}

// MAC.W @Rm+, @Rn+
// 0100nnnnmmmm1111
void sh4_inst_binary_macw_indgeninc_indgeninc(Sh4 *sh4, Sh4OpArgs inst) {
    static const int32_t MAX32 = 0x7fffffff;
    static const int32_t MIN32 = 0x80000000;
    reg32_t *dst_addrp = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t *src_addrp = sh4_gen_reg(sh4, inst.src_reg);

    int16_t lhs, rhs;
    if (sh4_read_mem(sh4, &lhs, *dst_addrp, sizeof(lhs)) != 0 ||
        sh4_read_mem(sh4, &rhs, *src_addrp, sizeof(rhs)) != 0)
        return;

    int64_t result = (int64_t)lhs * (int64_t)rhs;

    if (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_S_MASK) {
        /*
         * handle overflow
         *
         * There's a fairly ridiculous inconsistency in the sh4 documentation
         * regarding the mach register here.
         *
         * From page 327 of SH-4 Software Manual (Rev 6.00):
         *    "In a saturation operation, only the MACL register is valid"
         *    ...
         *    "If overflow occurs, the LSB of the MACH register is set to 1."
         *
         * Obviously both of these statements can't be true.
         * The current implementation interprets this literally by OR'ing 1
         * into mach when there is an overflow, and doing nothing when there is
         * not an overflow.  This is because I prefer not to change things when
         * I don't have to, although in this case it may not be the correct
         * behavior since setting the LSB to 1 is obviously useless unless you
         * are tracking the initial value.  Someday in the future I will need to
         * test this out on real hardware to see how this opcode effects the
         * mach register when the saturation bit is set in the SR register.
         */
        result += (int64_t)sh4->reg[SH4_REG_MACL];

        if (result < MIN32) {
            result = MIN32;
            sh4->reg[SH4_REG_MACH] |= 1;
        } else if (result > MAX32) {
            result = MAX32;
            sh4->reg[SH4_REG_MACH] |= 1;
        }

        sh4->reg[SH4_REG_MACL] = result;
    } else {
        // saturation arithmetic is disabled
        result += (int64_t)(((uint64_t)sh4->reg[SH4_REG_MACL]) | (((uint64_t)sh4->reg[SH4_REG_MACH]) << 32));
        sh4->reg[SH4_REG_MACL] = ((uint64_t)result) & 0xffffffff;
        sh4->reg[SH4_REG_MACH] = ((uint64_t)result) >> 32;
    }

    (*dst_addrp) += 2;
    (*src_addrp) += 2;

    sh4_next_inst(sh4);
}

// MOV.B R0, @(disp, Rn)
// 10000000nnnndddd
void sh4_inst_binary_movb_r0_binind_disp_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = inst.imm4 + *sh4_gen_reg(sh4, inst.base_reg_src);
    int8_t val = *sh4_gen_reg(sh4, 0);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.W R0, @(disp, Rn)
// 10000001nnnndddd
void sh4_inst_binary_movw_r0_binind_disp_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm4 << 1) + *sh4_gen_reg(sh4, inst.base_reg_src);
    int16_t val = *sh4_gen_reg(sh4, 0);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.L Rm, @(disp, Rn)
// 0001nnnnmmmmdddd
void sh4_inst_binary_movl_gen_binind_disp_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm4 << 2) + *sh4_gen_reg(sh4, inst.base_reg_dst);
    int32_t val = *sh4_gen_reg(sh4, inst.base_reg_src);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.B @(disp, Rm), R0
// 10000100mmmmdddd
void sh4_inst_binary_movb_binind_disp_gen_r0(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = inst.imm4 + *sh4_gen_reg(sh4, inst.base_reg_src);
    int8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, 0) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.W @(disp, Rm), R0
// 10000101mmmmdddd
void sh4_inst_binary_movw_binind_disp_gen_r0(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm4 << 1) + *sh4_gen_reg(sh4, inst.base_reg_src);
    int16_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, 0) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
void sh4_inst_binary_movl_binind_disp_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm4 << 2) + *sh4_gen_reg(sh4, inst.base_reg_src);
    int32_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.base_reg_dst) = val;

    sh4_next_inst(sh4);
}

// MOV.B Rm, @(R0, Rn)
// 0000nnnnmmmm0100
void sh4_inst_binary_movb_gen_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.dst_reg);
    uint8_t val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.W Rm, @(R0, Rn)
// 0000nnnnmmmm0101
void sh4_inst_binary_movw_gen_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.dst_reg);
    uint16_t val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.L Rm, @(R0, Rn)
// 0000nnnnmmmm0110
void sh4_inst_binary_movl_gen_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.dst_reg);
    uint32_t val = *sh4_gen_reg(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.B @(R0, Rm), Rn
// 0000nnnnmmmm1100
void sh4_inst_binary_movb_binind_r0_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.src_reg);
    int8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.W @(R0, Rm), Rn
// 0000nnnnmmmm1101
void sh4_inst_binary_movw_binind_r0_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.src_reg);
    int16_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.L @(R0, Rm), Rn
// 0000nnnnmmmm1110
void sh4_inst_binary_movl_binind_r0_gen_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.src_reg);
    int32_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, inst.dst_reg) = val;

    sh4_next_inst(sh4);
}

// MOV.B R0, @(disp, GBR)
// 11000000dddddddd
void sh4_inst_binary_movb_r0_binind_disp_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = inst.imm8 + sh4->reg[SH4_REG_GBR];
    int8_t val = *sh4_gen_reg(sh4, 0);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.W R0, @(disp, GBR)
// 11000001dddddddd
void sh4_inst_binary_movw_r0_binind_disp_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + sh4->reg[SH4_REG_GBR];
    int16_t val = *sh4_gen_reg(sh4, 0);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.L R0, @(disp, GBR)
// 11000010dddddddd
void sh4_inst_binary_movl_r0_binind_disp_gbr(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + sh4->reg[SH4_REG_GBR];
    int32_t val = *sh4_gen_reg(sh4, 0);

    if (sh4_write_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// MOV.B @(disp, GBR), R0
// 11000100dddddddd
void sh4_inst_binary_movb_binind_disp_gbr_r0(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = inst.imm8 + sh4->reg[SH4_REG_GBR];
    int8_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, 0) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.W @(disp, GBR), R0
// 11000101dddddddd
void sh4_inst_binary_movw_binind_disp_gbr_r0(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + sh4->reg[SH4_REG_GBR];
    int16_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, 0) = (int32_t)val;

    sh4_next_inst(sh4);
}

// MOV.L @(disp, GBR), R0
// 11000110dddddddd
void sh4_inst_binary_movl_binind_disp_gbr_r0(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + sh4->reg[SH4_REG_GBR];
    int32_t val;

    if (sh4_read_mem(sh4, &val, addr, sizeof(val)) != 0)
        return;

    *sh4_gen_reg(sh4, 0) = val;

    sh4_next_inst(sh4);
}

// MOVA @(disp, PC), R0
// 11000111dddddddd
void sh4_inst_binary_mova_binind_disp_pc_r0(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * The assembly for this one is a bit of a misnomer.
     * even though it has the @ indirection symbol around (disp, PC), it
     * actually just loads that address into R0 instead of the value at that
     * address.  It is roughly analagous to the x86 architectures lea family of
     * opcodes.
     */
    *sh4_gen_reg(sh4, 0) = (inst.imm8 << 2) + (sh4->reg[SH4_REG_PC] & ~3) + 4;

    sh4_next_inst(sh4);
}


/*
 * XXX There are a few different ways the MOVCA.L operator can effect the
 * processor's state upon a failure; (such as by allocating a new cache
 * line and subsequently failing to write).  They *seem* rather minor, but IDK.
 *
 * further research may be warranted to figure out how much state needs to be
 * rolled back here (if at all) when an SH4 CPU exception is raised.
 */
// MOVCA.L R0, @Rn
// 0000nnnn11000011
void sh4_inst_binary_movcal_r0_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t src_val = *sh4_gen_reg(sh4, 0);
    addr32_t vaddr = *sh4_gen_reg(sh4, inst.dst_reg);

    if (sh4_write_mem(sh4, &src_val, vaddr, sizeof(src_val)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FLDI0 FRn
// 1111nnnn10001101
void sh4_inst_unary_fldi0_fr(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_fpu_fr(sh4, inst.fr_reg) = 0.0f;

    sh4_next_inst(sh4);
}

// FLDI1 Frn
// 1111nnnn10011101
void sh4_inst_unary_fldi1_fr(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_fpu_fr(sh4, inst.fr_reg) = 1.0f;

    sh4_next_inst(sh4);
}

// FMOV FRm, FRn
// 1111nnnnmmmm1100
void sh4_inst_binary_fmov_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_fpu_fr(sh4, inst.dst_reg) = *sh4_fpu_fr(sh4, inst.src_reg);

    sh4_next_inst(sh4);
}

// FMOV.S @Rm, FRn
// 1111nnnnmmmm1000
void sh4_inst_binary_fmovs_indgen_fr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, inst.src_reg);
    float *dst_ptr = sh4_fpu_fr(sh4, inst.dst_reg);

    if (sh4_read_mem(sh4, dst_ptr, addr, sizeof(*dst_ptr)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV.S @(R0,Rm), FRn
// 1111nnnnmmmm0110
void sh4_inst_binary_fmovs_binind_r0_gen_fr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, 0) + * sh4_gen_reg(sh4, inst.src_reg);
    float *dst_ptr = sh4_fpu_fr(sh4, inst.dst_reg);

    if (sh4_read_mem(sh4, dst_ptr, addr, sizeof(*dst_ptr)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV.S @Rm+, FRn
// 1111nnnnmmmm1001
void sh4_inst_binary_fmovs_indgeninc_fr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_p = sh4_gen_reg(sh4, inst.src_reg);
    float *dst_ptr = sh4_fpu_fr(sh4, inst.dst_reg);

    if (sh4_read_mem(sh4, dst_ptr, *addr_p, sizeof(*dst_ptr)) != 0)
        return;

    *addr_p += 4;
    sh4_next_inst(sh4);
}

// FMOV.S FRm, @Rn
// 1111nnnnmmmm1010
void sh4_inst_binary_fmovs_fr_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, inst.dst_reg);
    float *src_p = sh4_fpu_fr(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV.S FRm, @-Rn
// 1111nnnnmmmm1011
void sh4_inst_binary_fmovs_fr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_p = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t addr = *addr_p - 4;
    float *src_p = sh4_fpu_fr(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    *addr_p = addr;
    sh4_next_inst(sh4);
}

// FMOV.S FRm, @(R0, Rn)
// 1111nnnnmmmm0111
void sh4_inst_binary_fmovs_fr_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.dst_reg);
    float *src_p = sh4_fpu_fr(sh4, inst.src_reg);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV DRm, DRn
// 1111nnn0mmm01100
void sh4_inst_binary_fmov_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_fpu_dr(sh4, inst.dr_dst) = *sh4_fpu_dr(sh4, inst.dr_src);

    sh4_next_inst(sh4);
}

// FMOV @Rm, DRn
// 1111nnn0mmmm1000
void sh4_inst_binary_fmov_indgen_dr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, inst.src_reg);
    double *dst_ptr = sh4_fpu_dr(sh4, inst.dr_dst);

    if (sh4_read_mem(sh4, dst_ptr, addr, sizeof(*dst_ptr)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV @(R0, Rm), DRn
// 1111nnn0mmmm0110
void sh4_inst_binary_fmov_binind_r0_gen_dr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, 0) + * sh4_gen_reg(sh4, inst.src_reg);
    double *dst_ptr = sh4_fpu_dr(sh4, inst.dr_dst);

    if (sh4_read_mem(sh4, dst_ptr, addr, sizeof(*dst_ptr)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV @Rm+, DRn
// 1111nnn0mmmm1001
void sh4_inst_binary_fmov_indgeninc_dr(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_p = sh4_gen_reg(sh4, inst.src_reg);
    double *dst_ptr = sh4_fpu_dr(sh4, inst.dr_dst);

    if (sh4_read_mem(sh4, dst_ptr, *addr_p, sizeof(*dst_ptr)) != 0)
        return;

    *addr_p += 8;
    sh4_next_inst(sh4);
}

// FMOV DRm, @Rn
// 1111nnnnmmm01010
void sh4_inst_binary_fmov_dr_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t addr = *sh4_gen_reg(sh4, inst.dst_reg);
    double *src_p = sh4_fpu_dr(sh4, inst.dr_src);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FMOV DRm, @-Rn
// 1111nnnnmmm01011
void sh4_inst_binary_fmov_dr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_p = sh4_gen_reg(sh4, inst.dst_reg);
    reg32_t addr = *addr_p - 8;
    double *src_p = sh4_fpu_dr(sh4, inst.dr_src);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    *addr_p = addr;
    sh4_next_inst(sh4);
}

// FMOV DRm, @(R0, Rn)
// 1111nnnnmmm00111
void sh4_inst_binary_fmov_dr_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, inst.dst_reg);
    double *src_p = sh4_fpu_dr(sh4, inst.dr_src);

    if (sh4_write_mem(sh4, src_p, addr, sizeof(*src_p)) != 0)
        return;

    sh4_next_inst(sh4);
}

// FLDS FRm, FPUL
// 1111mmmm00011101
void sh4_inst_binary_flds_fr_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    float *src_reg = sh4_fpu_fr(sh4, inst.gen_reg);

    memcpy(&sh4->fpu.fpul, src_reg, sizeof(sh4->fpu.fpul));

    sh4_next_inst(sh4);
}

// FSTS FPUL, FRn
// 1111nnnn00001101
void sh4_inst_binary_fsts_fpul_fr(Sh4 *sh4, Sh4OpArgs inst) {
    float *dst_reg = sh4_fpu_fr(sh4, inst.gen_reg);

    memcpy(dst_reg, &sh4->fpu.fpul, sizeof(*dst_reg));

    sh4_next_inst(sh4);
}

// FABS FRn
// 1111nnnn01011101
void sh4_inst_unary_fabs_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnn01011101");
    error_set_opcode_name("FABS FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FADD FRm, FRn
// 1111nnnnmmmm0000
void sh4_inst_binary_fadd_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0000");
    error_set_opcode_name("FADD FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FCMP/EQ FRm, FRn
// 1111nnnnmmmm0100
void sh4_inst_binary_fcmpeq_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0100");
    error_set_opcode_name("FCMP/EQ FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FCMP/GT FRm, FRn
// 1111nnnnmmmm0101
void sh4_inst_binary_fcmpgt_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0101");
    error_set_opcode_name("FCMP/GT FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FDIV FRm, FRn
// 1111nnnnmmmm0011
void sh4_inst_binary_fdiv_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0011");
    error_set_opcode_name("FDIV FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FLOAT FPUL, FRn
// 1111nnnn00101101
void sh4_inst_binary_float_fpul_fr(Sh4 *sh4, Sh4OpArgs inst) {
    float *dst_reg = sh4_fpu_fr(sh4, inst.gen_reg);

    *dst_reg = (float)sh4->fpu.fpul;

    sh4_next_inst(sh4);
}

// FMAC FR0, FRm, FRn
// 1111nnnnmmmm1110
void sh4_inst_trinary_fmac_fr0_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm1110");
    error_set_opcode_name("FMAC FR0, FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMUL FRm, FRn
// 1111nnnnmmmm0010
void sh4_inst_binary_fmul_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0010");
    error_set_opcode_name("FMUL FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FNEG FRn
// 1111nnnn01001101
void sh4_inst_unary_fneg_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnn01001101");
    error_set_opcode_name("FNEG FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FSQRT FRn
// 1111nnnn01101101
void sh4_inst_unary_fsqrt_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnn01101101");
    error_set_opcode_name("FSQRT FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FSUB FRm, FRn
// 1111nnnnmmmm0001
void sh4_inst_binary_fsub_fr_fr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmmm0001");
    error_set_opcode_name("FSUB FRm, FRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FTRC FRm, FPUL
// 1111mmmm00111101
void sh4_inst_binary_ftrc_fr_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    float const *val_in_p = sh4_fpu_fr(sh4, inst.gen_reg);
    float val = *val_in_p;
    uint32_t val_int;

    sh4_next_inst(sh4);
    sh4->fpu.fpscr &= ~SH4_FPSCR_CAUSE_MASK;

    int round_mode = fegetround();
    fesetround(FE_TOWARDZERO);

    val_int = val;
    memcpy(&sh4->fpu.fpul, &val_int, sizeof(sh4->fpu.fpul));

    fesetround(round_mode);
}

// FABS DRn
// 1111nnn001011101
void sh4_inst_unary_fabs_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001011101");
    error_set_opcode_name("FABS DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FADD DRm, DRn
// 1111nnn0mmm00000
void sh4_inst_binary_fadd_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00000");
    error_set_opcode_name("FADD DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FCMP/EQ DRm, DRn
// 1111nnn0mmm00100
void sh4_inst_binary_fcmpeq_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00100");
    error_set_opcode_name("FCMP/EQ DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FCMP/GT DRm, DRn
// 1111nnn0mmm00101
void sh4_inst_binary_fcmpgt_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00101");
    error_set_opcode_name("FCMP/GT DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FDIV DRm, DRn
// 1111nnn0mmm00011
void sh4_inst_binary_fdiv_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00011");
    error_set_opcode_name("FDIV DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FCNVDS DRm, FPUL
// 1111mmm010111101
void sh4_inst_binary_fcnvds_dr_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    sh4_next_inst(sh4);
    sh4->fpu.fpscr &= ~SH4_FPSCR_CAUSE_MASK;

    double in_val = *sh4_fpu_dr(sh4, inst.dr_reg);
    float out_val = in_val;

    memcpy(&sh4->fpu.fpul, &out_val, sizeof(sh4->fpu.fpul));
}

// FCNVSD FPUL, DRn
// 1111nnn010101101
void sh4_inst_binary_fcnvsd_fpul_dr(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    sh4_next_inst(sh4);
    sh4->fpu.fpscr &= ~SH4_FPSCR_CAUSE_MASK;

    float in_val;
    memcpy(&in_val, &sh4->fpu.fpul, sizeof(in_val));
    double out_val = in_val;

    *sh4_fpu_dr(sh4, inst.dr_reg) = out_val;
}

// FLOAT FPUL, DRn
// 1111nnn000101101
void sh4_inst_binary_float_fpul_dr(Sh4 *sh4, Sh4OpArgs inst) {
    double *dst_reg = sh4_fpu_dr(sh4, inst.dr_reg);

    *dst_reg = (double)sh4->fpu.fpul;

    sh4_next_inst(sh4);
}

// FMUL DRm, DRn
// 1111nnn0mmm00010
void sh4_inst_binary_fmul_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00010");
    error_set_opcode_name("FMUL DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FNEG DRn
// 1111nnn001001101
void sh4_inst_unary_fneg_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001001101");
    error_set_opcode_name("FNEG DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FSQRT DRn
// 1111nnn001101101
void sh4_inst_unary_fsqrt_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001101101");
    error_set_opcode_name("FSQRT DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FSUB DRm, DRn
// 1111nnn0mmm00001
void sh4_inst_binary_fsub_dr_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm00001");
    error_set_opcode_name("FSUB DRm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FTRC DRm, FPUL
// 1111mmm000111101
void sh4_inst_binary_ftrc_dr_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    double val_in = *sh4_fpu_dr(sh4, inst.dr_src);
    uint32_t val_int;

    sh4_next_inst(sh4);
    sh4->fpu.fpscr &= ~SH4_FPSCR_CAUSE_MASK;

    int round_mode = fegetround();
    fesetround(FE_TOWARDZERO);

    val_int = val_in;
    memcpy(&sh4->fpu.fpul, &val_int, sizeof(sh4->fpu.fpul));

    fesetround(round_mode);
}

// LDS Rm, FPSCR
// 0100mmmm01101010
void sh4_inst_binary_lds_gen_fpscr(Sh4 *sh4, Sh4OpArgs inst) {
    sh4_set_fpscr(sh4, *sh4_gen_reg(sh4, inst.gen_reg));

    sh4_next_inst(sh4);
}

// LDS Rm, FPUL
// 0100mmmm01011010
void sh4_inst_binary_gen_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    memcpy(&sh4->fpu.fpul, sh4_gen_reg(sh4, inst.gen_reg), sizeof(sh4->fpu.fpul));

    sh4_next_inst(sh4);
}

// LDS.L @Rm+, FPSCR
// 0100mmmm01100110
void sh4_inst_binary_ldsl_indgeninc_fpscr(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);

    if (sh4_read_mem(sh4, &val, *addr_reg, sizeof(val)) != 0)
        return;

    sh4_set_fpscr(sh4, val);

    *addr_reg += 4;

    sh4_next_inst(sh4);
}

// LDS.L @Rm+, FPUL
// 0100mmmm01010110
void sh4_inst_binary_ldsl_indgeninc_fpul(Sh4 *sh4, Sh4OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);

    if (sh4_read_mem(sh4, &val, *addr_reg, sizeof(val)) != 0)
        return;

    memcpy(&sh4->fpu.fpul, &val, sizeof(sh4->fpu.fpul));

    *addr_reg += 4;

    sh4_next_inst(sh4);
}

// STS FPSCR, Rn
// 0000nnnn01101010
void sh4_inst_binary_sts_fpscr_gen(Sh4 *sh4, Sh4OpArgs inst) {
    *sh4_gen_reg(sh4, inst.gen_reg) = sh4->fpu.fpscr;

    sh4_next_inst(sh4);
}

// STS FPUL, Rn
// 0000nnnn01011010
void sh4_inst_binary_sts_fpul_gen(Sh4 *sh4, Sh4OpArgs inst) {
    memcpy(sh4_gen_reg(sh4, inst.gen_reg), &sh4->fpu.fpul, sizeof(sh4->fpu.fpul));

    sh4_next_inst(sh4);
}

// STS.L FPSCR, @-Rn
// 0100nnnn01100010
void sh4_inst_binary_stsl_fpscr_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &sh4->fpu.fpscr, addr, sizeof(sh4->fpu.fpscr)) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// STS.L FPUL, @-Rn
// 0100nnnn01010010
void sh4_inst_binary_stsl_fpul_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    reg32_t *addr_reg = sh4_gen_reg(sh4, inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (sh4_write_mem(sh4, &sh4->fpu.fpul, addr, sizeof(sh4->fpu.fpul)) != 0)
        return;

    *addr_reg = addr;

    sh4_next_inst(sh4);
}

// FMOV DRm, XDn
// 1111nnn1mmm01100
void sh4_inst_binary_fmove_dr_xd(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmm01100");
    error_set_opcode_name("FMOV DRm, XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV XDm, DRn
// 1111nnn0mmm11100
void sh4_inst_binary_fmov_xd_dr(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn0mmm11100");
    error_set_opcode_name("FMOV XDm, DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV XDm, XDn
// 1111nnn1mmm11100
void sh4_inst_binary_fmov_xd_xd(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmm11100");
    error_set_opcode_name("FMOV XDm, XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV @Rm, XDn
// 1111nnn1mmmm1000
void sh4_inst_binary_fmov_indgen_xd(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmmm1000");
    error_set_opcode_name("FMOV @Rm, XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV @Rm+, XDn
// 1111nnn1mmmm1001
void sh4_inst_binary_fmov_indgeninc_xd(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmmm1001");
    error_set_opcode_name("FMOV @Rm+, XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV @(R0, Rn), XDn
// 1111nnn1mmmm0110
void sh4_inst_binary_fmov_binind_r0_gen_xd(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmmm0110");
    error_set_opcode_name("FMOV @(R0, Rn), XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV XDm, @Rn
// 1111nnnnmmm11010
void sh4_inst_binary_fmov_xd_indgen(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmm11010");
    error_set_opcode_name("FMOV XDm, @Rn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV XDm, @-Rn
// 1111nnnnmmm11011
void sh4_inst_binary_fmov_xd_inddecgen(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmm11011");
    error_set_opcode_name("FMOV XDm, @-Rn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FMOV XDm, @(R0, Rn)
// 1111nnnnmmm10111
void sh4_inst_binary_fmov_xs_binind_r0_gen(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmm10111");
    error_set_opcode_name("FMOV XDm, @(R0, Rn)");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FIPR FVm, FVn - vector dot product
// 1111nnmm11101101
void sh4_inst_binary_fipr_fv_fv(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnmm11101101");
    error_set_opcode_name("FIPR FVm, FVn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

// FTRV XMTRX, FVn - multiple vector by matrix
// 1111nn0111111101
void sh4_inst_binary_fitrv_mxtrx_fv(Sh4 *sh4, Sh4OpArgs inst) {
    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nn0111111101");
    error_set_opcode_name("FTRV MXTRX, FVn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

void sh4_inst_invalid(Sh4 *sh4, Sh4OpArgs inst) {
#ifdef DBG_EXIT_ON_UNDEFINED_OPCODE

    error_set_feature("SH4 CPU exception for unrecognized opcode");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
#else
#ifdef ENABLE_DEBUGGER
    /*
     * Send this to the gdb backend if it's running.  else, fall through to the
     * next case, where we raise an sh4 CPU exception.
     */
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg) {
        debug_on_softbreak(dbg, inst.inst, sh4->reg[SH4_REG_PC]);
        return;
    }

#endif /* ifdef ENABLE_DEBUGGER */
    /*
     * raise an sh4 CPU exception, this case is
     * what's actually supposed to happen on real hardware.
     */

    /*
     * Slot Illegal Instruction Exception supersedes General Illegal
     * Instruction Exception.
     */
    if (sh4->delayed_branch)
        sh4_set_exception(sh4, SH4_EXCP_SLOT_ILLEGAL_INST);
    else
        sh4_set_exception(sh4, SH4_EXCP_GEN_ILLEGAL_INST);
#endif /* ifdef DBG_EXIT_ON_UNDEFINED_OPCODE */
}

// TODO: what is the proper behavior when the PR bit is set?
// FLDI0 FRn
// 1111nnnn10001101
DEF_FPU_HANDLER(fldi0, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fldi0_fr, sh4_inst_invalid)

// TODO: what is the proper behavior when the PR bit is set?
// FLDI1 Frn
// 1111nnnn10011101
DEF_FPU_HANDLER(fldi1, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fldi1_fr, sh4_inst_unary_fldi1_fr)

// these handlers depend on the SZ bit
// FMOV FRm, FRn
// 1111nnnnmmmm1100
// FMOV DRm, DRn
// 1111nnn0mmm01100
DEF_FPU_HANDLER(fmov_gen, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmov_fr_fr, sh4_inst_binary_fmov_dr_dr)

// FMOV.S @Rm, FRn
// 1111nnnnmmmm1000
// FMOV @Rm, DRn
// 1111nnn0mmmm1000
DEF_FPU_HANDLER(fmovs_ind_gen, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_indgen_fr,
                sh4_inst_binary_fmov_indgen_dr)

// FMOV.S @(R0, Rm), FRn
// 1111nnnnmmmm0110
// FMOV @(R0, Rm), DRn
// 1111nnn0mmmm0110
DEF_FPU_HANDLER(fmov_binind_r0_gen_fpu, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_binind_r0_gen_fr,
                sh4_inst_binary_fmov_binind_r0_gen_dr)

// FMOV.S @Rm+, FRn
// 1111nnnnmmmm1001
// FMOV @Rm+, DRn
// 1111nnn0mmmm1001
DEF_FPU_HANDLER(fmov_indgeninc_fpu, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_indgeninc_fr,
                sh4_inst_binary_fmov_indgeninc_dr)

// FMOV.S FRm, @Rn
// 1111nnnnmmmm1010
// FMOV DRm, @Rn
// 1111nnnnmmm01010
DEF_FPU_HANDLER(fmov_fpu_indgen, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_fr_indgen,
                sh4_inst_binary_fmov_dr_indgen)

// FMOV.S FRm, @-Rn
// 1111nnnnmmmm1011
// FMOV DRm, @-Rn
// 1111nnnnmmm01011
DEF_FPU_HANDLER(fmov_fpu_inddecgen, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_fr_inddecgen,
                sh4_inst_binary_fmov_dr_inddecgen)

// FMOV.S FRm, @(R0, Rn)
// 1111nnnnmmmm0111
// FMOV DRm, @(R0, Rn)
// 1111nnnnmmm00111
DEF_FPU_HANDLER(fmov_fpu_binind_r0_gen, SH4_FPSCR_SZ_MASK,
                sh4_inst_binary_fmovs_fr_binind_r0_gen,
                sh4_inst_binary_fmov_dr_binind_r0_gen)

// FABS FRn
// 1111nnnn01011101
// FABS DRn
// 1111nnn001011101
DEF_FPU_HANDLER(fabs_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fabs_fr,
                sh4_inst_unary_fabs_dr)

// FADD FRm, FRn
// 1111nnnnmmmm0000
// FADD DRm, DRn
// 1111nnn0mmm00000
DEF_FPU_HANDLER(fadd_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fadd_fr_fr,
                sh4_inst_binary_fadd_dr_dr)

// FCMP/EQ FRm, FRn
// 1111nnnnmmmm0100
// FCMP/EQ DRm, DRn
// 1111nnn0mmm00100
DEF_FPU_HANDLER(fcmpeq_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fcmpeq_fr_fr,
                sh4_inst_binary_fcmpeq_dr_dr)

// FCMP/GT FRm, FRn
// 1111nnnnmmmm0101
// FCMP/GT DRm, DRn
// 1111nnn0mmm00101
DEF_FPU_HANDLER(fcmpgt_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fcmpgt_fr_fr,
                sh4_inst_binary_fcmpgt_dr_dr)

// FDIV FRm, FRn
// 1111nnnnmmmm0011
// FDIV DRm, DRn
// 1111nnn0mmm00011
DEF_FPU_HANDLER(fdiv_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fdiv_fr_fr,
                sh4_inst_binary_fdiv_dr_dr);

// FLOAT FPUL, FRn
// 1111nnnn00101101
// FLOAT FPUL, DRn
// 1111nnn000101101
DEF_FPU_HANDLER(float_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_float_fpul_fr,
                sh4_inst_binary_float_fpul_dr)

// FMAC FR0, FRm, FRn
// 1111nnnnmmmm1110
DEF_FPU_HANDLER(fmac_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_trinary_fmac_fr0_fr_fr,
                sh4_inst_invalid)

// FMUL FRm, FRn
// 1111nnnnmmmm0010
// FMUL DRm, DRn
// 1111nnn0mmm00010
DEF_FPU_HANDLER(fmul_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fmul_fr_fr,
                sh4_inst_binary_fmul_dr_dr)

// FNEG FRn
// 1111nnnn01001101
// FNEG DRn
// 1111nnn001001101
DEF_FPU_HANDLER(fneg_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fneg_fr,
                sh4_inst_unary_fneg_dr)

// FSQRT FRn
// 1111nnnn01101101
// FSQRT DRn
// 1111nnn001101101
DEF_FPU_HANDLER(fsqrt_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fsqrt_fr,
                sh4_inst_unary_fsqrt_dr);

// FSUB FRm, FRn
// 1111nnnnmmmm0001
// FSUB DRm, DRn
// 1111nnn0mmm00001
DEF_FPU_HANDLER(fsub_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fsub_fr_fr,
                sh4_inst_binary_fsub_dr_dr)

// FTRC FRm, FPUL
// 1111mmmm00111101
// FTRC DRm, FPUL
// 1111mmm000111101
DEF_FPU_HANDLER(ftrc_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_ftrc_fr_fpul,
                sh4_inst_binary_ftrc_dr_fpul)

// FCNVDS DRm, FPUL
// 1111mmm010111101
DEF_FPU_HANDLER(fcnvds_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_invalid,
                sh4_inst_binary_fcnvds_dr_fpul)

// FCNVSD FPUL, DRn
// 1111nnn010101101
DEF_FPU_HANDLER(fcnvsd_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_invalid,
                sh4_inst_binary_fcnvsd_fpul_dr);