/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include <limits>
#include <cstring>

#include "BaseException.hpp"

#include "sh4.hpp"

struct Sh4::InstOpcode Sh4::opcode_list[] = {
    // RTS
    { "0000000000001011", &Sh4::inst_rts },

    // CLRMAC
    { "0000000000101000", &Sh4::inst_clrmac },

    // CLRS
    { "0000000001001000", &Sh4::inst_clrs },

    // CLRT
    { "0000000000001000", &Sh4::inst_clrt },

    // LDTLB
    { "0000000000111000", &Sh4::inst_ldtlb },

    // NOP
    { "0000000000001001", &Sh4::inst_nop },

    // RTE
    { "0000000000101011", &Sh4::inst_rte },

    // SETS
    { "0000000001011000", &Sh4::inst_sets },

    // SETT
    { "0000000000011000", &Sh4::inst_sett },

    // SLEEP
    { "0000000000011011", &Sh4::inst_sleep },

    // FRCHG
    { "1111101111111101", &Sh4::inst_frchg },

    // FSCHG
    { "1111001111111101", &Sh4::inst_fschg },

    // MOVT
    { "0000nnnn00101001", &Sh4::inst_unary_movt_gen },

    // CMP/PZ
    { "0100nnnn00010001", &Sh4::inst_unary_cmppz_gen },

    // CMP/PL
    { "0100nnnn00010101", &Sh4::inst_unary_cmppl_gen },

    // DT
    { "0100nnnn00010000", &Sh4::inst_unary_dt_gen },

    // ROTL
    { "0100nnnn00000100", &Sh4::inst_unary_rotl_gen },

    // ROTR
    { "0100nnnn00000101", &Sh4::inst_unary_rotr_gen },

    // ROTCL
    { "0100nnnn00100100", &Sh4::inst_unary_rotcl_gen },

    // ROTCL
    { "0100nnnn00100101", &Sh4::inst_unary_rotcr_gen },

    // SHAL Rn
    { "0100nnnn00200000", &Sh4::inst_unary_shal_gen },

    // SHAR Rn
    { "0100nnnn00100001", &Sh4::inst_unary_shar_gen },

    // SHLL Rn
    { "0100nnnn00000000", &Sh4::inst_unary_shll_gen },

    // SHLR Rn
    { "0100nnnn00000001", &Sh4::inst_unary_shlr_gen },

    // SHLL2 Rn
    { "0100nnnn00001000", &Sh4::inst_unary_shll2_gen },

    // SHLL8 Rn
    { "0100nnnn00001001", &Sh4::inst_unary_shll8_gen },

    // SHLR8 Rn
    { "0100nnnn00011001", &Sh4::inst_unary_shlr8_gen },

    // SHLL16 Rn
    { "0100nnnn00101000", &Sh4::inst_unary_shll16_gen },

    // SHLR16 Rn
    { "0100nnnn00101001", &Sh4::inst_unary_shlr16_gen },

    // BRAF Rn
    { "0000nnnn00100011", &Sh4::inst_unary_braf_gen },

    // BSRF Rn
    { "0000nnnn00000011", &Sh4::inst_unary_bsrf_gen },

    // CMP/EQ #imm, R0
    { "10001000iiiiiiii", &Sh4::inst_binary_cmpeq_imm_r0 },

    // AND.B #imm, @(R0, GBR)
    { "11001101iiiiiiii", &Sh4::inst_binary_andb_imm_r0_gbr },

    // AND #imm, R0
    { "11001001iiiiiiii", &Sh4::inst_binary_and_imm_r0 },

    // OR.B #imm, @(R0, GBR)
    { "11001111iiiiiiii", &Sh4::inst_binary_orb_imm_r0_gbr },

    // OR #imm, R0
    { "11001011iiiiiiii", &Sh4::inst_binary_or_imm_r0 },

    // TST #imm, R0
    { "11001000iiiiiiii", &Sh4::inst_binary_tst_imm_r0 },

    // TST.B #imm, @(R0, GBR)
    { "11001100iiiiiiii", &Sh4::inst_binary_tstb_imm_r0_gbr },

    // XOR #imm, R0
    { "11001010iiiiiiii", &Sh4::inst_binary_xor_imm_r0 },

    // XOR.B #imm, @(R0, GBR)
    { "11001110iiiiiiii", &Sh4::inst_binary_xorb_imm_r0_gbr },

    // BF label
    { "10001011dddddddd", &Sh4::inst_unary_bf_disp },

    // BF/S label
    { "10001111dddddddd", &Sh4::inst_unary_bfs_disp },

    // BT label
    { "10001001dddddddd", &Sh4::inst_unary_bt_disp },

    // BT/S label
    { "10001101dddddddd", &Sh4::inst_unary_bts_disp },

    // BRA label
    { "1010dddddddddddd", &Sh4::inst_unary_bra_disp },

    // BSR label
    { "1011dddddddddddd", &Sh4::inst_unary_bsr_disp },

    // TRAPA #immed
    { "11000011iiiiiiii", &Sh4::inst_unary_trapa_disp },

    // TAS.B @Rn
    { "0100nnnn00011011", &Sh4::inst_unary_tasb_gen },

    // OCBI @Rn
    { "0000nnnn10100011", &Sh4::inst_unary_ocbi_indgen },

    // OCBP @Rn
    { "0000nnnn10100011", &Sh4::inst_unary_ocbp_indgen },

    // PREF @Rn
    { "0000nnnn10000011", &Sh4::inst_unary_pref_indgen },

    // JMP @Rn
    { "0100nnnn00101011", &Sh4::inst_unary_jmp_indgen },

    // JSR @Rn
    { "0100nnnn00001011", &Sh4::inst_unary_jsr_indgen },

    // LDC Rm, SR
    { "0100mmmm00001110", &Sh4::inst_binary_ldc_gen_sr },

    // LDC Rm, GBR
    { "0100mmmm00011110", &Sh4::inst_binary_ldc_gen_gbr },

    // LDC Rm, VBR
    { "0100mmmm00101110", &Sh4::inst_binary_ldc_gen_vbr },

    // LDC Rm, SSR
    { "0100mmmm00111110", &Sh4::inst_binary_ldc_gen_ssr },

    // LDC Rm, SPC
    { "0100mmmm01001110", &Sh4::inst_binary_ldc_gen_spc },

    // LDC Rm, DBR
    { "0100mmmm11111010", &Sh4::inst_binary_ldc_gen_dbr },

    // STC SR, Rn
    { "0000nnnn00000010", &Sh4::inst_binary_stc_sr_gen },

    // STC GBR, Rn
    { "0000nnnn00010010", &Sh4::inst_binary_stc_gbr_gen },

    // STC VBR, Rn
    { "0000nnnn00100010", &Sh4::inst_binary_stc_vbr_gen },

    // STC SSR, Rn
    { "0000nnnn00110010", &Sh4::inst_binary_stc_ssr_gen },

    // STC SPC, Rn
    { "0000nnnn01000010", &Sh4::inst_binary_stc_spc_gen },

    // STC SGR, Rn
    { "0000nnnn00111010", &Sh4::inst_binary_stc_sgr_gen },

    // STC DBR, Rn
    { "0000nnnn11111010", &Sh4::inst_binary_stc_dbr_gen },

    // LDC.L @Rm+, SR
    { "0100mmmm00000111", &Sh4::inst_binary_ldcl_indgeninc_sr },

    // LDC.L @Rm+, GBR
    { "0100mmmm00010111", &Sh4::inst_binary_ldcl_indgeninc_gbr },

    // LDC.L @Rm+, VBR
    { "0100mmmm00100111", &Sh4::inst_binary_ldcl_indgeninc_vbr },

    // LDC.L @Rm+, SSR
    { "0100mmmm00110111", &Sh4::inst_binary_ldcl_indgenic_ssr },

    // LDC.L @Rm+, SPC
    { "0100mmmm01000111", &Sh4::inst_binary_ldcl_indgeninc_spc },

    // LDC.L @Rm+, DBR
    { "0100mmmm11110110", &Sh4::inst_binary_ldcl_indgeninc_dbr },

    // STC.L SR, @-Rn
    { "0100nnnn00000011", &Sh4::inst_binary_stcl_sr_inddecgen },

    // STC.L GBR, @-Rn
    { "0100nnnn00010011", &Sh4::inst_binary_stcl_gbr_inddecgen },

    // STC.L VBR, @-Rn
    { "0100nnnn00100011", &Sh4::inst_binary_stcl_vbr_inddecgen },

    // STC.L SSR, @-Rn
    { "0100nnnn00110011", &Sh4::inst_binary_stcl_ssr_inddecgen },

    // STC.L SPC, @-Rn
    { "0100nnnn01000011", &Sh4::inst_binary_stcl_spc_inddecgen },

    // STC.L SGR, @-Rn
    { "0100nnnn00110010", &Sh4::inst_binary_stcl_sgr_inddecgen },

    // STC.L DBR, @-Rn
    { "0100nnnn11110010", &Sh4::inst_binary_stcl_dbr_inddecgen },

    // MOV #imm, Rn
    { "1110nnnniiiiiiii", &Sh4::inst_binary_mov_imm_gen },

    // ADD #imm, Rn
    { "0111nnnniiiiiiii", &Sh4::inst_binary_add_imm_gen },

    // MOV.W @(disp, PC), Rn
    { "1001nnnndddddddd", &Sh4::inst_binary_movw_binind_disp_pc_gen },

    // MOV.L @(disp, PC), Rn
    { "1101nnnndddddddd", &Sh4::inst_binary_movl_binind_disp_pc_gen },

    // MOV Rm, Rn
    { "0110nnnnmmmm0011", &Sh4::inst_binary_movw_gen_gen },

    // SWAP.B Rm, Rn
    { "0110nnnnmmmm1000", &Sh4::inst_binary_swapb_gen_gen },

    // SWAP.W Rm, Rn
    { "0110nnnnmmmm1001", &Sh4::inst_binary_swapw_gen_gen },

    // XTRCT Rm, Rn
    { "0110nnnnmmmm1101", &Sh4::inst_binary_xtrct_gen_gen },

    // ADD Rm, Rn
    { "0011nnnnmmmm1100", &Sh4::inst_binary_add_gen_gen },

    // ADDC Rm, Rn
    { "0011nnnnmmmm1110", &Sh4::inst_binary_addc_gen_gen },

    // ADDV Rm, Rn
    { "0011nnnnmmmm1111", &Sh4::inst_binary_addv_gen_gen },

    // CMP/EQ Rm, Rn
    { "0011nnnnmmmm0000", &Sh4::inst_binary_cmpeq_gen_gen },

    // CMP/HS Rm, Rn
    { "0011nnnnmmmm0010", &Sh4::inst_binary_cmphs_gen_gen },

    // CMP/GE Rm, Rn
    { "0011nnnnmmmm0011", &Sh4::inst_binary_cmpge_gen_gen },

    // CMP/HI Rm, Rn
    { "0011nnnnmmmm0110", &Sh4::inst_binary_cmphi_gen_gen },

    // CMP/GT Rm, Rn
    { "0011nnnnmmmm0111", &Sh4::inst_binary_cmpgt_gen_gen },

    // CMP/STR Rm, Rn
    { "0010nnnnmmmm1100", &Sh4::inst_binary_cmpstr_gen_gen },

    // DIV1 Rm, Rn
    { "0011nnnnmmmm0100", &Sh4::inst_binary_div1_gen_gen },

    // DIV0S Rm, Rn
    { "0010nnnnmmmm0111", &Sh4::inst_binary_div0s_gen_gen },

    // DMULS.L Rm, Rn
    { "0011nnnnmmmm1101", &Sh4::inst_binary_dmulsl_gen_gen },

    // DMULU.L Rm, Rn
    { "0011nnnnmmmm0101", &Sh4::inst_binary_dmulul_gen_gen },

    // EXTS.B Rm, Rn
    { "0110nnnnmmmm1110", &Sh4::inst_binary_extsb_gen_gen },

    // EXTS.W Rm, Rn
    { "0110nnnnmmmm1111", &Sh4::inst_binary_extsw_gen_gen },

    // EXTU.B Rm, Rn
    { "0110nnnnmmmm1100", &Sh4::inst_binary_extub_gen_gen },

    // EXTU.W Rm, Rn
    { "0110nnnnmmmm1101", &Sh4::inst_binary_extuw_gen_gen },

    // MUL.L Rm, Rn
    { "0000nnnnmmmm0111", &Sh4::inst_binary_mull_gen_gen },

    // MULS.W Rm, Rn
    { "0010nnnnmmmm1111", &Sh4::inst_binary_mulsw_gen_gen },

    // MULU.W Rm, Rn
    { "0010nnnnmmmm1110", &Sh4::inst_binary_muluw_gen_gen },

    // NEG Rm, Rn
    { "0110nnnnmmmm1011", &Sh4::inst_binary_neg_gen_gen },

    // NEGC Rm, Rn
    { "0110nnnnmmmm1010", &Sh4::inst_binary_negc_gen_gen },

    // SUB Rm, Rn
    { "0011nnnnmmmm1000", &Sh4::inst_binary_sub_gen_gen },

    // SUBC Rm, Rn
    { "0011nnnnmmmm1010", &Sh4::inst_binary_subc_gen_gen },

    // SUBV Rm, Rn
    { "0011nnnnmmmm1011", &Sh4::inst_binary_subv_gen_gen },

    // AND Rm, Rn
    { "0010nnnnmmmm1001", &Sh4::inst_binary_and_gen_gen },

    // NOT Rm, Rn
    { "0110nnnnmmmm0111", &Sh4::inst_binary_not_gen_gen },

    // OR Rm, Rn
    { "0010nnnnmmmm1011", &Sh4::inst_binary_or_gen_gen },

    // TST Rm, Rn
    { "0010nnnnmmmm1000", &Sh4::inst_binary_tst_gen_gen },

    // XOR Rm, Rn
    { "0010nnnnmmmm1010", &Sh4::inst_binary_xor_gen_gen },

    // SHAD Rm, Rn
    { "0100nnnnmmmm1100", &Sh4::inst_binary_shad_gen_gen },

    // SHLD Rm, Rn
    { "0100nnnnmmmm1101", &Sh4::inst_binary_shld_gen_gen },

    // LDC Rm, Rn_BANK
    { "0100mmmm1nnn1110", &Sh4::inst_binary_ldc_gen_bank },

    // LDC.L @Rm+, Rn_BANK
    { "0100mmmm1nnn0111", &Sh4::inst_binary_ldcl_indgeninc_bank },

    // STC Rm_BANK, Rn
    { "0000nnnn1mmm0010", &Sh4::inst_binary_stc_bank_gen },

    // STC.L Rm_BANK, @-Rn
    { "0100nnnn1mmm0011", &Sh4::inst_binary_stcl_bank_inddecgen },

    // LDS Rm,MACH
    { "0100mmmm00001010", &Sh4::inst_binary_lds_gen_mach },

    // LDS Rm, MACL
    { "0100mmmm00011010", &Sh4::inst_binary_lds_gen_macl },

    // STS MACH, Rn
    { "0000nnnn00001010", &Sh4::inst_binary_sts_mach_gen },

    // STS MACL, Rn
    { "0000nnnn00011010", &Sh4::inst_binary_sts_macl_gen },

    // LDS Rm, PR
    { "0100mmmm00101010", &Sh4::inst_binary_lds_gen_pr },

    // STS PR, Rn
    { "0000nnnn00101010", &Sh4::inst_binary_sts_pr_gen },

    // LDS.L @Rm+, MACH
    { "0100mmmm00000110", &Sh4::inst_binary_ldsl_indgeninc_mach },

    // LDS.L @Rm+, MACL
    { "0100mmmm00010110", &Sh4::inst_binary_ldsl_indgeninc_macl },

    // STS.L MACH, @-Rn
    { "0100mmmm00000010", &Sh4::inst_binary_stsl_mach_inddecgen },

    // STS.L MACL, @-Rn
    { "0100mmmm00010010", &Sh4::inst_binary_stsl_macl_inddecgen },

    // LDS.L @Rm+, PR
    { "0100mmmm00100110", &Sh4::inst_binary_ldsl_indgeninc_pr },

    // STS.L PR, @-Rn
    { "0100nnnn00100010", &Sh4::inst_binary_stsl_pr_inddecgen },

    // MOV.B Rm, @Rn
    { "0010nnnnmmmm0000", &Sh4::inst_binary_movb_gen_indgen },

    // MOV.W Rm, @Rn
    { "0010nnnnmmmm0001", &Sh4::inst_binary_movw_gen_indgen },

    // MOV.L Rm, @Rn
    { "0010nnnnmmmm0010", &Sh4::inst_binary_movl_gen_indgen },

    // MOV.B @Rm, Rn
    { "0110nnnnmmmm0000", &Sh4::inst_binary_movb_indgen_gen },

    // MOV.W @Rm, Rn
    { "0110nnnnmmmm0001", &Sh4::inst_binary_movw_indgen_gen },

    // MOV.L @Rm, Rn
    { "0110nnnnmmmm0010", &Sh4::inst_binary_movl_indgen_gen },

    // MOV.B Rm, @-Rn
    { "0010nnnnmmmm0100", &Sh4::inst_binary_movb_gen_inddecgen },

    // MOV.W Rm, @-Rn
    { "0010nnnnmmmm0101", &Sh4::inst_binary_movw_gen_inddecgen },

    // MOV.L Rm, @-Rn
    { "0010nnnnmmmm0110", &Sh4::inst_binary_movl_gen_inddecgen },

    // MOV.B @Rm+, Rn
    { "0110nnnnmmmm0100", &Sh4::inst_binary_movb_indgeninc_gen },

    // MOV.W @Rm+, Rn
    { "0110nnnnmmmm0101", &Sh4::inst_binary_movw_indgeninc_gen },

    // MOV.L @Rm+, Rn
    { "0110nnnnmmmm0110", &Sh4::inst_binary_movl_indgeninc_gen },

    // MAC.L @Rm+, @Rn+
    { "0000nnnnmmmm1111", &Sh4::inst_binary_macl_indgeninc_indgeninc },

    // MAC.W @Rm+, @Rn+
    { "0100nnnnmmmm1111", &Sh4::inst_binary_macw_indgeninc_indgeninc },

    // MOV.B R0, @(disp, Rn)
    { "10000000nnnndddd", &Sh4::inst_binary_movb_r0_binind_disp_gen },

    // MOV.W R0, @(disp, Rn)
    { "10000001nnnndddd", &Sh4::inst_binary_movw_r0_binind_disp_gen },

    // MOV.L Rm, @(disp, Rn)
    { "0001nnnnmmmmdddd", &Sh4::inst_binary_movl_gen_binind_disp_gen },

    // MOV.B @(disp, Rm), R0
    { "10000100mmmmdddd", &Sh4::inst_binary_movb_binind_disp_gen_r0 },

    // MOV.W @(disp, Rm), R0
    { "10000101mmmmdddd", &Sh4::inst_binary_movw_binind_disp_gen_r0 },

    // MOV.L @(disp, Rm), Rn
    { "0101nnnnmmmmdddd", &Sh4::inst_binary_movl_binind_disp_gen_gen },

    // MOV.B Rm, @(R0, Rn)
    { "0000nnnnmmmm0100", &Sh4::inst_binary_movb_gen_binind_r0_gen },

    // MOV.W Rm, @(R0, Rn)
    { "0000nnnnmmmm0101", &Sh4::inst_binary_movw_gen_binind_r0_gen },

    // MOV.L Rm, @(R0, Rn)
    { "0000nnnnmmmm0110", &Sh4::inst_binary_movl_gen_binind_r0_gen },

    // MOV.B @(R0, Rm), Rn
    { "0000nnnnmmmm1100", &Sh4::inst_binary_movb_binind_r0_gen_gen },

    // MOV.W @(R0, Rm), Rn
    { "0000nnnnmmmm1101", &Sh4::inst_binary_movw_binind_r0_gen_gen },

    // MOV.L @(R0, Rm), Rn
    { "0000nnnnmmmm1110", &Sh4::inst_binary_movl_binind_r0_gen_gen },

    // MOV.B R0, @(disp, GBR)
    { "11000000dddddddd", &Sh4::inst_binary_movb_r0_binind_disp_gbr },

    // MOV.W R0, @(disp, GBR)
    { "11000001dddddddd", &Sh4::inst_binary_movw_r0_binind_disp_gbr },

    // MOV.L R0, @(disp, GBR)
    { "11000010dddddddd", &Sh4::inst_binary_movl_r0_binind_disp_gbr },

    // MOV.B @(disp, GBR), R0
    { "11000100dddddddd", &Sh4::inst_binary_movb_binind_disp_gbr_r0 },

    // MOV.W @(disp, GBR), R0
    { "11000101dddddddd", &Sh4::inst_binary_movw_binind_disp_gbr_r0 },

    // MOV.L @(disp, GBR), R0
    { "11000110dddddddd", &Sh4::inst_binary_movl_binind_disp_gbr_r0 },

    // MOVA @(disp, PC), R0
    { "11000111dddddddd", &Sh4::inst_binary_mova_binind_disp_pc_r0 },

    // MOVCA.L R0, @Rn
    { "0000nnnn11000011", &Sh4::inst_binary_movcal_r0_indgen },

    // FLDI0 FRn
    { "1111nnnn10001101", &Sh4::inst_unary_fldi0_fr },

    // FLDI1 Frn
    { "1111nnnn10011101", &Sh4::inst_unary_fldi1_fr },

    // FMOV FRm, FRn
    { "1111nnnnmmmm1100", &Sh4::inst_binary_fmov_fr_fr },

    // FMOV.S @Rm, FRn
    { "1111nnnnmmmm1000", &Sh4::inst_binary_fmovs_indgen_fr },

    // FMOV.S @(R0,Rm), FRn
    { "1111nnnnmmmm0110", &Sh4::inst_binary_fmovs_binind_r0_gen_fr },

    // FMOV.S @Rm+, FRn
    { "1111nnnnmmmm1001", &Sh4::inst_binary_fmovs_indgeninc_fr },

    // FMOV.S FRm, @Rn
    { "1111nnnnmmmm1010", &Sh4::inst_binary_fmovs_fr_indgen },

    // FMOV.S FRm, @-Rn
    { "1111nnnnmmmm1011", &Sh4::inst_binary_fmovs_fr_inddecgen },

    // FMOV.S FRm, @(R0, Rn)
    { "1111nnnnmmmm0111", &Sh4::inst_binary_fmovs_fr_binind_r0_gen },

    // FMOV DRm, DRn
    { "1111nnn0mmm01100", &Sh4::inst_binary_fmov_dr_dr },

    // FMOV @Rm, DRn
    { "1111nnn0mmmm1000", &Sh4::inst_binary_fmov_indgen_dr },

    // FMOV @(R0, Rm), DRn
    { "1111nnn0mmmm0110", &Sh4::inst_binary_fmov_binind_r0_gen_dr },

    // FMOV @Rm+, DRn
    { "1111nnn0mmmm1001", &Sh4::inst_binary_fmov_indgeninc_dr },

    // FMOV DRm, @Rn
    { "1111nnnnmmm01010", &Sh4::inst_binary_fmov_dr_indgen },

    // FMOV DRm, @-Rn
    { "1111nnnnmmm01011", &Sh4::inst_binary_fmov_dr_inddecgen },

    // FMOV DRm, @(R0,Rn)
    { "1111nnnnmmm00111", &Sh4::inst_binary_fmov_dr_binind_r0_gen },

    // FLDS FRm, FPUL
    { "1111mmmm00011101", &Sh4::inst_binary_flds_fr_fpul },

    // FSTS FPUL, FRn
    { "1111nnnn00001101", &Sh4::inst_binary_fsts_fpul_fp },

    // FABS FRn
    { "1111nnnn01011101", &Sh4::inst_unary_fabs_fr },

    // FADD FRm, FRn
    { "1111nnnnmmmm0000", &Sh4::inst_binary_fadd_fr_fr },

    // FCMP/EQ FRm, FRn
    { "1111nnnnmmmm0100", &Sh4::inst_binary_fcmpeq_fr_fr },

    // FCMP/GT FRm, FRn
    { "1111nnnnmmmm0101", &Sh4::inst_binary_fcmpgt_fr_fr },

    // FDIV FRm, FRn
    { "1111nnnnmmmm0011", &Sh4::inst_binary_fdiv_fr_fr },

    // FLOAT FPUL, FRn
    { "1111nnnn00101101", &Sh4::inst_binary_float_fpul_fr },

    // FMAC FR0, FRm, FRn
    { "1111nnnnmmmm1110", &Sh4::inst_trinary_fmac_fr0_fr_fr },

    // FMUL FRm, FRn
    { "1111nnnnmmmm0010", &Sh4::inst_binary_fmul_fr_fr },

    // FNEG FRn
    { "1111nnnn01001101", &Sh4::inst_unary_fneg_fr },

    // FSQRT FRn
    { "1111nnnn01101101", &Sh4::inst_unary_fsqrt_fr },

    // FSUB FRm, FRn
    { "1111nnnnmmmm0001", &Sh4::inst_binary_fsub_fr_fr },

    // FTRC FRm, FPUL
    { "1111mmmm00111101", &Sh4::inst_binary_ftrc_fr_fpul },

    // FABS DRn
    { "1111nnn001011101", &Sh4::inst_unary_fabs_dr },

    // FADD DRm, DRn
    { "1111nnn0mmm00000", &Sh4::inst_binary_fadd_dr_dr },

    // FCMP/EQ DRm, DRn
    { "1111nnn0mmm00100", &Sh4::inst_binary_fcmpeq_dr_dr },

    // FCMP/GT DRm, DRn
    { "1111nnn0mmm00101", &Sh4::inst_binary_fcmpgt_dr_dr },

    // FDIV DRm, DRn
    { "1111nnn0mmm00011", &Sh4::inst_binary_fdiv_dr_dr },

    // FCNVDS DRm, FPUL
    { "1111mmm010111101", &Sh4::inst_binary_fcnvds_dr_fpul },

    // FCNVSD FPUL, DRn
    { "1111nnn010101101", &Sh4::inst_binary_fcnvsd_fpul_dr },

    // FLOAT FPUL, DRn
    { "1111nnn000101101", &Sh4::inst_binary_float_fpul_dr },

    // FMUL DRm, DRn
    { "1111nnn0mmm00010", &Sh4::inst_binary_fmul_dr_dr },

    // FNEG DRn
    { "1111nnn001001101", &Sh4::inst_unary_fneg_dr },

    // FSQRT DRn
    { "1111nnn001101101", &Sh4::inst_unary_fsqrt_dr },

    // FSUB DRm, DRn
    { "1111nnn0mmm00001", &Sh4::inst_binary_fsub_dr_dr },

    // FTRC DRm, FPUL
    { "1111mmm000111101", &Sh4::inst_binary_ftrc_dr_fpul },

    // LDS Rm, FPSCR
    { "0100mmmm01101010", &Sh4::inst_binary_lds_gen_fpscr },

    // LDS Rm, FPUL
    { "0100mmmm01011010", &Sh4::inst_binary_gen_fpul },

    // LDS.L @Rm+, FPSCR
    { "0100mmmm01100110", &Sh4::inst_binary_ldsl_indgeninc_fpscr },

    // LDS.L @Rm+, FPUL
    { "0100mmmm01010110", &Sh4::inst_binary_ldsl_indgeninc_fpul },

    // STS FPSCR, Rn
    { "0000nnnn01101010", &Sh4::inst_binary_sts_fpscr_gen },

    // STS FPUL, Rn
    { "0000nnnn01011010", &Sh4::inst_binary_sts_fpul_gen },

    // STS.L FPSCR, @-Rn
    { "0100nnnn01100010", &Sh4::inst_binary_stsl_fpscr_inddecgen },

    // STS.L FPUL, @-Rn
    { "0100nnnn01010010", &Sh4::inst_binary_stsl_fpul_inddecgen },

    // FMOV DRm, XDn
    { "1111nnn1mmm01100", &Sh4::inst_binary_fmove_dr_xd },

    // FMOV XDm, DRn
    { "1111nnn0mmm11100", &Sh4::inst_binary_fmov_xd_dr },

    // FMOV XDm, XDn
    { "1111nnn1mmm11100", &Sh4::inst_binary_fmov_xd_xd },

    // FMOV @Rm, XDn
    { "1111nnn1mmmm1000", &Sh4::inst_binary_fmov_indgen_xd },

    // FMOV @Rm+, XDn
    { "1111nnn1mmmm1001", &Sh4::inst_binary_fmov_indgeninc_xd },

    // FMOV @(R0, Rn), XDn
    { "1111nnn1mmmm0110", &Sh4::inst_binary_fmov_binind_r0_gen_xd },

    // FMOV XDm, @Rn
    { "1111nnnnmmm11010", &Sh4::inst_binary_fmov_xd_indgen },

    // FMOV XDm, @-Rn
    { "1111nnnnmmm11011", &Sh4::inst_binary_fmov_xd_inddecgen },

    // FMOV XDm, @(R0, Rn)
    { "1111nnnnmmm10111", &Sh4::inst_binary_fmov_xs_binind_r0_gen },

    // FIPR FVm, FVn - vector dot product
    { "1111nnmm11101101", &Sh4::inst_binary_fipr_fv_fv },

    // FTRV MXTRX, FVn - multiple vector by matrix
    { "1111nn0111111101", &Sh4::inst_binary_fitrv_mxtrx_fv },

    { NULL }
};

void Sh4::exec_inst() {
    inst_t inst;
    int exc_pending;

    if ((exc_pending = read_inst(&inst, reg.pc))) {
        // fuck it, i'll commit now and figure what to do here later
        throw UnimplementedError("Something to do with exceptions, I guess");
    }

    do_exec_inst(inst);

    reg.pc += 2;
}

void Sh4::do_exec_inst(inst_t inst) {
    InstOpcode *op = opcode_list;
    OpArgs oa;

    oa.inst = inst;

    while (op->fmt) {
        if ((op->mask & inst) == op->val) {
            opcode_func_t op_func = op->func;
            (this->*op_func)(oa);
            return;
        }
        op++;
    }

    throw UnimplementedError("CPU exception for unrecognized opcode");
}

void Sh4::compile_instructions() {
    InstOpcode *op = opcode_list;

    while (op->fmt) {
        compile_instruction(op);
        op++;
    }
}

void Sh4::compile_instruction(struct Sh4::InstOpcode *op) {
    char const *fmt = op->fmt;
    inst_t mask = 0, val = 0;

    if (strlen(fmt) != 16)
        throw InvalidParamError("Invalid instruction opcode format");

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
void Sh4::inst_rts(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}


// CLRMAC
// 0000000000101000
void Sh4::inst_clrmac(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}


// CLRS
// 0000000001001000
void Sh4::inst_clrs(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}


// CLRT
// 0000000000001000
void Sh4::inst_clrt(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDTLB
// 0000000000111000
void Sh4::inst_ldtlb(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// NOP
// 0000000000001001
void Sh4::inst_nop(OpArgs inst) {
    // do nothing
}

// RTE
// 0000000000101011
void Sh4::inst_rte(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SETS
// 0000000001011000
void Sh4::inst_sets(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SETT
// 0000000000011000
void Sh4::inst_sett(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SLEEP
// 0000000000011011
void Sh4::inst_sleep(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FRCHG
// 1111101111111101
void Sh4::inst_frchg(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSCHG
// 1111001111111101
void Sh4::inst_fschg(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MOVT Rn
// 0000nnnn00101001
void Sh4::inst_unary_movt_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) =
        (reg32_t)((reg.sr & SR_FLAG_T_MASK) >> SR_FLAG_T_SHIFT);
}

// CMP/PZ Rn
// 0100nnnn00010001
void Sh4::inst_unary_cmppz_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    uint32_t flag = (*gen_reg(inst.gen_reg)) >= 0;

    reg.sr |= flag << SR_FLAG_T_SHIFT;
}

// CMP/PL Rn
// 0100nnnn00010101
void Sh4::inst_unary_cmppl_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    uint32_t flag = (*gen_reg(inst.gen_reg)) > 0;

    reg.sr |= flag << SR_FLAG_T_SHIFT;
}

// DT Rn
// 0100nnnn00010000
void Sh4::inst_unary_dt_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// ROTL Rn
// 0100nnnn00000100
void Sh4::inst_unary_rotl_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// ROTR Rn
// 0100nnnn00000101
void Sh4::inst_unary_rotr_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// ROTCL Rn
// 0100nnnn00100100
void Sh4::inst_unary_rotcl_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// ROTCR Rn
// 0100nnnn00100101
void Sh4::inst_unary_rotcr_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHAL Rn
// 0100nnnn00200000
void Sh4::inst_unary_shal_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHAR Rn
// 0100nnnn00100001
void Sh4::inst_unary_shar_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLL Rn
// 0100nnnn00000000
void Sh4::inst_unary_shll_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLR Rn
// 0100nnnn00000001
void Sh4::inst_unary_shlr_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLL2 Rn
// 0100nnnn00001000
void Sh4::inst_unary_shll2_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLR2 Rn
// 0100nnnn00001001
void Sh4::inst_unary_shlr2_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLL8 Rn
// 0100nnnn00011000
void Sh4::inst_unary_shll8_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLR8 Rn
// 0100nnnn00011001
void Sh4::inst_unary_shlr8_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLL16 Rn
// 0100nnnn00101000
void Sh4::inst_unary_shll16_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLR16 Rn
// 0100nnnn00101001
void Sh4::inst_unary_shlr16_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BRAF Rn
// 0000nnnn00100011
void Sh4::inst_unary_braf_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BSRF Rn
// 0000nnnn00000011
void Sh4::inst_unary_bsrf_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// CMP/EQ #imm, R0
// 10001000iiiiiiii
void Sh4::inst_binary_cmpeq_imm_r0(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= ((*gen_reg(0) == inst.imm8) << SR_FLAG_T_SHIFT);
}

// AND.B #imm, @(R0, GBR)
// 11001101iiiiiiii
void Sh4::inst_binary_andb_imm_r0_gbr(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + reg.gbr;
    uint8_t val;

    if (read_mem(&val, addr, sizeof(val)) != 0)
        return;

    val &= inst.imm8;

    if (write_mem(&val, addr, sizeof(val)) != 0)
        return;
}

// AND #imm, R0
// 11001001iiiiiiii
void Sh4::inst_binary_and_imm_r0(OpArgs inst) {
    *gen_reg(0) &= inst.imm8;
}

// OR.B #imm, @(R0, GBR)
// 11001111iiiiiiii
void Sh4::inst_binary_orb_imm_r0_gbr(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + reg.gbr;
    uint8_t val;

    if (read_mem(&val, addr, sizeof(val)) != 0)
        return;

    val |= inst.imm8;

    if (write_mem(&val, addr, sizeof(val)) != 0)
        return;
}

// OR #imm, R0
// 11001011iiiiiiii
void Sh4::inst_binary_or_imm_r0(OpArgs inst) {
    *gen_reg(0) |= inst.imm8;
}

// TST #imm, R0
// 11001000iiiiiiii
void Sh4::inst_binary_tst_imm_r0(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    reg32_t flag = !(inst.imm8 & *gen_reg(0)) <<
        SR_FLAG_T_SHIFT;
    reg.sr |= flag;
}

// TST.B #imm, @(R0, GBR)
// 11001100iiiiiiii
void Sh4::inst_binary_tstb_imm_r0_gbr(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + reg.gbr;
    uint8_t val;

    read_mem(&val, addr, sizeof(val));

    reg.sr &= ~SR_FLAG_T_MASK;
    reg32_t flag = !(inst.imm8 & val) <<
        SR_FLAG_T_SHIFT;
    reg.sr |= flag;
}

// XOR #imm, R0
// 11001010iiiiiiii
void Sh4::inst_binary_xor_imm_r0(OpArgs inst) {
    *gen_reg(0) ^= inst.imm8;
}

// XOR.B #imm, @(R0, GBR)
// 11001110iiiiiiii
void Sh4::inst_binary_xorb_imm_r0_gbr(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + reg.gbr;
    uint8_t val;

    if (read_mem(&val, addr, sizeof(val)) != 0)
        return;

    val ^= inst.imm8;

    if (write_mem(&val, addr, sizeof(val)) != 0)
        return;
}

// BF label
// 10001011dddddddd
void Sh4::inst_unary_bf_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BF/S label
// 10001111dddddddd
void Sh4::inst_unary_bfs_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BT label
// 10001001dddddddd
void Sh4::inst_unary_bt_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BT/S label
// 10001101dddddddd
void Sh4::inst_unary_bts_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BRA label
// 1010dddddddddddd
void Sh4::inst_unary_bra_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// BSR label
// 1011dddddddddddd
void Sh4::inst_unary_bsr_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// TRAPA #immed
// 11000011iiiiiiii
void Sh4::inst_unary_trapa_disp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// TAS.B @Rn
// 0100nnnn00011011
void Sh4::inst_unary_tasb_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.gen_reg);
    uint8_t val;
    reg32_t mask;

    read_mem(&val, addr, sizeof(val));
    reg.sr &= ~SR_FLAG_T_MASK;
    mask = (!val) << SR_FLAG_T_SHIFT;
    reg.sr |= mask;
    val |= 0x80;
    write_mem(&val, addr, sizeof(val));
}

// OCBI @Rn
// 0000nnnn10100011
void Sh4::inst_unary_ocbi_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// OCBP @Rn
// 0000nnnn10100011
void Sh4::inst_unary_ocbp_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// PREF @Rn
// 0000nnnn10000011
void Sh4::inst_unary_pref_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// JMP @Rn
// 0100nnnn00101011
void Sh4::inst_unary_jmp_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// JSR @Rn
//0100nnnn00001011
void Sh4::inst_unary_jsr_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDC Rm, SR
// 0100mmmm00001110
void Sh4::inst_binary_ldc_gen_sr(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg.sr = *gen_reg(inst.gen_reg);
}

// LDC Rm, GBR
// 0100mmmm00011110
void Sh4::inst_binary_ldc_gen_gbr(OpArgs inst) {
    reg.gbr = *gen_reg(inst.gen_reg);
}

// LDC Rm, VBR
// 0100mmmm00101110
void Sh4::inst_binary_ldc_gen_vbr(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg.vbr = *gen_reg(inst.gen_reg);
}

// LDC Rm, SSR
// 0100mmmm00111110
void Sh4::inst_binary_ldc_gen_ssr(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg.ssr = *gen_reg(inst.gen_reg);
}

// LDC Rm, SPC
// 0100mmmm01001110
void Sh4::inst_binary_ldc_gen_spc(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg.spc = *gen_reg(inst.gen_reg);
}

// LDC Rm, DBR
// 0100mmmm11111010
void Sh4::inst_binary_ldc_gen_dbr(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg.dbr = *gen_reg(inst.gen_reg);
}

// STC SR, Rn
// 0000nnnn00000010
void Sh4::inst_binary_stc_sr_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.sr;
}

// STC GBR, Rn
// 0000nnnn00010010
void Sh4::inst_binary_stc_gbr_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) = reg.gbr;
}

// STC VBR, Rn
// 0000nnnn00100010
void Sh4::inst_binary_stc_vbr_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.vbr;
}

// STC SSR, Rn
// 0000nnnn00110010
void Sh4::inst_binary_stc_ssr_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.ssr;
}

// STC SPC, Rn
// 0000nnnn01000010
void Sh4::inst_binary_stc_spc_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.spc;
}

// STC SGR, Rn
// 0000nnnn00111010
void Sh4::inst_binary_stc_sgr_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.sgr;
}

// STC DBR, Rn
// 0000nnnn11111010
void Sh4::inst_binary_stc_dbr_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = reg.dbr;
}

// LDC.L @Rm+, SR
// 0100mmmm00000111
void Sh4::inst_binary_ldcl_indgeninc_sr(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.sr = val;
}

// LDC.L @Rm+, GBR
// 0100mmmm00010111
void Sh4::inst_binary_ldcl_indgeninc_gbr(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.gbr = val;
}

// LDC.L @Rm+, VBR
// 0100mmmm00100111
void Sh4::inst_binary_ldcl_indgeninc_vbr(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.vbr = val;
}

// LDC.L @Rm+, SSR
// 0100mmmm00110111
void Sh4::inst_binary_ldcl_indgenic_ssr(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.ssr = val;
}

// LDC.L @Rm+, SPC
// 0100mmmm01000111
void Sh4::inst_binary_ldcl_indgeninc_spc(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.spc = val;
}

// LDC.L @Rm+, DBR
// 0100mmmm11110110
void Sh4::inst_binary_ldcl_indgeninc_dbr(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    reg.dbr = val;
}

// STC.L SR, @-Rn
// 0100nnnn00000011
void Sh4::inst_binary_stcl_sr_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.sr, addr, sizeof(reg.sr)) != 0)
        return;

    *regp = addr;
}

// STC.L GBR, @-Rn
// 0100nnnn00010011
void Sh4::inst_binary_stcl_gbr_inddecgen(OpArgs inst) {
    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.gbr, addr, sizeof(reg.gbr)) != 0)
        return;

    *regp = addr;
}

// STC.L VBR, @-Rn
// 0100nnnn00100011
void Sh4::inst_binary_stcl_vbr_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.vbr, addr, sizeof(reg.vbr)) != 0)
        return;

    *regp = addr;
}

// STC.L SSR, @-Rn
// 0100nnnn00110011
void Sh4::inst_binary_stcl_ssr_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.ssr, addr, sizeof(reg.ssr)) != 0)
        return;

    *regp = addr;
}

// STC.L SPC, @-Rn
// 0100nnnn01000011
void Sh4::inst_binary_stcl_spc_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.spc, addr, sizeof(reg.spc)) != 0)
        return;

    *regp = addr;
}

// STC.L SGR, @-Rn
// 0100nnnn00110010
void Sh4::inst_binary_stcl_sgr_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.sgr, addr, sizeof(reg.sgr)) != 0)
        return;

    *regp = addr;
}

// STC.L DBR, @-Rn
// 0100nnnn11110010
void Sh4::inst_binary_stcl_dbr_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *regp = gen_reg(inst.gen_reg);
    addr32_t addr = *regp - 4;
    if (write_mem(&reg.dbr, addr, sizeof(reg.dbr)) != 0)
        return;

    *regp = addr;
}

// MOV #imm, Rn
// 1110nnnniiiiiiii
void Sh4::inst_binary_mov_imm_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) = (reg32_t)(inst.imm8);
}

// ADD #imm, Rn
// 0111nnnniiiiiiii
void Sh4::inst_binary_add_imm_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) += inst.imm8;
}

// MOV.W @(disp, PC), Rn
// 1001nnnndddddddd
void Sh4::inst_binary_movw_binind_disp_pc_gen(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + reg.pc + 4;
    int reg_no = inst.gen_reg;
    int16_t mem_in;

    read_mem<int16_t>(&mem_in, addr, sizeof(mem_in));
    *gen_reg(reg_no) = (int32_t)mem_in;
}

// MOV.L @(disp, PC), Rn
// 1101nnnndddddddd
void Sh4::inst_binary_movl_binind_disp_pc_gen(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + (reg.pc & ~3) + 4;
    int reg_no = inst.gen_reg;
    int32_t mem_in;

    read_mem(&mem_in, addr, sizeof(mem_in));
    *gen_reg(reg_no) = mem_in;
}

// MOV Rm, Rn
// 0110nnnnmmmm0011
void Sh4::inst_binary_movw_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) = *gen_reg(inst.src_reg);
}

// SWAP.B Rm, Rn
// 0110nnnnmmmm1000
void Sh4::inst_binary_swapb_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
void Sh4::inst_binary_swapw_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// XTRCT Rm, Rn
// 0110nnnnmmmm1101
void Sh4::inst_binary_xtrct_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// ADD Rm, Rn
// 0011nnnnmmmm1100
void Sh4::inst_binary_add_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) += *gen_reg(inst.src_reg);
}

// ADDC Rm, Rn
// 0011nnnnmmmm1110
void Sh4::inst_binary_addc_gen_gen(OpArgs inst) {
    // detect carry by doing 64-bit math
    boost::uint64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = gen_reg(inst.src_reg);
    dst_reg = gen_reg(inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst += in_src;

    unsigned carry_bit = ((in_dst & 0x100000000) >> 32) << SR_FLAG_T_SHIFT;
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= carry_bit;

    *dst_reg = in_dst;
}

// ADDV Rm, Rn
// 0011nnnnmmmm1111
void Sh4::inst_binary_addv_gen_gen(OpArgs inst) {
    // detect overflow using 64-bit math
    boost::int64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = gen_reg(inst.src_reg);
    dst_reg = gen_reg(inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst += in_src;

    unsigned overflow_bit = (in_dst != int32_t(in_dst)) << SR_FLAG_T_SHIFT;
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= overflow_bit;

    *dst_reg = in_dst;
}

// CMP/EQ Rm, Rn
// 0011nnnnmmmm0000
void Sh4::inst_binary_cmpeq_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= ((*gen_reg(inst.src_reg) == *gen_reg(inst.dst_reg)) <<
               SR_FLAG_T_SHIFT);
}

// CMP/HS Rm, Rn
// 0011nnnnmmmm0010
void Sh4::inst_binary_cmphs_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    uint32_t lhs = *gen_reg(inst.dst_reg);
    uint32_t rhs = *gen_reg(inst.src_reg);
    reg.sr |= ((lhs >= rhs) << SR_FLAG_T_SHIFT);
}

// CMP/GE Rm, Rn
// 0011nnnnmmmm0011
void Sh4::inst_binary_cmpge_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    int32_t lhs = *gen_reg(inst.dst_reg);
    int32_t rhs = *gen_reg(inst.src_reg);
    reg.sr |= ((lhs >= rhs) << SR_FLAG_T_SHIFT);
}

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
void Sh4::inst_binary_cmphi_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    uint32_t lhs = *gen_reg(inst.dst_reg);
    uint32_t rhs = *gen_reg(inst.src_reg);
    reg.sr |= ((lhs > rhs) << SR_FLAG_T_SHIFT);
}

// CMP/GT Rm, Rn
// 0011nnnnmmmm0111
void Sh4::inst_binary_cmpgt_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    int32_t lhs = *gen_reg(inst.dst_reg);
    int32_t rhs = *gen_reg(inst.src_reg);
    reg.sr |= ((lhs > rhs) << SR_FLAG_T_SHIFT);
}

// CMP/STR Rm, Rn
// 0010nnnnmmmm1100
void Sh4::inst_binary_cmpstr_gen_gen(OpArgs inst) {
    uint32_t lhs = *gen_reg(inst.dst_reg);
    uint32_t rhs = *gen_reg(inst.src_reg);
    uint32_t flag;

    flag = !!(((lhs & 0x000000ff) == (rhs & 0x000000ff)) ||
              ((lhs & 0x0000ff00) == (rhs & 0x0000ff00)) ||
              ((lhs & 0x00ff0000) == (rhs & 0x00ff0000)) ||
              ((lhs & 0xff000000) == (rhs & 0xff000000)));

    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= flag << SR_FLAG_T_SHIFT;
}

// DIV1 Rm, Rn
// 0011nnnnmmmm0100
void Sh4::inst_binary_div1_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// DIV0S Rm, Rn
// 0010nnnnmmmm0111
void Sh4::inst_binary_div0s_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// DMULS.L Rm, Rn
//0011nnnnmmmm1101
void Sh4::inst_binary_dmulsl_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// DMULU.L Rm, Rn
// 0011nnnnmmmm0101
void Sh4::inst_binary_dmulul_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// EXTS.B Rm, Rn
// 0110nnnnmmmm1110
void Sh4::inst_binary_extsb_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// EXTS.W Rm, Rnn
// 0110nnnnmmmm1111
void Sh4::inst_binary_extsw_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// EXTU.B Rm, Rn
// 0110nnnnmmmm1100
void Sh4::inst_binary_extub_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// EXTU.W Rm, Rn
// 0110nnnnmmmm1101
void Sh4::inst_binary_extuw_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MUL.L Rm, Rn
// 0000nnnnmmmm0111
void Sh4::inst_binary_mull_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MULS.W Rm, Rn
// 0010nnnnmmmm1111
void Sh4::inst_binary_mulsw_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MULU.W Rm, Rn
// 0010nnnnmmmm1110
void Sh4::inst_binary_muluw_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// NEG Rm, Rn
// 0110nnnnmmmm1011
void Sh4::inst_binary_neg_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// NEGC Rm, Rn
// 0110nnnnmmmm1010
void Sh4::inst_binary_negc_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SUB Rm, Rn
// 0011nnnnmmmm1000
void Sh4::inst_binary_sub_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) -= *gen_reg(inst.src_reg);
}

// SUBC Rm, Rn
// 0011nnnnmmmm1010
void Sh4::inst_binary_subc_gen_gen(OpArgs inst) {
    // detect carry by doing 64-bit math
    boost::uint64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = gen_reg(inst.src_reg);
    dst_reg = gen_reg(inst.dst_reg);

    in_src = *src_reg;
    in_dst = *dst_reg;

    assert(!(in_src & 0xffffffff00000000));
    assert(!(in_dst & 0xffffffff00000000));

    in_dst -= in_src;

    unsigned carry_bit = ((in_dst & 0x100000000) >> 32) << SR_FLAG_T_SHIFT;
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= carry_bit;

    *dst_reg = in_dst;
}

// SUBV Rm, Rn
// 0011nnnnmmmm1011
void Sh4::inst_binary_subv_gen_gen(OpArgs inst) {
    // detect overflow using 64-bit math
    boost::int64_t in_src, in_dst;
    reg32_t *src_reg, *dst_reg;

    src_reg = gen_reg(inst.src_reg);
    dst_reg = gen_reg(inst.dst_reg);

    // cast to int32_t instead of int64_t so it gets sign-extended
    // instead of zero-extended.
    in_src = int32_t(*src_reg);
    in_dst = int32_t(*dst_reg);

    in_dst -= in_src;

    unsigned overflow_bit = (in_dst > std::numeric_limits<int32_t>::max()) ||
        (in_dst < std::numeric_limits<int32_t>::min());
    reg.sr &= ~SR_FLAG_T_MASK;
    reg.sr |= overflow_bit;

    *dst_reg = in_dst;
}

// AND Rm, Rn
// 0010nnnnmmmm1001
void Sh4::inst_binary_and_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) &= *gen_reg(inst.src_reg);
}

// NOT Rm, Rn
// 0110nnnnmmmm0111
void Sh4::inst_binary_not_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) = ~(*gen_reg(inst.src_reg));
}

// OR Rm, Rn
// 0010nnnnmmmm1011
void Sh4::inst_binary_or_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) |= *gen_reg(inst.src_reg);
}

// TST Rm, Rn
// 0010nnnnmmmm1000
void Sh4::inst_binary_tst_gen_gen(OpArgs inst) {
    reg.sr &= ~SR_FLAG_T_MASK;
    reg32_t flag = !(*gen_reg(inst.src_reg) & *gen_reg(inst.dst_reg)) <<
        SR_FLAG_T_MASK;
    reg.sr |= flag;
}

// XOR Rm, Rn
// 0010nnnnmmmm1010
void Sh4::inst_binary_xor_gen_gen(OpArgs inst) {
    *gen_reg(inst.dst_reg) ^= *gen_reg(inst.src_reg);
}

// SHAD Rm, Rn
// 0100nnnnmmmm1100
void Sh4::inst_binary_shad_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// SHLD Rm, Rn
// 0100nnnnmmmm1101
void Sh4::inst_binary_shld_gen_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDC Rm, Rn_BANK
// 0100mmmm1nnn1110
void Sh4::inst_binary_ldc_gen_bank(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *bank_reg(inst.bank_reg) = *gen_reg(inst.gen_reg);
}

// LDC.L @Rm+, Rn_BANK
// 0100mmmm1nnn0111
void Sh4::inst_binary_ldcl_indgeninc_bank(OpArgs inst) {
    uint32_t val;
    reg32_t *src_reg;

#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    src_reg = gen_reg(inst.gen_reg);
    if (read_mem(&val, *src_reg, sizeof(val)) != 0) {
        return;
    }

    (*src_reg) += 4;
    *bank_reg(inst.bank_reg) = val;
}

// STC Rm_BANK, Rn
// 0000nnnn1mmm0010
void Sh4::inst_binary_stc_bank_gen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    *gen_reg(inst.gen_reg) = *bank_reg(inst.bank_reg);
}

// STC.L Rm_BANK, @-Rn
// 0100nnnn1mmm0011
void Sh4::inst_binary_stcl_bank_inddecgen(OpArgs inst) {
#ifdef ENABLE_SH4_MMU
    if (!(reg.sr & SR_MD_MASK))
        throw UnimplementedError("CPU exception for using a privileged "
                                 "exception in an unprivileged mode");
#endif

    reg32_t *addr_reg = gen_reg(inst.gen_reg);
    reg32_t src_val = *bank_reg(inst.bank_reg);
    addr32_t addr = *addr_reg - 4;

    if (write_mem(&src_val, addr, sizeof(src_val)) != 0)
        return;

    *addr_reg = addr;
}

// LDS Rm, MACH
// 0100mmmm00001010
void Sh4::inst_binary_lds_gen_mach(OpArgs inst) {
    reg.mach = *gen_reg(inst.gen_reg);
}

// LDS Rm, MACL
// 0100mmmm00011010
void Sh4::inst_binary_lds_gen_macl(OpArgs inst) {
    reg.macl = *gen_reg(inst.gen_reg);
}

// STS MACH, Rn
// 0000nnnn00001010
void Sh4::inst_binary_sts_mach_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) = reg.mach;
}

// STS MACL, Rn
// 0000nnnn00011010
void Sh4::inst_binary_sts_macl_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) = reg.macl;
}

// LDS Rm, PR
// 0100mmmm00101010
void Sh4::inst_binary_lds_gen_pr(OpArgs inst) {
    reg.pr = *gen_reg(inst.gen_reg);
}

// STS PR, Rn
// 0000nnnn00101010
void Sh4::inst_binary_sts_pr_gen(OpArgs inst) {
    *gen_reg(inst.gen_reg) = reg.pr;
}

// LDS.L @Rm+, MACH
// 0100mmmm00000110
void Sh4::inst_binary_ldsl_indgeninc_mach(OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = gen_reg(inst.gen_reg);

    if (read_mem(&val, *addr_reg, sizeof(val)) != 0)
        return;

    reg.mach = val;

    *addr_reg += 4;
}

// LDS.L @Rm+, MACL
// 0100mmmm00010110
void Sh4::inst_binary_ldsl_indgeninc_macl(OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = gen_reg(inst.gen_reg);

    if (read_mem(&val, *addr_reg, sizeof(val)) != 0)
        return;

    reg.macl = val;

    *addr_reg += 4;
}

// STS.L MACH, @-Rn
// 0100mmmm00000010
void Sh4::inst_binary_stsl_mach_inddecgen(OpArgs inst) {
    reg32_t *addr_reg = gen_reg(inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (write_mem(&reg.mach, addr, sizeof(reg.mach)) != 0)
        return;

    *addr_reg = addr;
}

// STS.L MACL, @-Rn
// 0100mmmm00010010
void Sh4::inst_binary_stsl_macl_inddecgen(OpArgs inst) {
    reg32_t *addr_reg = gen_reg(inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (write_mem(&reg.macl, addr, sizeof(reg.macl)) != 0)
        return;

    *addr_reg = addr;
}

// LDS.L @Rm+, PR
// 0100mmmm00100110
void Sh4::inst_binary_ldsl_indgeninc_pr(OpArgs inst) {
    uint32_t val;
    reg32_t *addr_reg = gen_reg(inst.gen_reg);

    if (read_mem(&val, *addr_reg, sizeof(val)) != 0)
        return;

    reg.pr = val;

    *addr_reg += 4;
}

// STS.L PR, @-Rn
// 0100nnnn00100010
void Sh4::inst_binary_stsl_pr_inddecgen(OpArgs inst) {
    reg32_t *addr_reg = gen_reg(inst.gen_reg);
    addr32_t addr = *addr_reg - 4;

    if (write_mem(&reg.pr, addr, sizeof(reg.pr)) != 0)
        return;

    *addr_reg = addr;
}

// MOV.B Rm, @Rn
// 0010nnnnmmmm0000
void Sh4::inst_binary_movb_gen_indgen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.dst_reg);
    uint8_t mem_val = *gen_reg(inst.src_reg);

    write_mem(&mem_val, addr, sizeof(mem_val));
}

// MOV.W Rm, @Rn
// 0010nnnnmmmm0001
void Sh4::inst_binary_movw_gen_indgen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.dst_reg);
    uint16_t mem_val = *gen_reg(inst.src_reg);

    write_mem(&mem_val, addr, sizeof(mem_val));
}

// MOV.L Rm, @Rn
// 0010nnnnmmmm0010
void Sh4::inst_binary_movl_gen_indgen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.dst_reg);
    uint32_t mem_val = *gen_reg(inst.src_reg);

    write_mem(&mem_val, addr, sizeof(mem_val));
}

// MOV.B @Rm, Rn
// 0110nnnnmmmm0000
void Sh4::inst_binary_movb_indgen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.src_reg);
    int8_t mem_val;

    read_mem(&mem_val, addr, sizeof(mem_val));

    *gen_reg(inst.dst_reg) = int32_t(mem_val);
}

// MOV.W @Rm, Rn
// 0110nnnnmmmm0001
void Sh4::inst_binary_movw_indgen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.src_reg);
    int16_t mem_val;

    read_mem(&mem_val, addr, sizeof(mem_val));

    *gen_reg(inst.dst_reg) = int32_t(mem_val);
}

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
void Sh4::inst_binary_movl_indgen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(inst.src_reg);
    int32_t mem_val;

    read_mem(&mem_val, addr, sizeof(mem_val));

    *gen_reg(inst.dst_reg) = mem_val;
}

// MOV.B Rm, @-Rn
// 0010nnnnmmmm0100
void Sh4::inst_binary_movb_gen_inddecgen(OpArgs inst) {
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    reg32_t *src_reg = gen_reg(inst.src_reg);
    int8_t val;

    (*dst_reg)--;
    val = *src_reg;
    write_mem(&val, *dst_reg, sizeof(val));
}

// MOV.W Rm, @-Rn
// 0010nnnnmmmm0101
void Sh4::inst_binary_movw_gen_inddecgen(OpArgs inst) {
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    reg32_t *src_reg = gen_reg(inst.src_reg);
    int16_t val;

    (*dst_reg) -= 2;
    val = *src_reg;
    write_mem(&val, *dst_reg, sizeof(val));
}

// MOV.L Rm, @-Rn
// 0010nnnnmmmm0110
void Sh4::inst_binary_movl_gen_inddecgen(OpArgs inst) {
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    reg32_t *src_reg = gen_reg(inst.src_reg);
    int32_t val;

    (*dst_reg) -= 4;
    val = *src_reg;
    write_mem(&val, *dst_reg, sizeof(val));
}

// MOV.B @Rm+, Rn
// 0110nnnnmmmm0100
void Sh4::inst_binary_movb_indgeninc_gen(OpArgs inst) {
    reg32_t *src_reg = gen_reg(inst.src_reg);
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    int8_t val;

    read_mem(&val, *src_reg, sizeof(val));
    *dst_reg = int32_t(val);

    (*src_reg)++;
}

// MOV.W @Rm+, Rn
// 0110nnnnmmmm0101
void Sh4::inst_binary_movw_indgeninc_gen(OpArgs inst) {
    reg32_t *src_reg = gen_reg(inst.src_reg);
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    int16_t val;

    read_mem(&val, *src_reg, sizeof(val));
    *dst_reg = int32_t(val);

    (*src_reg) += 2;
}

// MOV.L @Rm+, Rn
// 0110nnnnmmmm0110
void Sh4::inst_binary_movl_indgeninc_gen(OpArgs inst) {
    reg32_t *src_reg = gen_reg(inst.src_reg);
    reg32_t *dst_reg = gen_reg(inst.dst_reg);
    int32_t val;

    read_mem(&val, *src_reg, sizeof(val));
    *dst_reg = int32_t(val);

    (*src_reg) += 4;
}

// MAC.L @Rm+, @Rn+
// 0000nnnnmmmm1111
void Sh4::inst_binary_macl_indgeninc_indgeninc(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MAC.W @Rm+, @Rn+
// 0100nnnnmmmm1111
void Sh4::inst_binary_macw_indgeninc_indgeninc(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// MOV.B R0, @(disp, Rn)
// 10000000nnnndddd
void Sh4::inst_binary_movb_r0_binind_disp_gen(OpArgs inst) {
    addr32_t addr = inst.imm4 + *gen_reg(inst.base_reg_src);
    int8_t val = *gen_reg(0);

    write_mem(&val, addr, sizeof(val));
}

// MOV.W R0, @(disp, Rn)
// 10000001nnnndddd
void Sh4::inst_binary_movw_r0_binind_disp_gen(OpArgs inst) {
    addr32_t addr = (inst.imm4 << 1) + *gen_reg(inst.base_reg_src);
    int16_t val = *gen_reg(0);

    write_mem(&val, addr, sizeof(val));
}

// MOV.L Rm, @(disp, Rn)
// 0001nnnnmmmmdddd
void Sh4::inst_binary_movl_gen_binind_disp_gen(OpArgs inst) {
    addr32_t addr = (inst.imm4 << 2) + *gen_reg(inst.base_reg_dst);
    int32_t val = *gen_reg(inst.base_reg_src);

    write_mem(&val, addr, sizeof(val));
}

// MOV.B @(disp, Rm), R0
// 10000100mmmmdddd
void Sh4::inst_binary_movb_binind_disp_gen_r0(OpArgs inst) {
    addr32_t addr = inst.imm4 + *gen_reg(inst.base_reg_src);
    int8_t val;

    read_mem(&val, addr, sizeof(val));

    *gen_reg(0) = int32_t(val);
}

// MOV.W @(disp, Rm), R0
// 10000101mmmmdddd
void Sh4::inst_binary_movw_binind_disp_gen_r0(OpArgs inst) {
    addr32_t addr = (inst.imm4 << 1) + *gen_reg(inst.base_reg_src);
    int16_t val;

    read_mem(&val, addr, sizeof(val));

    *gen_reg(0) = int32_t(val);
}

// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
void Sh4::inst_binary_movl_binind_disp_gen_gen(OpArgs inst) {
    addr32_t addr = (inst.imm4 << 2) + *gen_reg(inst.base_reg_src);
    int32_t val;

    read_mem(&val, addr, sizeof(val));

    *gen_reg(inst.base_reg_dst) = val;
}

// MOV.B Rm, @(R0, Rn)
// 0000nnnnmmmm0100
void Sh4::inst_binary_movb_gen_binind_r0_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.dst_reg);
    uint8_t val = *gen_reg(inst.src_reg);

    write_mem(&val, addr, sizeof(val));
}

// MOV.W Rm, @(R0, Rn)
// 0000nnnnmmmm0101
void Sh4::inst_binary_movw_gen_binind_r0_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.dst_reg);
    uint16_t val = *gen_reg(inst.src_reg);

    write_mem(&val, addr, sizeof(val));
}

// MOV.L Rm, @(R0, Rn)
// 0000nnnnmmmm0110
void Sh4::inst_binary_movl_gen_binind_r0_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.dst_reg);
    uint32_t val = *gen_reg(inst.src_reg);

    write_mem(&val, addr, sizeof(val));
}

// MOV.B @(R0, Rm), Rn
// 0000nnnnmmmm1100
void Sh4::inst_binary_movb_binind_r0_gen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.src_reg);
    int8_t val;
    read_mem(&val, addr, sizeof(val));
    *gen_reg(inst.dst_reg) = int32_t(val);
}

// MOV.W @(R0, Rm), Rn
// 0000nnnnmmmm1101
void Sh4::inst_binary_movw_binind_r0_gen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.src_reg);
    int16_t val;

    read_mem(&val, addr, sizeof(val));
    *gen_reg(inst.dst_reg) = int32_t(val);
}

// MOV.L @(R0, Rm), Rn
// 0000nnnnmmmm1110
void Sh4::inst_binary_movl_binind_r0_gen_gen(OpArgs inst) {
    addr32_t addr = *gen_reg(0) + *gen_reg(inst.src_reg);
    int32_t val;

    read_mem(&val, addr, sizeof(val));
    *gen_reg(inst.dst_reg) = val;
}

// MOV.B R0, @(disp, GBR)
// 11000000dddddddd
void Sh4::inst_binary_movb_r0_binind_disp_gbr(OpArgs inst) {
    addr32_t addr = inst.imm8 + reg.gbr;
    int8_t val = *gen_reg(0);

    write_mem(&val, addr, sizeof(val));
}

// MOV.W R0, @(disp, GBR)
// 11000001dddddddd
void Sh4::inst_binary_movw_r0_binind_disp_gbr(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + reg.gbr;
    int16_t val = *gen_reg(0);

    write_mem(&val, addr, sizeof(val));
}

// MOV.L R0, @(disp, GBR)
// 11000010dddddddd
void Sh4::inst_binary_movl_r0_binind_disp_gbr(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + reg.gbr;
    int32_t val = *gen_reg(0);

    write_mem(&val, addr, sizeof(val));
}

// MOV.B @(disp, GBR), R0
// 11000100dddddddd
void Sh4::inst_binary_movb_binind_disp_gbr_r0(OpArgs inst) {
    addr32_t addr = inst.imm8 + reg.gbr;
    int8_t val;

    read_mem(&val, addr, sizeof(val));
    *gen_reg(0) = int32_t(val);
}

// MOV.W @(disp, GBR), R0
// 11000101dddddddd
void Sh4::inst_binary_movw_binind_disp_gbr_r0(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 1) + reg.gbr;
    int16_t val;

    read_mem(&val, addr, sizeof(val));
    *gen_reg(0) = int32_t(val);
}

// MOV.L @(disp, GBR), R0
// 11000110dddddddd
void Sh4::inst_binary_movl_binind_disp_gbr_r0(OpArgs inst) {
    addr32_t addr = (inst.imm8 << 2) + reg.gbr;
    int32_t val;

    read_mem(&val, addr, sizeof(val));
    *gen_reg(0) = val;
}

// MOVA @(disp, PC), R0
// 11000111dddddddd
void Sh4::inst_binary_mova_binind_disp_pc_r0(OpArgs inst) {
    /*
     * The assembly for this one is a bit of a misnomer.
     * even though it has the @ indirection symbol around (disp, PC), it
     * actually just loads that address into R0 instead of the value at that
     * address.  It is roughly analagous to the x86 architectures lea family of
     * opcodes.
     */
    *gen_reg(0) = (inst.imm8 << 2) + (reg.pc & ~3) + 4;
}

// MOVCA.L R0, @Rn
// 0000nnnn11000011
void Sh4::inst_binary_movcal_r0_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FLDI0 FRn
// 1111nnnn10001101
void Sh4::inst_unary_fldi0_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FLDI1 Frn
// 1111nnnn10011101
void Sh4::inst_unary_fldi1_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV FRm, FRn
// 1111nnnnmmmm1100
void Sh4::inst_binary_fmov_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S @Rm, FRn
// 1111nnnnmmmm1000
void Sh4::inst_binary_fmovs_indgen_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S @(R0,Rm), FRn
// 1111nnnnmmmm0110
void Sh4::inst_binary_fmovs_binind_r0_gen_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S @Rm+, FRn
// 1111nnnnmmmm1001
void Sh4::inst_binary_fmovs_indgeninc_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S FRm, @Rn
// 1111nnnnmmmm1010
void Sh4::inst_binary_fmovs_fr_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S FRm, @-Rn
// 1111nnnnmmmm1011
void Sh4::inst_binary_fmovs_fr_inddecgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV.S FRm, @(R0, Rn)
// 1111nnnnmmmm0111
void Sh4::inst_binary_fmovs_fr_binind_r0_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV DRm, DRn
// 1111nnn0mmm01100
void Sh4::inst_binary_fmov_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @Rm, DRn
// 1111nnn0mmmm1000
void Sh4::inst_binary_fmov_indgen_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @(R0, Rm), DRn
// 1111nnn0mmmm0110
void Sh4::inst_binary_fmov_binind_r0_gen_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @Rm+, DRn
// 1111nnn0mmmm1001
void Sh4::inst_binary_fmov_indgeninc_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV DRm, @Rn
// 1111nnnnmmm01010
void Sh4::inst_binary_fmov_dr_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV DRm, @-Rn
// 1111nnnnmmm01011
void Sh4::inst_binary_fmov_dr_inddecgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV DRm, @(R0,Rn)
// 1111nnnnmmm00111
void Sh4::inst_binary_fmov_dr_binind_r0_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FLDS FRm, FPUL
// 1111mmmm00011101
void Sh4::inst_binary_flds_fr_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSTS FPUL, FRn
// 1111nnnn00001101
void Sh4::inst_binary_fsts_fpul_fp(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FABS FRn
// 1111nnnn01011101
void Sh4::inst_unary_fabs_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FADD FRm, FRn
// 1111nnnnmmmm0000
void Sh4::inst_binary_fadd_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCMP/EQ FRm, FRn
// 1111nnnnmmmm0100
void Sh4::inst_binary_fcmpeq_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCMP/GT FRm, FRn
// 1111nnnnmmmm0101
void Sh4::inst_binary_fcmpgt_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FDIV FRm, FRn
// 1111nnnnmmmm0011
void Sh4::inst_binary_fdiv_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FLOAT FPUL, FRn
// 1111nnnn00101101
void Sh4::inst_binary_float_fpul_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMAC FR0, FRm, FRn
// 1111nnnnmmmm1110
void Sh4::inst_trinary_fmac_fr0_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMUL FRm, FRn
// 1111nnnnmmmm0010
void Sh4::inst_binary_fmul_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FNEG FRn
// 1111nnnn01001101
void Sh4::inst_unary_fneg_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSQRT FRn
// 1111nnnn01101101
void Sh4::inst_unary_fsqrt_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSUB FRm, FRn
// 1111nnnnmmmm0001
void Sh4::inst_binary_fsub_fr_fr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FTRC FRm, FPUL
// 1111mmmm00111101
void Sh4::inst_binary_ftrc_fr_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FABS DRn
// 1111nnn001011101
void Sh4::inst_unary_fabs_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FADD DRm, DRn
// 1111nnn0mmm00000
void Sh4::inst_binary_fadd_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCMP/EQ DRm, DRn
// 1111nnn0mmm00100
void Sh4::inst_binary_fcmpeq_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCMP/GT DRm, DRn
// 1111nnn0mmm00101
void Sh4::inst_binary_fcmpgt_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FDIV DRm, DRn
// 1111nnn0mmm00011
void Sh4::inst_binary_fdiv_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCNVDS DRm, FPUL
// 1111mmm010111101
void Sh4::inst_binary_fcnvds_dr_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FCNVSD FPUL, DRn
// 1111nnn010101101
void Sh4::inst_binary_fcnvsd_fpul_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FLOAT FPUL, DRn
// 1111nnn000101101
void Sh4::inst_binary_float_fpul_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMUL DRm, DRn
// 1111nnn0mmm00010
void Sh4::inst_binary_fmul_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FNEG DRn
// 1111nnn001001101
void Sh4::inst_unary_fneg_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSQRT DRn
// 1111nnn001101101
void Sh4::inst_unary_fsqrt_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FSUB DRm, DRn
// 1111nnn0mmm00001
void Sh4::inst_binary_fsub_dr_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FTRC DRm, FPUL
// 1111mmm000111101
void Sh4::inst_binary_ftrc_dr_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDS Rm, FPSCR
// 0100mmmm01101010
void Sh4::inst_binary_lds_gen_fpscr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDS Rm, FPUL
// 0100mmmm01011010
void Sh4::inst_binary_gen_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDS.L @Rm+, FPSCR
// 0100mmmm01100110
void Sh4::inst_binary_ldsl_indgeninc_fpscr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// LDS.L @Rm+, FPUL
// 0100mmmm01010110
void Sh4::inst_binary_ldsl_indgeninc_fpul(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// STS FPSCR, Rn
// 0000nnnn01101010
void Sh4::inst_binary_sts_fpscr_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// STS FPUL, Rn
// 0000nnnn01011010
void Sh4::inst_binary_sts_fpul_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// STS.L FPSCR, @-Rn
// 0100nnnn01100010
void Sh4::inst_binary_stsl_fpscr_inddecgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// STS.L FPUL, @-Rn
// 0100nnnn01010010
void Sh4::inst_binary_stsl_fpul_inddecgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV DRm, XDn
// 1111nnn1mmm01100
void Sh4::inst_binary_fmove_dr_xd(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV XDm, DRn
// 1111nnn0mmm11100
void Sh4::inst_binary_fmov_xd_dr(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV XDm, XDn
// 1111nnn1mmm11100
void Sh4::inst_binary_fmov_xd_xd(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @Rm, XDn
// 1111nnn1mmmm1000
void Sh4::inst_binary_fmov_indgen_xd(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @Rm+, XDn
// 1111nnn1mmmm1001
void Sh4::inst_binary_fmov_indgeninc_xd(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV @(R0, Rn), XDn
// 1111nnn1mmmm0110
void Sh4::inst_binary_fmov_binind_r0_gen_xd(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV XDm, @Rn
// 1111nnnnmmm11010
void Sh4::inst_binary_fmov_xd_indgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV XDm, @-Rn
// 1111nnnnmmm11011
void Sh4::inst_binary_fmov_xd_inddecgen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FMOV XDm, @(R0, Rn)
// 1111nnnnmmm10111
void Sh4::inst_binary_fmov_xs_binind_r0_gen(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FIPR FVm, FVn - vector dot product
// 1111nnmm11101101
void Sh4::inst_binary_fipr_fv_fv(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}

// FTRV MXTRX, FVn - multiple vector by matrix
// 1111nn0111111101
void Sh4::inst_binary_fitrv_mxtrx_fv(OpArgs inst) {
    throw UnimplementedError("Instruction handler");
}
