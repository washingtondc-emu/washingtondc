/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include <assert.h>
#include <fenv.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "washdc/error.h"
#include "dreamcast.h"
#include "sh4_ocache.h"
#include "sh4.h"
#include "sh4_mem.h"
#include "sh4_tbl.h"
#include "sh4_excp.h"
#include "sh4_jit.h"
#include "log.h"
#include "intmath.h"

#ifdef ENABLE_DEBUGGER
#include "washdc/debugger.h"
#endif

#ifdef DEEP_SYSCALL_TRACE
#include "deep_syscall_trace.h"
#endif

#include "sh4_inst.h"

static DEF_ERROR_STRING_ATTR(opcode_format)
static DEF_ERROR_STRING_ATTR(opcode_name)
static DEF_ERROR_INT_ATTR(instruction)
static DEF_ERROR_INT_ATTR(instruction_mask)
static DEF_ERROR_INT_ATTR(instruction_expect)
static DEF_ERROR_U32_ATTR(fpscr)
static DEF_ERROR_U32_ATTR(fpscr_expect)
static DEF_ERROR_U32_ATTR(fpscr_mask)
static DEF_ERROR_INT_ATTR(inst_bin)

static inline uint16_t inst_imm8(int inst) {
    return inst & 0xff;
}

static inline int8_t inst_simm8(int inst) {
    return inst & 0xff;
}

static inline int16_t inst_simm12(int inst) {
    int16_t ret = inst & 0xfff;
    return ((int16_t)(ret << 4)) >> 4;
}

static inline uint16_t inst_imm4(int inst) {
    return inst & 0xf;
}

#ifdef SH4_FPU_PEDANTIC
/*
 * set the FPU's invalid operation flag in FPSCR and maybe raise an exception
 *
 * dst is the destination register index for the operation.
 * It will be set to qNaN if the exceptions are disabled.
 */
static void sh4_fr_invalid(Sh4 *sh4, unsigned dst_reg);
#endif

static InstOpcode const* sh4_decode_inst_slow(cpu_inst_param inst);

#ifdef INVARIANTS
#define CHECK_INST(inst, mask, val) \
    do_check_inst(inst, mask, val, __LINE__, __FILE__, __func__)

static void do_check_inst(cpu_inst_param inst, uint16_t mask, uint16_t val,
                          int line_no, char const *file_name,
                          char const *func_name) {
    if ((inst & mask) != val) {
        error_set_instruction(inst);
        error_set_instruction_mask(mask);
        error_set_instruction_expect(val);
        error_set_line(line_no);
        error_set_file(file_name);
        error_set_function(func_name);
        error_raise(ERROR_INTEGRITY);
    }
}

#define CHECK_FPSCR(fpscr, mask, expect) \
    do_check_fpscr(fpscr, mask, expect, __LINE__, __FILE__, __func__)

static void do_check_fpscr(reg32_t fpscr, reg32_t mask, reg32_t expect,
                           int line_no, char const *file_name,
                           char const *func_name) {
    if ((fpscr & mask) != expect) {
        error_set_fpscr(fpscr);
        error_set_fpscr_mask(mask);
        error_set_fpscr_expect(expect);
        error_set_line(line_no);
        error_set_file(file_name);
        error_set_function(func_name);
        error_raise(ERROR_INTEGRITY);
    }
}

#else

#define CHECK_INST(inst, mask, val)

#define CHECK_FPSCR(fpscr, mask, expect)

#endif

static struct InstOpcode opcode_list[] = {
    // RTS
    { &sh4_inst_rts, sh4_jit_rts, true, SH4_GROUP_CO, 2, 0xffff, 0x000b },

    // CLRMAC
    { &sh4_inst_clrmac, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xffff, 0x0028 },

    // CLRS
    { &sh4_inst_clrs, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xffff, 0x0048 },

    // CLRT
    { &sh4_inst_clrt, sh4_jit_clrt, false,
      SH4_GROUP_MT, 1, 0xffff, 0x0008 },

    // LDTLB
    { &sh4_inst_ldtlb, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xffff, 0x0038 },

    // NOP
    { &sh4_inst_nop, sh4_jit_nop, false,
      SH4_GROUP_MT, 1, 0xffff, 0x0009 },

    // RTE
    { &sh4_inst_rte, sh4_jit_rte, true, SH4_GROUP_CO, 5, 0xffff, 0x002b },

    // SETS
    { &sh4_inst_sets, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xffff, 0x0058 },

    // SETT
    { &sh4_inst_sett, sh4_jit_sett, false,
      SH4_GROUP_MT, 1, 0xffff, 0x0018 },

    // SLEEP
    { &sh4_inst_sleep, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xffff, 0x001b },

    // FRCHG
    { &sh4_inst_frchg, sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xffff, 0xfbfd },

    // FSCHG
    { &sh4_inst_fschg, sh4_jit_fschg, false,
      SH4_GROUP_FE, 1, 0xffff, 0xf3fd },

    // MOVT Rn
    { &sh4_inst_unary_movt_gen, sh4_jit_movt, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x0029 },

    // CMP/PZ
    { &sh4_inst_unary_cmppz_gen, sh4_jit_cmppz_rn, false,
      SH4_GROUP_MT, 1, 0xf0ff, 0x4011 },

    // CMP/PL
    { &sh4_inst_unary_cmppl_gen, sh4_jit_cmppl_rn, false,
      SH4_GROUP_MT, 1, 0xf0ff, 0x4015 },

    // DT
    { &sh4_inst_unary_dt_gen, sh4_jit_dt_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4010 },

    // ROTL Rn
    { &sh4_inst_unary_rotl_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4004 },

    // ROTR Rn
    { &sh4_inst_unary_rotr_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4005 },

    // ROTCL Rn
    { &sh4_inst_unary_rotcl_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4024 },

    // ROTCR Rn
    { &sh4_inst_unary_rotcr_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4025 },

    // SHAL Rn
    { &sh4_inst_unary_shal_gen, sh4_jit_shal_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4020 },

    // SHAR Rn
    { &sh4_inst_unary_shar_gen, sh4_jit_shar_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4021 },

    // SHLL Rn
    { &sh4_inst_unary_shll_gen, sh4_jit_shll_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4000 },

    // SHLR Rn
    { &sh4_inst_unary_shlr_gen, sh4_jit_shlr_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4001 },

    // SHLL2 Rn
    { &sh4_inst_unary_shll2_gen, sh4_jit_shll2_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4008 },

    // SHLR2 Rn
    { &sh4_inst_unary_shlr2_gen, sh4_jit_shlr2_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4009 },

    // SHLL8 Rn
    { &sh4_inst_unary_shll8_gen, sh4_jit_shll8_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4018 },

    // SHLR8 Rn
    { &sh4_inst_unary_shlr8_gen, sh4_jit_shlr8_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4019 },

    // SHLL16 Rn
    { &sh4_inst_unary_shll16_gen, sh4_jit_shll16_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4028 },

    // SHLR16 Rn
    { &sh4_inst_unary_shlr16_gen, sh4_jit_shlr16_rn, false,
      SH4_GROUP_EX, 1, 0xf0ff, 0x4029 },

    // BRAF Rn
    { &sh4_inst_unary_braf_gen, sh4_jit_braf_rn, true, SH4_GROUP_CO, 2, 0xf0ff, 0x0023 },

    // BSRF Rn
    { &sh4_inst_unary_bsrf_gen, sh4_jit_bsrf_rn, true, SH4_GROUP_CO, 2, 0xf0ff, 0x0003 },

    // CMP/EQ #imm, R0
    { &sh4_inst_binary_cmpeq_imm_r0, sh4_jit_fallback, false,
      SH4_GROUP_MT, 1, 0xff00, 0x8800 },

    // AND.B #imm, @(R0, GBR)
    { &sh4_inst_binary_andb_imm_r0_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xff00, 0xcd00 },

    // AND #imm, R0
    { &sh4_inst_binary_and_imm_r0, sh4_inst_binary_andb_imm_r0, false,
      SH4_GROUP_EX, 1, 0xff00, 0xc900 },

    // OR.B #imm, @(R0, GBR)
    { &sh4_inst_binary_orb_imm_r0_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xff00, 0xcf00 },

    // OR #imm, R0
    { &sh4_inst_binary_or_imm_r0, sh4_jit_or_imm8_r0, false,
      SH4_GROUP_EX, 1, 0xff00, 0xcb00 },

    // TST #imm, R0
    { &sh4_inst_binary_tst_imm_r0, sh4_jit_tst_imm8_r0, false,
      SH4_GROUP_MT, 1, 0xff00, 0xc800 },

    // TST.B #imm, @(R0, GBR)
    { &sh4_inst_binary_tstb_imm_r0_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 3, 0xff00, 0xcc00 },

    // XOR #imm, R0
    { &sh4_inst_binary_xor_imm_r0, sh4_jit_xor_imm8_r0, false,
      SH4_GROUP_EX, 1, 0xff00, 0xca00 },

    // XOR.B #imm, @(R0, GBR)
    { &sh4_inst_binary_xorb_imm_r0_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xff00, 0xce00 },

    // BF label
    { &sh4_inst_unary_bf_disp, sh4_jit_bf, true,
      SH4_GROUP_BR, 1, 0xff00, 0x8b00 },

    // BF/S label
    { &sh4_inst_unary_bfs_disp, sh4_jit_bfs, true,
      SH4_GROUP_BR, 1, 0xff00, 0x8f00 },

    // BT label
    { &sh4_inst_unary_bt_disp, sh4_jit_bt, true,
      SH4_GROUP_BR, 1, 0xff00, 0x8900 },

    // BT/S label
    { &sh4_inst_unary_bts_disp, sh4_jit_bts, true,
      SH4_GROUP_BR, 1, 0xff00, 0x8d00 },

    // BRA label
    { &sh4_inst_unary_bra_disp, sh4_jit_bra, true,
      SH4_GROUP_BR, 1, 0xf000, 0xa000 },

    // BSR label
    { &sh4_inst_unary_bsr_disp, sh4_jit_bsr, true,
      SH4_GROUP_BR, 1, 0xf000, 0xb000 },

    // TRAPA #immed
    { &sh4_inst_unary_trapa_disp, sh4_jit_fallback, false,
      SH4_GROUP_CO, 7, 0xff00, 0xc300 },

    // TAS.B @Rn
    { &sh4_inst_unary_tasb_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 5, 0xf0ff, 0x401b },

    // OCBI @Rn
    { &sh4_inst_unary_ocbi_indgen, sh4_jit_ocbi_arn, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x0093 },

    // OCBP @Rn
    { &sh4_inst_unary_ocbp_indgen, sh4_jit_ocbp_arn, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x00a3 },

    // OCBWB @Rn
    { &sh4_inst_unary_ocbwb_indgen, sh4_jit_ocbwb_arn, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x00b3 },

    // PREF @Rn
    { &sh4_inst_unary_pref_indgen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x0083 },

    // JMP @Rn
    { &sh4_inst_unary_jmp_indgen, sh4_jit_jmp_arn, true,
      SH4_GROUP_CO, 2, 0xf0ff, 0x402b },

    // JSR @Rn
    { &sh4_inst_unary_jsr_indgen, sh4_jit_jsr_arn, true, SH4_GROUP_CO, 2, 0xf0ff, 0x400b },

    // LDC Rm, SR
    { &sh4_inst_binary_ldc_gen_sr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xf0ff, 0x400e },

    // LDC Rm, GBR
    { &sh4_inst_binary_ldc_gen_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 3, 0xf0ff, 0x401e },

    // LDC Rm, VBR
    { &sh4_inst_binary_ldc_gen_vbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x402e },

    // LDC Rm, SSR
    { &sh4_inst_binary_ldc_gen_ssr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x403e },

    // LDC Rm, SPC
    { &sh4_inst_binary_ldc_gen_spc, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x404e },

    // LDC Rm, DBR
    { &sh4_inst_binary_ldc_gen_dbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x40fa },

    // STC SR, Rn
    { &sh4_inst_binary_stc_sr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x0002 },

    // STC GBR, Rn
    { &sh4_inst_binary_stc_gbr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x0012 },

    // STC VBR, Rn
    { &sh4_inst_binary_stc_vbr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x0022 },

    // STC SSR, Rn
    { &sh4_inst_binary_stc_ssr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x0032 },

    // STC SPC, Rn
    { &sh4_inst_binary_stc_spc_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x0042 },

    // STC SGR, Rn
    { &sh4_inst_binary_stc_sgr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 3, 0xf0ff, 0x003a },

    // STC DBR, Rn
    { &sh4_inst_binary_stc_dbr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x00fa },

    // LDC.L @Rm+, SR
    { &sh4_inst_binary_ldcl_indgeninc_sr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 4, 0xf0ff, 0x4007 },

    // LDC.L @Rm+, GBR
    { &sh4_inst_binary_ldcl_indgeninc_gbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 3, 0xf0ff, 0x4017 },

    // LDC.L @Rm+, VBR
    { &sh4_inst_binary_ldcl_indgeninc_vbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4027 },

    // LDC.L @Rm+, SSR
    { &sh4_inst_binary_ldcl_indgenic_ssr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4037 },

    // LDC.L @Rm+, SPC
    { &sh4_inst_binary_ldcl_indgeninc_spc, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4047 },

    // LDC.L @Rm+, DBR
    { &sh4_inst_binary_ldcl_indgeninc_dbr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x40f6 },

    // STC.L SR, @-Rn
    { &sh4_inst_binary_stcl_sr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4003 },

    // STC.L GBR, @-Rn
    { &sh4_inst_binary_stcl_gbr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4013 },

    // STC.L VBR, @-Rn
    { &sh4_inst_binary_stcl_vbr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4023 },

    // STC.L SSR, @-Rn
    { &sh4_inst_binary_stcl_ssr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4033 },

    // STC.L SPC, @-Rn
    { &sh4_inst_binary_stcl_spc_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4043 },

    // STC.L SGR, @-Rn
    { &sh4_inst_binary_stcl_sgr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 3, 0xf0ff, 0x4032 },

    // STC.L DBR, @-Rn
    { &sh4_inst_binary_stcl_dbr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x40f2 },

    // MOV #imm, Rn
    { &sh4_inst_binary_mov_imm_gen, sh4_jit_mov_imm8_rn, false,
      SH4_GROUP_EX, 1, 0xf000, 0xe000 },

    // ADD #imm, Rn
    { &sh4_inst_binary_add_imm_gen, sh4_jit_add_imm_rn, false,
      SH4_GROUP_EX, 1, 0xf000, 0x7000 },

    // MOV.W @(disp, PC), Rn
    { &sh4_inst_binary_movw_binind_disp_pc_gen, sh4_jit_movw_a_disp_pc_rn,
      true, SH4_GROUP_LS, 1, 0xf000, 0x9000 },

    // MOV.L @(disp, PC), Rn
    { &sh4_inst_binary_movl_binind_disp_pc_gen, sh4_jit_movl_a_disp_pc_rn,
      true, SH4_GROUP_LS, 1, 0xf000, 0xd000 },

    // MOV Rm, Rn
    { &sh4_inst_binary_mov_gen_gen, sh4_jit_mov_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x6003 },

    // SWAP.B Rm, Rn
    { &sh4_inst_binary_swapb_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x6008 },

    // SWAP.W Rm, Rn
    { &sh4_inst_binary_swapw_gen_gen, sh4_jit_swapw_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x6009 },

    // XTRCT Rm, Rn
    { &sh4_inst_binary_xtrct_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x200d },

    // ADD Rm, Rn
    { &sh4_inst_binary_add_gen_gen, sh4_jit_add_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x300c },

    // ADDC Rm, Rn
    { &sh4_inst_binary_addc_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x300e },

    // ADDV Rm, Rn
    { &sh4_inst_binary_addv_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x300f },

    // CMP/EQ Rm, Rn
    { &sh4_inst_binary_cmpeq_gen_gen, sh4_jit_cmpeq_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x3000 },

    // CMP/HS Rm, Rn
    { &sh4_inst_binary_cmphs_gen_gen, sh4_jit_cmphs_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x3002 },

    // CMP/GE Rm, Rn
    { &sh4_inst_binary_cmpge_gen_gen, sh4_jit_cmpge_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x3003 },

    // CMP/HI Rm, Rn
    { &sh4_inst_binary_cmphi_gen_gen, sh4_jit_cmphi_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x3006 },

    // CMP/GT Rm, Rn
    { &sh4_inst_binary_cmpgt_gen_gen, sh4_jit_cmpgt_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x3007 },

    // CMP/STR Rm, Rn
    { &sh4_inst_binary_cmpstr_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x200c },

    // DIV1 Rm, Rn
    { &sh4_inst_binary_div1_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x3004 },

    // DIV0S Rm, Rn
    { &sh4_inst_binary_div0s_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x2007 },

    // DIV0U
    { &sh4_inst_noarg_div0u, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xffff, 0x0019 },

    // DMULS.L Rm, Rn
    { &sh4_inst_binary_dmulsl_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf00f, 0x300d },

    // DMULU.L Rm, Rn
    { &sh4_inst_binary_dmulul_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf00f, 0x3005 },

    // EXTS.B Rm, Rn
    { &sh4_inst_binary_extsb_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600e },

    // EXTS.W Rm, Rn
    { &sh4_inst_binary_extsw_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600f },

    // EXTU.B Rm, Rn
    { &sh4_inst_binary_extub_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600c },

    // EXTU.W Rm, Rn
    { &sh4_inst_binary_extuw_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600d },

    // MUL.L Rm, Rn
    { &sh4_inst_binary_mull_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf00f, 0x0007 },

    // MULS.W Rm, Rn
    { &sh4_inst_binary_mulsw_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf00f, 0x200f },

    // MULU.W Rm, Rn
    { &sh4_inst_binary_muluw_gen_gen, sh4_jit_muluw_rm_rn, false,
      SH4_GROUP_CO, 2, 0xf00f, 0x200e },

    // NEG Rm, Rn
    { &sh4_inst_binary_neg_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600b },

    // NEGC Rm, Rn
    { &sh4_inst_binary_negc_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x600a },

    // SUB Rm, Rn
    { &sh4_inst_binary_sub_gen_gen, sh4_jit_sub_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x3008 },

    // SUBC Rm, Rn
    { &sh4_inst_binary_subc_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x300a },

    // SUBV Rm, Rn
    { &sh4_inst_binary_subv_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x300b },

    // AND Rm, Rn
    { &sh4_inst_binary_and_gen_gen, sh4_jit_and_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x2009 },

    // NOT Rm, Rn
    { &sh4_inst_binary_not_gen_gen, sh4_jit_not_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x6007 },

    // OR Rm, Rn
    { &sh4_inst_binary_or_gen_gen, sh4_jit_or_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x200b },

    // TST Rm, Rn
    { &sh4_inst_binary_tst_gen_gen, sh4_jit_tst_rm_rn, false,
      SH4_GROUP_MT, 1, 0xf00f, 0x2008 },

    // XOR Rm, Rn
    { &sh4_inst_binary_xor_gen_gen, sh4_jit_xor_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x200a },

    // SHAD Rm, Rn
    { &sh4_inst_binary_shad_gen_gen, sh4_jit_shad_rm_rn, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x400c },

    // SHLD Rm, Rn
    { &sh4_inst_binary_shld_gen_gen, sh4_jit_fallback, false,
      SH4_GROUP_EX, 1, 0xf00f, 0x400d },

    // LDC Rm, Rn_BANK
    { &sh4_inst_binary_ldc_gen_bank, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf08f, 0x408e },

    // LDC.L @Rm+, Rn_BANK
    { &sh4_inst_binary_ldcl_indgeninc_bank, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf08f, 0x4087 },

    // STC Rm_BANK, Rn
    { &sh4_inst_binary_stc_bank_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf08f, 0x0082 },

    // STC.L Rm_BANK, @-Rn
    { &sh4_inst_binary_stcl_bank_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf08f, 0x4083 },

    // LDS Rm, MACH
    { &sh4_inst_binary_lds_gen_mach, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x400a },

    // LDS Rm, MACL
    { &sh4_inst_binary_lds_gen_macl, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x401a },

    // STS MACH, Rn
    { &sh4_inst_binary_sts_mach_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x000a },

    // STS MACL, Rn
    { &sh4_inst_binary_sts_macl_gen, sh4_jit_sts_macl_rn, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x001a },

    // LDS Rm, PR
    { &sh4_inst_binary_lds_gen_pr, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x402a },

    // STS PR, Rn
    { &sh4_inst_binary_sts_pr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x002a },

    // LDS.L @Rm+, MACH
    { &sh4_inst_binary_ldsl_indgeninc_mach, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4006 },

    // LDS.L @Rm+, MACL
    { &sh4_inst_binary_ldsl_indgeninc_macl, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4016 },

    // STS.L MACH, @-Rn
    { &sh4_inst_binary_stsl_mach_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4002 },

    // STS.L MACL, @-Rn
    { &sh4_inst_binary_stsl_macl_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4012 },

    // LDS.L @Rm+, PR
    { &sh4_inst_binary_ldsl_indgeninc_pr, sh4_jit_ldsl_armp_pr, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4026 },

    // STS.L PR, @-Rn
    { &sh4_inst_binary_stsl_pr_inddecgen, sh4_jit_stsl_pr_amrn, false,
      SH4_GROUP_CO, 2, 0xf0ff, 0x4022 },

    // MOV.B Rm, @Rn
    { &sh4_inst_binary_movb_gen_indgen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2000 },

    // MOV.W Rm, @Rn
    { &sh4_inst_binary_movw_gen_indgen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2001 },

    // MOV.L Rm, @Rn
    { &sh4_inst_binary_movl_gen_indgen, sh4_jit_movl_rm_arn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2002 },

    // MOV.B @Rm, Rn
    { &sh4_inst_binary_movb_indgen_gen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6000 },

    // MOV.W @Rm, Rn
    { &sh4_inst_binary_movw_indgen_gen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6001 },

    // MOV.L @Rm, Rn
    { &sh4_inst_binary_movl_indgen_gen, sh4_jit_movl_arm_rn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6002 },

    // MOV.B Rm, @-Rn
    { &sh4_inst_binary_movb_gen_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2004 },

    // MOV.W Rm, @-Rn
    { &sh4_inst_binary_movw_gen_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2005 },

    // MOV.L Rm, @-Rn
    { &sh4_inst_binary_movl_gen_inddecgen, sh4_jit_movl_rm_amrn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x2006 },

    // MOV.B @Rm+, Rn
    { &sh4_inst_binary_movb_indgeninc_gen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6004 },

    // MOV.W @Rm+, Rn
    { &sh4_inst_binary_movw_indgeninc_gen, sh4_jit_movw_armp_rn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6005 },

    // MOV.L @Rm+, Rn
    { &sh4_inst_binary_movl_indgeninc_gen, sh4_jit_movl_armp_rn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0x6006 },

    // MAC.L @Rm+, @Rn+
    { &sh4_inst_binary_macl_indgeninc_indgeninc, sh4_jit_fallback,
      false, SH4_GROUP_CO, 2, 0xf00f, 0x000f },

    // MAC.W @Rm+, @Rn+
    { &sh4_inst_binary_macw_indgeninc_indgeninc, sh4_jit_fallback,
      false, SH4_GROUP_CO, 2, 0xf00f, 0x400f },

    // MOV.B R0, @(disp, Rn)
    { &sh4_inst_binary_movb_r0_binind_disp_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0x8000 },

    // MOV.W R0, @(disp, Rn)
    { &sh4_inst_binary_movw_r0_binind_disp_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0x8100 },

    // MOV.L Rm, @(disp, Rn)
    { &sh4_inst_binary_movl_gen_binind_disp_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf000, 0x1000 },

    // MOV.B @(disp, Rm), R0
    { &sh4_inst_binary_movb_binind_disp_gen_r0, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0x8400 },

    // MOV.W @(disp, Rm), R0
    { &sh4_inst_binary_movw_binind_disp_gen_r0, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0x8500 },

    // MOV.L @(disp, Rm), Rn
    { &sh4_inst_binary_movl_binind_disp_gen_gen, sh4_jit_movl_a_disp4_rm_rn,
      false, SH4_GROUP_LS, 1, 0xf000, 0x5000 },

    // MOV.B Rm, @(R0, Rn)
    { &sh4_inst_binary_movb_gen_binind_r0_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x0004 },

    // MOV.W Rm, @(R0, Rn)
    { &sh4_inst_binary_movw_gen_binind_r0_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x0005 },

    // MOV.L Rm, @(R0, Rn)
    { &sh4_inst_binary_movl_gen_binind_r0_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x0006 },

    // MOV.B @(R0, Rm), Rn
    { &sh4_inst_binary_movb_binind_r0_gen_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x000c },

    // MOV.W @(R0, Rm), Rn
    { &sh4_inst_binary_movw_binind_r0_gen_gen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x000d },

    // MOV.L @(R0, Rm), Rn
    { &sh4_inst_binary_movl_binind_r0_gen_gen, sh4_jit_movl_a_r0_rm_rn,
      false, SH4_GROUP_LS, 1, 0xf00f, 0x000e },

    // MOV.B R0, @(disp, GBR)
    { &sh4_inst_binary_movb_r0_binind_disp_gbr, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0xc000 },

    // MOV.W R0, @(disp, GBR)
    { &sh4_inst_binary_movw_r0_binind_disp_gbr, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0xc100 },

    // MOV.L R0, @(disp, GBR)
    { &sh4_inst_binary_movl_r0_binind_disp_gbr, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0xc200 },

    // MOV.B @(disp, GBR), R0
    { &sh4_inst_binary_movb_binind_disp_gbr_r0, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xff00, 0xc400 },

    // MOV.W @(disp, GBR), R0
    { &sh4_inst_binary_movw_binind_disp_gbr_r0, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xff00, 0xc500 },

    // MOV.L @(disp, GBR), R0
    { &sh4_inst_binary_movl_binind_disp_gbr_r0, sh4_jit_movl_a_disp8_gbr_r0,
      false, SH4_GROUP_LS, 1, 0xff00, 0xc600 },

    // MOVA @(disp, PC), R0
    { &sh4_inst_binary_mova_binind_disp_pc_r0, sh4_jit_mova_a_disp_pc_r0,
      true, SH4_GROUP_EX, 1, 0xff00, 0xc700 },

    // MOVCA.L R0, @Rn
    { &sh4_inst_binary_movcal_r0_indgen, sh4_jit_fallback,
      false, SH4_GROUP_LS, 1, 0xf0ff, 0x00c3 },

    // FLDI0 FRn
    { FPU_HANDLER(fldi0), sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf08d },

    // FLDI1 Frn
    { FPU_HANDLER(fldi1), sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf09d },

    // FMOV FRm, FRn
    // 1111nnnnmmmm1100
    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    // FMOV XDm, DRn
    // 1111nnn0mmm11100
    // FMOV DRm, XDn
    // 1111nnn1mmm01100
    // FMOV XDm, XDn
    // 1111nnn1mmm11100
    { FPU_HANDLER(fmov_gen), sh4_jit_fmov_frm_frn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf00c },

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    // FMOV @Rm, XDn
    // 1111nnn1mmmm1000
    { FPU_HANDLER(fmovs_ind_gen), sh4_jit_fmov_arm_fpu, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf008 },

    // FMOV.S @(R0, Rm), FRn
    // 1111nnnnmmmm0110
    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    // FMOV @(R0, Rm), XDn
    // 1111nnn1mmmm0110
    { FPU_HANDLER(fmov_binind_r0_gen_fpu), sh4_jit_fmovs_a_r0_rm_fpu, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf006 },

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    // FMOV @Rm+, XDn
    // 1111nnn1mmmm1001
    { FPU_HANDLER(fmov_indgeninc_fpu), sh4_jit_fmov_fpu_armp_fpu, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf009 },

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    // FMOV XDm, @Rn
    // 1111nnnnmmm11010
    { FPU_HANDLER(fmov_fpu_indgen), sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf00a },

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    // FMOV XDm, @-Rn
    // 1111nnnnmmm11011
    { FPU_HANDLER(fmov_fpu_inddecgen), sh4_jit_fmov_fpu_amrn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf00b },

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    // FMOV DRm, @(R0, Rn)
    // 1111nnnnmmm00111
    // FMOV XDm, @(R0, Rn)
    // 1111nnnnmmm10111
    { FPU_HANDLER(fmov_fpu_binind_r0_gen), sh4_jit_fmov_fpu_a_r0_rn, false,
      SH4_GROUP_LS, 1, 0xf00f, 0xf007 },

    // FLDS FRm, FPUL
    // XXX Should this check the SZ or PR bits of FPSCR ?
    { &sh4_inst_binary_flds_fr_fpul, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf01d },

    // FSTS FPUL, FRn
    // XXX Should this check the SZ or PR bits of FPSCR ?
    { &sh4_inst_binary_fsts_fpul_fr, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf00d },

    // FABS FRn
    // 1111nnnn01011101
    // FABS DRn
    // 1111nnn001011101
    { FPU_HANDLER(fabs_fpu), sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf05d },

    // FADD FRm, FRn
    // 1111nnnnmmmm0000
    // FADD DRm, DRn
    // 1111nnn0mmm00000
    { FPU_HANDLER(fadd_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf000 },

    // FCMP/EQ FRm, FRn
    // 1111nnnnmmmm0100
    // FCMP/EQ DRm, DRn
    // 1111nnn0mmm00100
    { FPU_HANDLER(fcmpeq_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf004 },

    // FCMP/GT FRm, FRn
    // 1111nnnnmmmm0101
    // FCMP/GT DRm, DRn
    // 1111nnn0mmm00101
    { FPU_HANDLER(fcmpgt_fpu), sh4_jit_fcmpgt_frm_frn, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf005 },

    // FDIV FRm, FRn
    // 1111nnnnmmmm0011
    // FDIV DRm, DRn
    // 1111nnn0mmm00011
    { FPU_HANDLER(fdiv_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf003 },

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    // FLOAT FPUL, DRn
    // 1111nnn000101101
    { FPU_HANDLER(float_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf0ff, 0xf02d },

    // FMAC FR0, FRm, FRn
    // 1111nnnnmmmm1110
    { FPU_HANDLER(fmac_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf00e },

    // FMUL FRm, FRn
    // 1111nnnnmmmm0010
    // FMUL DRm, DRn
    // 1111nnn0mmm00010
    { FPU_HANDLER(fmul_fpu), sh4_jit_fmul_frm_frn, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf002 },

    // FNEG FRn
    // 1111nnnn01001101
    // FNEG DRn
    // 1111nnn001001101
    { FPU_HANDLER(fneg_fpu), sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0xf04d },

    // FSQRT FRn
    // 1111nnnn01101101
    // FSQRT DRn
    // 1111nnn001101101
    { FPU_HANDLER(fsqrt_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf0ff, 0xf06d },

    // FSUB FRm, FRn
    // 1111nnnnmmmm0001
    // FSUB DRm, DRn
    // 1111nnn0mmm00001
    { FPU_HANDLER(fsub_fpu), sh4_jit_fsub_frm_frn, false,
      SH4_GROUP_FE, 1, 0xf00f, 0xf001 },

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    // FTRC DRm, FPUL
    // 1111mmm000111101
    { FPU_HANDLER(ftrc_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf0ff, 0xf03d },

    // FCNVDS DRm, FPUL
    // 1111mmm010111101
    { FPU_HANDLER(fcnvds_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf1ff, 0xf0bd },

    // FCNVSD FPUL, DRn
    // 1111nnn010101101
    { FPU_HANDLER(fcnvsd_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf1ff, 0xf0ad },

    // LDS Rm, FPSCR
    { &sh4_inst_binary_lds_gen_fpscr, sh4_jit_lds_rm_fpscr, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x406a },

    // LDS Rm, FPUL
    { &sh4_inst_binary_gen_fpul, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x405a },

    // LDS.L @Rm+, FPSCR
    { &sh4_inst_binary_ldsl_indgeninc_fpscr, sh4_jit_ldsl_armp_fpscr, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4066 },

    // LDS.L @Rm+, FPUL
    { &sh4_inst_binary_ldsl_indgeninc_fpul, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4056 },

    // STS FPSCR, Rn
    { &sh4_inst_binary_sts_fpscr_gen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x006a },

    // STS FPUL, Rn
    { &sh4_inst_binary_sts_fpul_gen, sh4_jit_fallback, false,
      SH4_GROUP_LS, 1, 0xf0ff, 0x005a },

    // STS.L FPSCR, @-Rn
    { &sh4_inst_binary_stsl_fpscr_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4062 },

    // STS.L FPUL, @-Rn
    { &sh4_inst_binary_stsl_fpul_inddecgen, sh4_jit_fallback, false,
      SH4_GROUP_CO, 1, 0xf0ff, 0x4052 },

    // FIPR FVm, FVn - vector dot product
    { &sh4_inst_binary_fipr_fv_fv, sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf0ff, 0xf0ed },

    // FTRV XMTRX, FVn - multiple vector by matrix
    { &sh4_inst_binary_fitrv_mxtrx_fv, sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf3ff, 0xf1fd },

    // FSCA FPUL, DRn - sine/cosine table lookup
    // TODO: the issue cycle count here might be wrong, I couldn't find that
    //       value for this instruction
    { FPU_HANDLER(fsca_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf1ff, 0xf0fd },

    // FSRRA FRn
    // 1111nnnn01111101
    // TODO: the issue cycle for this opcode might be wrong as well
    { FPU_HANDLER(fsrra_fpu), sh4_jit_fallback, false,
      SH4_GROUP_FE, 1, 0xf0ff, 0xf07d },

    { NULL }
};

static InstOpcode invalid_opcode = {
    &sh4_inst_invalid, sh4_jit_fallback, false, (sh4_inst_group_t)0, 0, 0, 0
};

#define SH4_INST_RAISE_ERROR(sh4, error_tp)     \
    do {                                        \
        RAISE_ERROR(error_tp);                  \
    } while (0)

InstOpcode const *sh4_inst_lut[1 << 16];

void sh4_init_inst_lut() {
    cpu_inst_param inst;
    for (inst = 0; inst < (1 << 16); inst++)
        sh4_inst_lut[inst] = sh4_decode_inst_slow(inst);
}

// used to initialize the sh4_inst_lut
static InstOpcode const* sh4_decode_inst_slow(cpu_inst_param inst) {
    InstOpcode const *op = opcode_list;

    while (op->func) {
        if ((op->mask & inst) == op->val) {
            return op;
        }
        op++;
    }

    return &invalid_opcode;
}

#ifdef SH4_FPU_PEDANTIC
#define SH4_FPU_QNAN 0x7fbfffff

static void sh4_fr_invalid(Sh4 *sh4, unsigned dst_reg) {
    assert(dst_reg >= SH4_REG_FR0 && dst_reg <= SH4_REG_FR15);

    sh4->reg[SH4_REG_FPSCR] |= (SH4_FPSCR_FLAG_V_MASK | SH4_FPSCR_CAUSE_V_MASK);

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_ENABLE_V_MASK)
        sh4_set_exception(sh4, SH4_EXCP_FPU);
    else
        sh4->reg[dst_reg] = SH4_FPU_QNAN;
}

static void sh4_fpu_error(Sh4 *sh4) {
    sh4->reg[SH4_REG_FPSCR] |= SH4_FPSCR_CAUSE_E_MASK;
    sh4_set_exception(sh4, SH4_EXCP_FPU);
}
#endif

#define INST_MASK_0000000000001011 0xffff
#define INST_CONS_0000000000001011 0x000b

// RTS
// 0000000000001011
void sh4_inst_rts(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000001011, INST_CONS_0000000000001011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PR];
}

#define INST_MASK_0000000000101000 0xffff
#define INST_CONS_0000000000101000 0x0028

// CLRMAC
// 0000000000101000
void sh4_inst_clrmac(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000101000, INST_CONS_0000000000101000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_MACL] = sh4->reg[SH4_REG_MACH] = 0;
}

#define INST_MASK_0000000001001000 0xffff
#define INST_CONS_0000000001001000 0x0048

// CLRS
// 0000000001001000
void sh4_inst_clrs(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000001001000, INST_CONS_0000000001001000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_S_MASK;
}


#define INST_MASK_0000000000001000 0xffff
#define INST_CONS_0000000000001000 0x0008

// CLRT
// 0000000000001000
void sh4_inst_clrt(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000001000, INST_CONS_0000000000001000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0000000000111000 0xffff
#define INST_CONS_0000000000111000 0x0038

// LDTLB
// 0000000000111000
void sh4_inst_ldtlb(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000111000, INST_CONS_0000000000111000);

    error_set_feature("opcode implementation");
    error_set_opcode_format("0000000000111000");
    error_set_opcode_name("LDTLB");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_0000000000001001 0xffff
#define INST_CONS_0000000000001001 0x0009

// NOP
// 0000000000001001
void sh4_inst_nop(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000001001, INST_CONS_0000000000001001);

    // do nothing
}

#define INST_MASK_0000000000101011 0xffff
#define INST_CONS_0000000000101011 0x002b

// RTE
// 0000000000101011
void sh4_inst_rte(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000101011, INST_CONS_0000000000101011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    reg32_t old_sr_val = sh4->reg[SH4_REG_SR];
    sh4->reg[SH4_REG_SR] = sh4->reg[SH4_REG_SSR];
    sh4_on_sr_change(sh4, old_sr_val);
}

#define INST_MASK_0000000001011000 0xffff
#define INST_CONS_0000000001011000 0x0058

// SETS
// 0000000001011000
void sh4_inst_sets(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000001011000, INST_CONS_0000000001011000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_S_MASK;
}

#define INST_MASK_0000000000011000 0xffff
#define INST_CONS_0000000000011000 0x0018

// SETT
// 0000000000011000
void sh4_inst_sett(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000011000, INST_CONS_0000000000011000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0000000000011011 0xffff
#define INST_CONS_0000000000011011 0x001b

// SLEEP
// 0000000000011011
void sh4_inst_sleep(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000011011, INST_CONS_0000000000011011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

#define INST_MASK_1111101111111101 0xffff
#define INST_CONS_1111101111111101 0xfbfd

// FRCHG
// 1111101111111101
void sh4_inst_frchg(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111101111111101, INST_CONS_1111101111111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    /*
     * TODO: the software manual says the behavior is undefined if the PR bit
     * is not set in FPSCR.  This means I need to figure out what the acutal
     * hardware does when the PR bit is not set and mimc that here.  For now I
     * just let the operation go through so I can avoid branching.
     */

    sh4->reg[SH4_REG_FPSCR] ^= SH4_FPSCR_FR_MASK;
    sh4_fpu_bank_switch(sh4);
}

#define INST_MASK_1111001111111101 0xffff
#define INST_CONS_1111001111111101 0xf3fd

// FSCHG
// 1111001111111101
void sh4_inst_fschg(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111001111111101, INST_CONS_1111001111111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    /*
     * TODO: the software manual says the behavior is undefined if the PR bit
     * is not set in FPSCR.  This means I need to figure out what the acutal
     * hardware does when the PR bit is not set and mimc that here.  For now I
     * just let the operation go through so I can avoid branching.
     */

    sh4->reg[SH4_REG_FPSCR] ^= SH4_FPSCR_SZ_MASK;
}

#define INST_MASK_0000nnnn00101001 0xf0ff
#define INST_CONS_0000nnnn00101001 0x0029

// MOVT Rn
// 0000nnnn00101001
void sh4_inst_unary_movt_gen(void *cpu, cpu_inst_param inst) {
    CHECK_INST(inst, INST_MASK_0000nnnn00101001, INST_CONS_0000nnnn00101001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    *sh4_gen_reg(sh4, reg_no) =
        (reg32_t)((sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0100nnnn00010001 0xf0ff
#define INST_CONS_0100nnnn00010001 0x4011

// CMP/PZ Rn
// 0100nnnn00010001
void sh4_inst_unary_cmppz_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00010001, INST_CONS_0100nnnn00010001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t flag = ((int32_t)*sh4_gen_reg(sh4, reg_no)) >= 0;

    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;
}

#define INST_MASK_0100nnnn00010101 0xf0ff
#define INST_CONS_0100nnnn00010101 0x4015

// CMP/PL Rn
// 0100nnnn00010101
void sh4_inst_unary_cmppl_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00010101, INST_CONS_0100nnnn00010101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t flag = ((int32_t)*sh4_gen_reg(sh4, reg_no)) > 0;

    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;
}

#define INST_MASK_0100nnnn00010000 0xf0ff
#define INST_CONS_0100nnnn00010000 0x4010

// DT Rn
// 0100nnnn00010000
void sh4_inst_unary_dt_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00010000, INST_CONS_0100nnnn00010000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *valp = sh4_gen_reg(sh4, reg_no);
    (*valp)--;
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (!*valp) << SH4_SR_FLAG_T_SHIFT;
}

#define INST_MASK_0100nnnn00000100 0xf0ff
#define INST_CONS_0100nnnn00000100 0x4004

// ROTL Rn
// 0100nnnn00000100
void sh4_inst_unary_rotl_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00000100, INST_CONS_0100nnnn00000100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val = (val << 1) | shift_out;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00000101 0xf0ff
#define INST_CONS_0100nnnn00000101 0x4005

// ROTR Rn
// 0100nnnn00000101
void sh4_inst_unary_rotr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00000101, INST_CONS_0100nnnn00000101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = val & 1;

    val = (val >> 1) | (shift_out << 31);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00100100 0xf0ff
#define INST_CONS_0100nnnn00100100 0x4024

// ROTCL Rn
// 0100nnnn00100100
void sh4_inst_unary_rotcl_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100100, INST_CONS_0100nnnn00100100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;
    reg32_t shift_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT;

    val = (val << 1) | shift_in;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00100101 0xf0ff
#define INST_CONS_0100nnnn00100101 0x4025

// ROTCR Rn
// 0100nnnn00100101
void sh4_inst_unary_rotcr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100101, INST_CONS_0100nnnn00100101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = val & 1;
    reg32_t shift_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >> SH4_SR_FLAG_T_SHIFT;

    val = (val >> 1) | (shift_in << 31);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00100000 0xf0ff
#define INST_CONS_0100nnnn00100000 0x4020

// SHAL Rn
// 0100nnnn00100000
void sh4_inst_unary_shal_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100000, INST_CONS_0100nnnn00100000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val <<= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00100001 0xf0ff
#define INST_CONS_0100nnnn00100001 0x4021

// SHAR Rn
// 0100nnnn00100001
void sh4_inst_unary_shar_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100001, INST_CONS_0100nnnn00100001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    int32_t val = *regp;
    reg32_t shift_out = val & 1;

    val >>= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00000000 0xf0ff
#define INST_CONS_0100nnnn00000000 0x4000

// SHLL Rn
// 0100nnnn00000000
void sh4_inst_unary_shll_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00000000, INST_CONS_0100nnnn00000000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;
    reg32_t shift_out = (val & 0x80000000) >> 31;

    val <<= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00000001 0xf0ff
#define INST_CONS_0100nnnn00000001 0x4001

// SHLR Rn
// 0100nnnn00000001
void sh4_inst_unary_shlr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00000001, INST_CONS_0100nnnn00000001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    uint32_t val = *regp;
    reg32_t shift_out = val & 1;

    val >>= 1;
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) | (shift_out << SH4_SR_FLAG_T_SHIFT);

    *regp = val;
}

#define INST_MASK_0100nnnn00001000 0xf0ff
#define INST_CONS_0100nnnn00001000 0x4008

// SHLL2 Rn
// 0100nnnn00001000
void sh4_inst_unary_shll2_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00001000, INST_CONS_0100nnnn00001000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val <<= 2;
    *regp = val;
}

#define INST_MASK_0100nnnn00001001 0xf0ff
#define INST_CONS_0100nnnn00001001 0x4009

// SHLR2 Rn
// 0100nnnn00001001
void sh4_inst_unary_shlr2_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00001001, INST_CONS_0100nnnn00001001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val >>= 2;
    *regp = val;
}

#define INST_MASK_0100nnnn00011000 0xf0ff
#define INST_CONS_0100nnnn00011000 0x4018

// SHLL8 Rn
// 0100nnnn00011000
void sh4_inst_unary_shll8_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00011000, INST_CONS_0100nnnn00011000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val <<= 8;
    *regp = val;
}

#define INST_MASK_0100nnnn00011001 0xf0ff
#define INST_CONS_0100nnnn00011001 0x4019

// SHLR8 Rn
// 0100nnnn00011001
void sh4_inst_unary_shlr8_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00011001, INST_CONS_0100nnnn00011001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val >>= 8;
    *regp = val;
}

#define INST_MASK_0100nnnn00101000 0xf0ff
#define INST_CONS_0100nnnn00101000 0x4028

// SHLL16 Rn
// 0100nnnn00101000
void sh4_inst_unary_shll16_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00101000, INST_CONS_0100nnnn00101000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val <<= 16;
    *regp = val;
}

#define INST_MASK_0100nnnn00101001 0xf0ff
#define INST_CONS_0100nnnn00101001 0x4029

// SHLR16 Rn
// 0100nnnn00101001
void sh4_inst_unary_shlr16_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00101001, INST_CONS_0100nnnn00101001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    reg32_t *regp = sh4_gen_reg(sh4, reg_no);
    reg32_t val = *regp;

    val >>= 16;
    *regp = val;
}

#define INST_MASK_0000nnnn00100011 0xf0ff
#define INST_CONS_0000nnnn00100011 0x0023

// BRAF Rn
// 0000nnnn00100011
void sh4_inst_unary_braf_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00100011, INST_CONS_0000nnnn00100011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + *sh4_gen_reg(sh4, reg_no) + 4;
}

#define INST_MASK_0000nnnn00000011 0xf0ff
#define INST_CONS_0000nnnn00000011 0x0003

// BSRF Rn
// 0000nnnn00000011
void sh4_inst_unary_bsrf_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00000011, INST_CONS_0000nnnn00000011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int reg_no = (inst >> 8) & 0xf;

    sh4->delayed_branch = true;
    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + *sh4_gen_reg(sh4, reg_no) + 4;
}

#define INST_MASK_10001000iiiiiiii 0xff00
#define INST_CONS_10001000iiiiiiii 0x8800

// CMP/EQ #imm, R0
// 10001000iiiiiiii
void sh4_inst_binary_cmpeq_imm_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10001000iiiiiiii, INST_CONS_10001000iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int8_t imm8 = inst_simm8(inst);
    reg32_t imm_val = (int32_t)(imm8);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= ((*sh4_gen_reg(sh4, 0) == imm_val) << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_11001101iiiiiiii 0xff00
#define INST_CONS_11001101iiiiiiii 0xcd00

// AND.B #imm, @(R0, GBR)
// 11001101iiiiiiii
void sh4_inst_binary_andb_imm_r0_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001101iiiiiiii, INST_CONS_11001101iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val = memory_map_read_8(sh4->mem.map, addr);

    val &= inst_imm8(inst);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_11001001iiiiiiii 0xff00
#define INST_CONS_11001001iiiiiiii 0xc900

// AND #imm, R0
// 11001001iiiiiiii
void sh4_inst_binary_and_imm_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001001iiiiiiii, INST_CONS_11001001iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, 0) &= inst_imm8(inst);
}

#define INST_MASK_11001111iiiiiiii 0xff00
#define INST_CONS_11001111iiiiiiii 0xcf00

// OR.B #imm, @(R0, GBR)
// 11001111iiiiiiii
void sh4_inst_binary_orb_imm_r0_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001111iiiiiiii, INST_CONS_11001111iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val = memory_map_read_8(sh4->mem.map, addr);

    val |= inst_imm8(inst);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_11001011iiiiiiii 0xff00
#define INST_CONS_11001011iiiiiiii 0xcb00

// OR #imm, R0
// 11001011iiiiiiii
void sh4_inst_binary_or_imm_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001011iiiiiiii, INST_CONS_11001011iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, 0) |= inst_imm8(inst);
}

#define INST_MASK_11001000iiiiiiii 0xff00
#define INST_CONS_11001000iiiiiiii 0xc800

// TST #imm, R0
// 11001000iiiiiiii
void sh4_inst_binary_tst_imm_r0(void *cpu, cpu_inst_param inst) {
    CHECK_INST(inst, INST_MASK_11001000iiiiiiii, INST_CONS_11001000iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(inst_imm8(inst) & *sh4_gen_reg(sh4, 0)) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;
}

#define INST_MASK_11001100iiiiiiii 0xff00
#define INST_CONS_11001100iiiiiiii 0xcc00

// TST.B #imm, @(R0, GBR)
// 11001100iiiiiiii
void sh4_inst_binary_tstb_imm_r0_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001100iiiiiiii, INST_CONS_11001100iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val = memory_map_read_8(sh4->mem.map, addr);

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(inst_imm8(inst) & val) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;
}

#define INST_MASK_11001010iiiiiiii 0xff00
#define INST_CONS_11001010iiiiiiii 0xca00

// XOR #imm, R0
// 11001010iiiiiiii
void sh4_inst_binary_xor_imm_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001010iiiiiiii, INST_CONS_11001010iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, 0) ^= inst_imm8(inst);
}

#define INST_MASK_11001110iiiiiiii 0xff00
#define INST_CONS_11001110iiiiiiii 0xce00

// XOR.B #imm, @(R0, GBR)
// 11001110iiiiiiii
void sh4_inst_binary_xorb_imm_r0_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11001110iiiiiiii, INST_CONS_11001110iiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + sh4->reg[SH4_REG_GBR];
    uint8_t val = memory_map_read_8(sh4->mem.map, addr);

    val ^= inst_imm8(inst);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_10001011dddddddd 0xff00
#define INST_CONS_10001011dddddddd 0x8b00

// BF label
// 10001011dddddddd
void sh4_inst_unary_bf_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10001011dddddddd, INST_CONS_10001011dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)) {
        sh4->reg[SH4_REG_PC] += (((int32_t)inst_simm8(inst)) << 1) + (4 - 2);
#ifdef DEEP_SYSCALL_TRACE
        deep_syscall_notify_jump(sh4->reg[SH4_REG_PC]);
#endif
    }
}

#define INST_MASK_10001111dddddddd 0xff00
#define INST_CONS_10001111dddddddd 0x8f00

// BF/S label
// 10001111dddddddd
void sh4_inst_unary_bfs_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10001111dddddddd, INST_CONS_10001111dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)) {
        sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst_simm8(inst)) << 1) + 4;
        sh4->delayed_branch = true;
    }
}

#define INST_MASK_10001001dddddddd 0xff00
#define INST_CONS_10001001dddddddd 0x8900

// BT label
// 10001001dddddddd
void sh4_inst_unary_bt_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10001001dddddddd, INST_CONS_10001001dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) {
        sh4->reg[SH4_REG_PC] += (((int32_t)inst_simm8(inst)) << 1) + (4 - 2);
#ifdef DEEP_SYSCALL_TRACE
        deep_syscall_notify_jump(sh4->reg[SH4_REG_PC]);
#endif
    }
}

#define INST_MASK_10001101dddddddd 0xff00
#define INST_CONS_10001101dddddddd 0x8d00

// BT/S label
// 10001101dddddddd
void sh4_inst_unary_bts_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10001101dddddddd, INST_CONS_10001101dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) {
        sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst_simm8(inst)) << 1) + 4;
        sh4->delayed_branch = true;
    }
}

#define INST_MASK_1010dddddddddddd 0xf000
#define INST_CONS_1010dddddddddddd 0xa000

// BRA label
// 1010dddddddddddd
void sh4_inst_unary_bra_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1010dddddddddddd, INST_CONS_1010dddddddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->delayed_branch = true;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst_simm12(inst)) << 1) + 4;
}

#define INST_MASK_1011dddddddddddd 0xf000
#define INST_CONS_1011dddddddddddd 0xb000

// BSR label
// 1011dddddddddddd
void sh4_inst_unary_bsr_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1011dddddddddddd, INST_CONS_1011dddddddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = sh4->reg[SH4_REG_PC] + (((int32_t)inst_simm12(inst)) << 1) + 4;
    sh4->delayed_branch = true;
}

#define INST_MASK_11000011iiiiiiii 0xff00
#define INST_CONS_11000011iiiiiiii 0xc300

// TRAPA #immed
// 11000011iiiiiiii
void sh4_inst_unary_trapa_disp(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000011iiiiiiii, INST_CONS_11000011iiiiiiii);

#ifdef ENABLE_DEBUGGER
    struct Sh4 *sh4 = (struct Sh4*)cpu;

    /*
     * Send this to the gdb backend if it's running.  else, fall through to the
     * next case, which would jump to exception handling code if I had bothered
     * to implement it.
     */
    if (dc_debugger_enabled()) {
        debug_on_softbreak(inst, sh4->reg[SH4_REG_PC]);
        return;
    }
#endif /* ifdef ENABLE_DEBUGGER */

    error_set_feature("opcode implementation");
    error_set_opcode_format("11000011iiiiiiii");
    error_set_opcode_name("TRAPA #immed");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_0100nnnn00011011 0xf0ff
#define INST_CONS_0100nnnn00011011 0x401b

// TAS.B @Rn
// 0100nnnn00011011
void sh4_inst_unary_tasb_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00011011, INST_CONS_0100nnnn00011011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint8_t val_new, val_old;
    reg32_t mask;

    val_old = memory_map_read_8(sh4->mem.map, addr);
    val_new = val_old | 0x80;
    memory_map_write_8(sh4->mem.map, addr, val_new);

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    mask = (!val_old) << SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= mask;
}

#define INST_MASK_0000nnnn10010011 0xf0ff
#define INST_CONS_0000nnnn10010011 0x0093

// OCBI @Rn
// 0000nnnn10010011
void sh4_inst_unary_ocbi_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn10010011, INST_CONS_0000nnnn10010011);

    /* TODO: if mmu is enabled, this inst can generate exceptions */
}

#define INST_MASK_0000nnnn10100011 0xf0ff
#define INST_CONS_0000nnnn10100011 0x00a3

// OCBP @Rn
// 0000nnnn10100011
void sh4_inst_unary_ocbp_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn10100011, INST_CONS_0000nnnn10100011);

    /* TODO: if mmu is enabled, this inst can generate exceptions */
}

#define INST_MASK_0000nnnn10110011 0xf0ff
#define INST_CONS_0000nnnn10110011 0x00b3

// OCBWB @Rn
// 0000nnnn10110011
void sh4_inst_unary_ocbwb_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn10110011, INST_CONS_0000nnnn10110011);

    /* TODO: if mmu is enabled, this inst can generate exceptions */
}

#define INST_MASK_0000nnnn10000011 0xf0ff
#define INST_CONS_0000nnnn10000011 0x0083

// PREF @Rn
// 0000nnnn10000011
void sh4_inst_unary_pref_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn10000011, INST_CONS_0000nnnn10000011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned reg_no = (inst >> 8) & 0xf;
    addr32_t addr = *sh4_gen_reg(sh4, reg_no);

    if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL)
        sh4_sq_pref(sh4, addr);
}

#define INST_MASK_0100nnnn00101011 0xf0ff
#define INST_CONS_0100nnnn00101011 0x402b

// JMP @Rn
// 0100nnnn00101011
void sh4_inst_unary_jmp_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00101011, INST_CONS_0100nnnn00101011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->delayed_branch_addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    sh4->delayed_branch = true;
}

#define INST_MASK_0100nnnn00001011 0xf0ff
#define INST_CONS_0100nnnn00001011 0x400b

// JSR @Rn
// 0100nnnn00001011
void sh4_inst_unary_jsr_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00001011, INST_CONS_0100nnnn00001011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_PR] = sh4->reg[SH4_REG_PC] + 4;
    sh4->delayed_branch_addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    sh4->delayed_branch = true;
}

#define INST_MASK_0100mmmm00001110 0xf0ff
#define INST_CONS_0100mmmm00001110 0x400e

// LDC Rm, SR
// 0100mmmm00001110
void sh4_inst_binary_ldc_gen_sr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00001110, INST_CONS_0100mmmm00001110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t old_sr = sh4->reg[SH4_REG_SR];
    sh4->reg[SH4_REG_SR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    sh4_on_sr_change(sh4, old_sr);
}

#define INST_MASK_0100mmmm00011110 0xf0ff
#define INST_CONS_0100mmmm00011110 0x401e

// LDC Rm, GBR
// 0100mmmm00011110
void sh4_inst_binary_ldc_gen_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00011110, INST_CONS_0100mmmm00011110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_GBR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm00101110 0xf0ff
#define INST_CONS_0100mmmm00101110 0x402e

// LDC Rm, VBR
// 0100mmmm00101110
void sh4_inst_binary_ldc_gen_vbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00101110, INST_CONS_0100mmmm00101110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_VBR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm00111110 0xf0ff
#define INST_CONS_0100mmmm00111110 0x403e

// LDC Rm, SSR
// 0100mmmm00111110
void sh4_inst_binary_ldc_gen_ssr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00111110, INST_CONS_0100mmmm00111110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_SSR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm01001110 0xf0ff
#define INST_CONS_0100mmmm01001110 0x404e

// LDC Rm, SPC
// 0100mmmm01001110
void sh4_inst_binary_ldc_gen_spc(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01001110, INST_CONS_0100mmmm01001110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_SPC] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm11111010 0xf0ff
#define INST_CONS_0100mmmm11111010 0x40fa

// LDC Rm, DBR
// 0100mmmm11111010
void sh4_inst_binary_ldc_gen_dbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm11111010, INST_CONS_0100mmmm11111010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    sh4->reg[SH4_REG_DBR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0000nnnn00000010 0xf0ff
#define INST_CONS_0000nnnn00000010 0x0002

// STC SR, Rn
// 0000nnnn00000010
void sh4_inst_binary_stc_sr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00000010, INST_CONS_0000nnnn00000010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_SR];
}

#define INST_MASK_0000nnnn00010010 0xf0ff
#define INST_CONS_0000nnnn00010010 0x0012

// STC GBR, Rn
// 0000nnnn00010010
void sh4_inst_binary_stc_gbr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00010010, INST_CONS_0000nnnn00010010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_GBR];
}

#define INST_MASK_0000nnnn00100010 0xf0ff
#define INST_CONS_0000nnnn00100010 0x0022

// STC VBR, Rn
// 0000nnnn00100010
void sh4_inst_binary_stc_vbr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00100010, INST_CONS_0000nnnn00100010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_VBR];
}

#define INST_MASK_0000nnnn00110010 0xf0ff
#define INST_CONS_0000nnnn00110010 0x0032

// STC SSR, Rn
// 0000nnnn00110010
void sh4_inst_binary_stc_ssr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00110010, INST_CONS_0000nnnn00110010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_SSR];
}

#define INST_MASK_0000nnnn01000010 0xf0ff
#define INST_CONS_0000nnnn01000010 0x0042

// STC SPC, Rn
// 0000nnnn01000010
void sh4_inst_binary_stc_spc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn01000010, INST_CONS_0000nnnn01000010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_SPC];
}

#define INST_MASK_0000nnnn00111010 0xf0ff
#define INST_CONS_0000nnnn00111010 0x003a

// STC SGR, Rn
// 0000nnnn00111010
void sh4_inst_binary_stc_sgr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00111010, INST_CONS_0000nnnn00111010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_SGR];
}

#define INST_MASK_0000nnnn11111010 0xf0ff
#define INST_CONS_0000nnnn11111010 0x00fa

// STC DBR, Rn
// 0000nnnn11111010
void sh4_inst_binary_stc_dbr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn11111010, INST_CONS_0000nnnn11111010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_DBR];
}

#define INST_MASK_0100mmmm00000111 0xf0ff
#define INST_CONS_0100mmmm00000111 0x4007

// LDC.L @Rm+, SR
// 0100mmmm00000111
void sh4_inst_binary_ldcl_indgeninc_sr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00000111, INST_CONS_0100mmmm00000111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    reg32_t old_sr_val = sh4->reg[SH4_REG_SR];
    sh4->reg[SH4_REG_SR] = val;
    sh4_on_sr_change(sh4, old_sr_val);
}

#define INST_MASK_0100mmmm00010111 0xf0ff
#define INST_CONS_0100mmmm00010111 0x4017

// LDC.L @Rm+, GBR
// 0100mmmm00010111
void sh4_inst_binary_ldcl_indgeninc_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00010111, INST_CONS_0100mmmm00010111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *src_reg;

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    sh4->reg[SH4_REG_GBR] = val;
}

#define INST_MASK_0100mmmm00100111 0xf0ff
#define INST_CONS_0100mmmm00100111 0x4027

// LDC.L @Rm+, VBR
// 0100mmmm00100111
void sh4_inst_binary_ldcl_indgeninc_vbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00100111, INST_CONS_0100mmmm00100111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    sh4->reg[SH4_REG_VBR] = val;
}

#define INST_MASK_0100mmmm00110111 0xf0ff
#define INST_CONS_0100mmmm00110111 0x4037

// LDC.L @Rm+, SSR
// 0100mmmm00110111
void sh4_inst_binary_ldcl_indgenic_ssr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00110111, INST_CONS_0100mmmm00110111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    sh4->reg[SH4_REG_SSR] = val;
}

#define INST_MASK_0100mmmm01000111 0xf0ff
#define INST_CONS_0100mmmm01000111 0x4047

// LDC.L @Rm+, SPC
// 0100mmmm01000111
void sh4_inst_binary_ldcl_indgeninc_spc(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01000111, INST_CONS_0100mmmm01000111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    sh4->reg[SH4_REG_SPC] = val;
}

#define INST_MASK_0100mmmm11110110 0xf0ff
#define INST_CONS_0100mmmm11110110 0x40f6

// LDC.L @Rm+, DBR
// 0100mmmm11110110
void sh4_inst_binary_ldcl_indgeninc_dbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm11110110, INST_CONS_0100mmmm11110110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    sh4->reg[SH4_REG_DBR] = val;
}

#define INST_MASK_0100nnnn00000011 0xf0ff
#define INST_CONS_0100nnnn00000011 0x4003

// STC.L SR, @-Rn
// 0100nnnn00000011
void sh4_inst_binary_stcl_sr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00000011, INST_CONS_0100nnnn00000011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_SR]);

    *regp = addr;
}

#define INST_MASK_0100nnnn00010011 0xf0ff
#define INST_CONS_0100nnnn00010011 0x4013

// STC.L GBR, @-Rn
// 0100nnnn00010011
void sh4_inst_binary_stcl_gbr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00010011, INST_CONS_0100nnnn00010011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_GBR]);

    *regp = addr;
}

#define INST_MASK_0100nnnn00100011 0xf0ff
#define INST_CONS_0100nnnn00100011 0x4023

// STC.L VBR, @-Rn
// 0100nnnn00100011
void sh4_inst_binary_stcl_vbr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100011, INST_CONS_0100nnnn00100011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_VBR]);

    *regp = addr;
}

#define INST_MASK_0100nnnn00110011 0xf0ff
#define INST_CONS_0100nnnn00110011 0x4033

// STC.L SSR, @-Rn
// 0100nnnn00110011
void sh4_inst_binary_stcl_ssr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00110011, INST_CONS_0100nnnn00110011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_SSR]);

    *regp = addr;
}

#define INST_MASK_0100nnnn01000011 0xf0ff
#define INST_CONS_0100nnnn01000011 0x4043

// STC.L SPC, @-Rn
// 0100nnnn01000011
void sh4_inst_binary_stcl_spc_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn01000011, INST_CONS_0100nnnn01000011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_SPC]);

    *regp = addr;
}

#define INST_MASK_0100nnnn00110010 0xf0ff
#define INST_CONS_0100nnnn00110010 0x4032

// STC.L SGR, @-Rn
// 0100nnnn00110010
void sh4_inst_binary_stcl_sgr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00110010, INST_CONS_0100nnnn00110010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_SGR]);

    *regp = addr;
}

#define INST_MASK_0100nnnn11110010 0xf0ff
#define INST_CONS_0100nnnn11110010 0x40f2

// STC.L DBR, @-Rn
// 0100nnnn11110010
void sh4_inst_binary_stcl_dbr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn11110010, INST_CONS_0100nnnn11110010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *regp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *regp - 4;
    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_DBR]);

    *regp = addr;
}

#define INST_MASK_1110nnnniiiiiiii 0xf000
#define INST_CONS_1110nnnniiiiiiii 0xe000

// MOV #imm, Rn
// 1110nnnniiiiiiii
void sh4_inst_binary_mov_imm_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1110nnnniiiiiiii, INST_CONS_1110nnnniiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)((int8_t)inst_imm8(inst));
}

#define INST_MASK_0111nnnniiiiiiii 0xf000
#define INST_CONS_0111nnnniiiiiiii 0x7000

// ADD #imm, Rn
// 0111nnnniiiiiiii
void sh4_inst_binary_add_imm_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0111nnnniiiiiiii, INST_CONS_0111nnnniiiiiiii);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) += (int32_t)((int8_t)(inst_imm8(inst)));
}

#define INST_MASK_1001nnnndddddddd 0xf000
#define INST_CONS_1001nnnndddddddd 0x9000

// MOV.W @(disp, PC), Rn
// 1001nnnndddddddd
void sh4_inst_binary_movw_binind_disp_pc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1001nnnndddddddd, INST_CONS_1001nnnndddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 1) + sh4->reg[SH4_REG_PC] + 4;
    int reg_no = (inst >> 8) & 0xf;
    int16_t mem_in;

    mem_in = memory_map_read_16(sh4->mem.map, addr);
    *sh4_gen_reg(sh4, reg_no) = (int32_t)mem_in;
}

#define INST_MASK_1101nnnndddddddd 0xf000
#define INST_CONS_1101nnnndddddddd 0xd000

// MOV.L @(disp, PC), Rn
// 1101nnnndddddddd
void sh4_inst_binary_movl_binind_disp_pc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1101nnnndddddddd, INST_CONS_1101nnnndddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 2) + (sh4->reg[SH4_REG_PC] & ~3) + 4;
    int reg_no = (inst >> 8) & 0xf;
    int32_t mem_in;

    mem_in = memory_map_read_32(sh4->mem.map, addr);
    *sh4_gen_reg(sh4, reg_no) = mem_in;
}

#define INST_MASK_0110nnnnmmmm0011 0xf00f
#define INST_CONS_0110nnnnmmmm0011 0x6003

// MOV Rm, Rn
// 0110nnnnmmmm0011
void sh4_inst_binary_mov_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0011, INST_CONS_0110nnnnmmmm0011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0110nnnnmmmm1000 0xf00f
#define INST_CONS_0110nnnnmmmm1000 0x6008

// SWAP.B Rm, Rn
// 0110nnnnmmmm1000
void sh4_inst_binary_swapb_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1000, INST_CONS_0110nnnnmmmm1000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned byte0, byte1;
    reg32_t *reg_src = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t val_src = *reg_src;

    byte0 = val_src & 0x00ff;
    byte1 = (val_src & 0xff00) >> 8;

    val_src &= ~0xffff;
    val_src |= byte1 | (byte0 << 8);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = val_src;
}

#define INST_MASK_0110nnnnmmmm1001 0xf00f
#define INST_CONS_0110nnnnmmmm1001 0x6009

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
void sh4_inst_binary_swapw_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1001, INST_CONS_0110nnnnmmmm1001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned word0, word1;
    uint32_t *reg_src = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    uint32_t val_src = *reg_src;

    word0 = val_src & 0xffff;
    word1 = val_src >> 16;

    val_src = word1 | (word0 << 16);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = val_src;
}

#define INST_MASK_0010nnnnmmmm1101 0xf00f
#define INST_CONS_0010nnnnmmmm1101 0x200d

// XTRCT Rm, Rn
// 0110nnnnmmmm1101
void sh4_inst_binary_xtrct_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1101, INST_CONS_0010nnnnmmmm1101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *reg_dst = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *reg_src = sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    *reg_dst = (((*reg_dst) & 0xffff0000) >> 16) |
        (((*reg_src) & 0x0000ffff) << 16);
}

#define INST_MASK_0011nnnnmmmm1100 0xf00f
#define INST_CONS_0011nnnnmmmm1100 0x300c

// ADD Rm, Rn
// 0011nnnnmmmm1100
void sh4_inst_binary_add_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1100, INST_CONS_0011nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) += *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0011nnnnmmmm1110 0xf00f
#define INST_CONS_0011nnnnmmmm1110 0x300e

// ADDC Rm, Rn
// 0011nnnnmmmm1110
void sh4_inst_binary_addc_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1110, INST_CONS_0011nnnnmmmm1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    bool carry_out;
    bool carry_in = (bool)(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);
    *dst_reg = add_flags(*src_reg, *dst_reg, carry_in, &carry_out, NULL);
    if (carry_out)
        sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
    else
        sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0011nnnnmmmm1111 0xf00f
#define INST_CONS_0011nnnnmmmm1111 0x300f

// ADDV Rm, Rn
// 0011nnnnmmmm1111
void sh4_inst_binary_addv_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1111, INST_CONS_0011nnnnmmmm1111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    bool overflow;
    *dst_reg = add_flags(*src_reg,  *dst_reg, false, NULL, &overflow);

    if (overflow)
        sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
    else
        sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0011nnnnmmmm0000 0xf00f
#define INST_CONS_0011nnnnmmmm0000 0x3000

// CMP/EQ Rm, Rn
// 0011nnnnmmmm0000
void sh4_inst_binary_cmpeq_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0000, INST_CONS_0011nnnnmmmm0000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |=
        ((*sh4_gen_reg(sh4, (inst >> 4) & 0xf) ==
          *sh4_gen_reg(sh4, (inst >> 8) & 0xf)) <<
               SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0011nnnnmmmm0010 0xf00f
#define INST_CONS_0011nnnnmmmm0010 0x3002

// CMP/HS Rm, Rn
// 0011nnnnmmmm0010
void sh4_inst_binary_cmphs_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0010, INST_CONS_0011nnnnmmmm0010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint32_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    sh4->reg[SH4_REG_SR] |= ((lhs >= rhs) << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0011nnnnmmmm0011 0xf00f
#define INST_CONS_0011nnnnmmmm0011 0x3003

// CMP/GE Rm, Rn
// 0011nnnnmmmm0011
void sh4_inst_binary_cmpge_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0011, INST_CONS_0011nnnnmmmm0011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    int32_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    sh4->reg[SH4_REG_SR] |= ((lhs >= rhs) << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0011nnnnmmmm0110 0xf00f
#define INST_CONS_0011nnnnmmmm0110 0x3006

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
void sh4_inst_binary_cmphi_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0110, INST_CONS_0011nnnnmmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    uint32_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint32_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    sh4->reg[SH4_REG_SR] |= ((lhs > rhs) << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0011nnnnmmmm0111 0xf00f
#define INST_CONS_0011nnnnmmmm0111 0x3007

// CMP/GT Rm, Rn
// 0011nnnnmmmm0111
void sh4_inst_binary_cmpgt_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0111, INST_CONS_0011nnnnmmmm0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    int32_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    sh4->reg[SH4_REG_SR] |= ((lhs > rhs) << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0010nnnnmmmm1100 0xf00f
#define INST_CONS_0010nnnnmmmm1100 0x200c

// CMP/STR Rm, Rn
// 0010nnnnmmmm1100
void sh4_inst_binary_cmpstr_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1100, INST_CONS_0010nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint32_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    uint32_t flag;

    flag = !!(((lhs & 0x000000ff) == (rhs & 0x000000ff)) ||
              ((lhs & 0x0000ff00) == (rhs & 0x0000ff00)) ||
              ((lhs & 0x00ff0000) == (rhs & 0x00ff0000)) ||
              ((lhs & 0xff000000) == (rhs & 0xff000000)));

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= flag << SH4_SR_FLAG_T_SHIFT;
}

#define INST_MASK_0011nnnnmmmm0100 0xf00f
#define INST_CONS_0011nnnnmmmm0100 0x3004

// DIV1 Rm, Rn
// 0011nnnnmmmm0100
void sh4_inst_binary_div1_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0100, INST_CONS_0011nnnnmmmm0100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *dividend_p = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *divisor_p  = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
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
}

#define INST_MASK_0010nnnnmmmm0111 0xf00f
#define INST_CONS_0010nnnnmmmm0111 0x2007

// DIV0S Rm, Rn
// 0010nnnnmmmm0111
void sh4_inst_binary_div0s_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0111, INST_CONS_0010nnnnmmmm0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t divisor = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t dividend = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    reg32_t new_q = (divisor & 0x80000000) >> 31;
    reg32_t new_m = (dividend & 0x80000000) >> 31;
    reg32_t new_t = new_q ^ new_m;

    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_Q_MASK) |
        (new_q << SH4_SR_Q_SHIFT);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_M_MASK) |
        (new_m << SH4_SR_M_SHIFT);
    sh4->reg[SH4_REG_SR] = (sh4->reg[SH4_REG_SR] & ~SH4_SR_FLAG_T_MASK) |
        (new_t << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_0000000000011001 0xffff
#define INST_CONS_0000000000011001 0x0019

// DIV0U
// 0000000000011001
void sh4_inst_noarg_div0u(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000000000011001, INST_CONS_0000000000011001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &=
        ~(SH4_SR_M_MASK | SH4_SR_Q_MASK | SH4_SR_FLAG_T_MASK);
}

#define INST_MASK_0011nnnnmmmm1101 0xf00f
#define INST_CONS_0011nnnnmmmm1101 0x300d

// DMULS.L Rm, Rn
// 0011nnnnmmmm1101
void sh4_inst_binary_dmulsl_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1101, INST_CONS_0011nnnnmmmm1101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int32_t val1 = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t val2 = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int64_t res = (int64_t)val1 * (int64_t)val2;

    sh4->reg[SH4_REG_MACH] = ((uint64_t)res) >> 32;
    sh4->reg[SH4_REG_MACL] = ((uint64_t)res) & 0xffffffff;
}

#define INST_MASK_0011nnnnmmmm0101 0xf00f
#define INST_CONS_0011nnnnmmmm0101 0x3005

// DMULU.L Rm, Rn
// 0011nnnnmmmm0101
void sh4_inst_binary_dmulul_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm0101, INST_CONS_0011nnnnmmmm0101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint64_t val1 = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint64_t val2 = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    uint64_t res = (uint64_t)val1 * (uint64_t)val2;

    sh4->reg[SH4_REG_MACH] = res >> 32;
    sh4->reg[SH4_REG_MACL] = res & 0xffffffff;
}

#define INST_MASK_0110nnnnmmmm1110 0xf00f
#define INST_CONS_0110nnnnmmmm1110 0x600e

// EXTS.B Rm, Rn
// 0110nnnnmmmm1110
void sh4_inst_binary_extsb_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1110, INST_CONS_0110nnnnmmmm1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t src_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)((int8_t)(src_val & 0xff));
}

#define INST_MASK_0110nnnnmmmm1111 0xf00f
#define INST_CONS_0110nnnnmmmm1111 0x600f

// EXTS.W Rm, Rnn
// 0110nnnnmmmm1111
void sh4_inst_binary_extsw_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1111, INST_CONS_0110nnnnmmmm1111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t src_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)((int16_t)(src_val & 0xffff));
}

#define INST_MASK_0110nnnnmmmm1100 0xf00f
#define INST_CONS_0110nnnnmmmm1100 0x600c

// EXTU.B Rm, Rn
// 0110nnnnmmmm1100
void sh4_inst_binary_extub_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1100, INST_CONS_0110nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t src_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = src_val & 0xff;
}

#define INST_MASK_0110nnnnmmmm1101 0xf00f
#define INST_CONS_0110nnnnmmmm1101 0x600d

// EXTU.W Rm, Rn
// 0110nnnnmmmm1101
void sh4_inst_binary_extuw_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1101, INST_CONS_0110nnnnmmmm1101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t src_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = src_val & 0xffff;
}

#define INST_MASK_0000nnnnmmmm0111 0xf00f
#define INST_CONS_0000nnnnmmmm0111 0x0007

// MUL.L Rm, Rn
// 0000nnnnmmmm0111
void sh4_inst_binary_mull_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm0111, INST_CONS_0000nnnnmmmm0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_MACL] =
        *sh4_gen_reg(sh4, (inst >> 8) & 0xf) *
        *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0010nnnnmmmm1111 0xf00f
#define INST_CONS_0010nnnnmmmm1111 0x200f

// MULS.W Rm, Rn
// 0010nnnnmmmm1111
void sh4_inst_binary_mulsw_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1111, INST_CONS_0010nnnnmmmm1111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int16_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int16_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    sh4->reg[SH4_REG_MACL] = (int32_t)lhs * (int32_t)rhs;
}

#define INST_MASK_0010nnnnmmmm1110 0xf00f
#define INST_CONS_0010nnnnmmmm1110 0x200e

// MULU.W Rm, Rn
// 0010nnnnmmmm1110
void sh4_inst_binary_muluw_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1110, INST_CONS_0010nnnnmmmm1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint16_t lhs = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint16_t rhs = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    sh4->reg[SH4_REG_MACL] = (uint32_t)lhs * (uint32_t)rhs;
}

#define INST_MASK_0110nnnnmmmm1011 0xf00f
#define INST_CONS_0110nnnnmmmm1011 0x600b

// NEG Rm, Rn
// 0110nnnnmmmm1011
void sh4_inst_binary_neg_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1011, INST_CONS_0110nnnnmmmm1011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = -*sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0110nnnnmmmm1010 0xf00f
#define INST_CONS_0110nnnnmmmm1010 0x600a

// NEGC Rm, Rn
// 0110nnnnmmmm1010
void sh4_inst_binary_negc_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm1010, INST_CONS_0110nnnnmmmm1010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    int32_t src = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t flag_t_in = (sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) >>
        SH4_SR_FLAG_T_SHIFT;

    uint32_t tmp = -src;
    uint32_t dst = tmp - flag_t_in;
    reg32_t flag_t_out = (tmp || dst > tmp);

    sh4->reg[SH4_REG_SR] |= (flag_t_out << SH4_SR_FLAG_T_SHIFT);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = dst;
}

#define INST_MASK_0011nnnnmmmm1000 0xf00f
#define INST_CONS_0011nnnnmmmm1000 0x3008

// SUB Rm, Rn
// 0011nnnnmmmm1000
void sh4_inst_binary_sub_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1000, INST_CONS_0011nnnnmmmm1000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) -= *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0011nnnnmmmm1010 0xf00f
#define INST_CONS_0011nnnnmmmm1010 0x300a

// SUBC Rm, Rn
// 0011nnnnmmmm1010
void sh4_inst_binary_subc_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1010, INST_CONS_0011nnnnmmmm1010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    // detect carry by doing 64-bit math
    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    bool carry_in = (bool)(sh4->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);
    bool carry_bit;
    *dst_reg = sub_flags((int32_t)*src_reg, (int32_t)*dst_reg,
                         carry_in, &carry_bit, NULL);

    if (carry_bit)
        sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
    else
        sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0011nnnnmmmm1011 0xf00f
#define INST_CONS_0011nnnnmmmm1011 0x300b

// SUBV Rm, Rn
// 0011nnnnmmmm1011
void sh4_inst_binary_subv_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0011nnnnmmmm1011, INST_CONS_0011nnnnmmmm1011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    // detect overflow using 64-bit math
    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    bool overflow_bit;
    *dst_reg = sub_flags((int32_t)*src_reg, (int32_t)*dst_reg,
                         false, NULL, &overflow_bit);

    if (overflow_bit)
        sh4->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
    else
        sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
}

#define INST_MASK_0010nnnnmmmm1001 0xf00f
#define INST_CONS_0010nnnnmmmm1001 0x2009

// AND Rm, Rn
// 0010nnnnmmmm1001
void sh4_inst_binary_and_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1001, INST_CONS_0010nnnnmmmm1001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) &= *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0110nnnnmmmm0111 0xf00f
#define INST_CONS_0110nnnnmmmm0111 0x6007

// NOT Rm, Rn
// 0110nnnnmmmm0111
void sh4_inst_binary_not_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0111, INST_CONS_0110nnnnmmmm0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = ~(*sh4_gen_reg(sh4, (inst >> 4) & 0xf));
}

#define INST_MASK_0010nnnnmmmm1011 0xf00f
#define INST_CONS_0010nnnnmmmm1011 0x200b

// OR Rm, Rn
// 0010nnnnmmmm1011
void sh4_inst_binary_or_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1011, INST_CONS_0010nnnnmmmm1011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) |= *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0010nnnnmmmm1000 0xf00f
#define INST_CONS_0010nnnnmmmm1000 0x2008

// TST Rm, Rn
// 0010nnnnmmmm1000
void sh4_inst_binary_tst_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1000, INST_CONS_0010nnnnmmmm1000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    reg32_t flag = !(*sh4_gen_reg(sh4, (inst >> 4) & 0xf) & *sh4_gen_reg(sh4, (inst >> 8) & 0xf)) <<
        SH4_SR_FLAG_T_SHIFT;
    sh4->reg[SH4_REG_SR] |= flag;
}

#define INST_MASK_0010nnnnmmmm1010 0xf00f
#define INST_CONS_0010nnnnmmmm1010 0x200a

// XOR Rm, Rn
// 0010nnnnmmmm1010
void sh4_inst_binary_xor_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm1010, INST_CONS_0010nnnnmmmm1010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) ^= *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_0100nnnnmmmm1100 0xf00f
#define INST_CONS_0100nnnnmmmm1100 0x400c

// SHAD Rm, Rn
// 0100nnnnmmmm1100
void sh4_inst_binary_shad_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnnmmmm1100, INST_CONS_0100nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *srcp = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dstp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t src = (int32_t)*srcp;
    int32_t dst = (int32_t)*dstp;

    if (src >= 0) {
        dst <<= src;
    } else {
        dst >>= -src;
    }

    *dstp = dst;
}

#define INST_MASK_0100nnnnmmmm1101 0xf00f
#define INST_CONS_0100nnnnmmmm1101 0x400d

// SHLD Rm, Rn
// 0100nnnnmmmm1101
void sh4_inst_binary_shld_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnnmmmm1101, INST_CONS_0100nnnnmmmm1101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *srcp = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    reg32_t *dstp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t src = (int32_t)*srcp;
    uint32_t dst = (int32_t)*dstp;

    if (src >= 0) {
        dst <<= src;
    } else {
        dst >>= -src;
    }

    *dstp = dst;
}

#define INST_MASK_0100mmmm1nnn1110 0xf08f
#define INST_CONS_0100mmmm1nnn1110 0x408e

// LDC Rm, Rn_BANK
// 0100mmmm1nnn1110
void sh4_inst_binary_ldc_gen_bank(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm1nnn1110, INST_CONS_0100mmmm1nnn1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_bank_reg(sh4, (inst >> 4) & 0x7) =
        *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm1nnn0111 0xf08f
#define INST_CONS_0100mmmm1nnn0111 0x4087

// LDC.L @Rm+, Rn_BANK
// 0100mmmm1nnn0111
void sh4_inst_binary_ldcl_indgeninc_bank(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm1nnn0111, INST_CONS_0100mmmm1nnn0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

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

    src_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    val = memory_map_read_32(sh4->mem.map, *src_reg);

    (*src_reg) += 4;
    *sh4_bank_reg(sh4, (inst >> 4) & 0x7) = val;
}

#define INST_MASK_0000nnnn1mmm0010 0xf08f
#define INST_CONS_0000nnnn1mmm0010 0x0082

// STC Rm_BANK, Rn
// 0000nnnn1mmm0010
void sh4_inst_binary_stc_bank_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn1mmm0010, INST_CONS_0000nnnn1mmm0010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = *sh4_bank_reg(sh4, (inst >> 4) & 0x7);
}

#define INST_MASK_0100nnnn1mmm0011 0xf08f
#define INST_CONS_0100nnnn1mmm0011 0x4083

// STC.L Rm_BANK, @-Rn
// 0100nnnn1mmm0011
void sh4_inst_binary_stcl_bank_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn1mmm0011, INST_CONS_0100nnnn1mmm0011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

#ifdef ENABLE_SH4_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        error_set_feature("CPU exception for using a "
                          "privileged exception in an "
                          "unprivileged mode");
        SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
    }
#endif

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t src_val = *sh4_bank_reg(sh4, (inst >> 4) & 0x7);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, src_val);

    *addr_reg = addr;
}

#define INST_MASK_0100mmmm00001010 0xf0ff
#define INST_CONS_0100mmmm00001010 0x400a

// LDS Rm, MACH
// 0100mmmm00001010
void sh4_inst_binary_lds_gen_mach(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00001010, INST_CONS_0100mmmm00001010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_MACH] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0100mmmm00011010 0xf0ff
#define INST_CONS_0100mmmm00011010 0x401a

// LDS Rm, MACL
// 0100mmmm00011010
void sh4_inst_binary_lds_gen_macl(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00011010, INST_CONS_0100mmmm00011010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_MACL] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0000nnnn00001010 0xf0ff
#define INST_CONS_0000nnnn00001010 0x000a

// STS MACH, Rn
// 0000nnnn00001010
void sh4_inst_binary_sts_mach_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00001010, INST_CONS_0000nnnn00001010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_MACH];
}

#define INST_MASK_0000nnnn00011010 0xf0ff
#define INST_CONS_0000nnnn00011010 0x001a

// STS MACL, Rn
// 0000nnnn00011010
void sh4_inst_binary_sts_macl_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00011010, INST_CONS_0000nnnn00011010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_MACL];
}

#define INST_MASK_0100mmmm00101010 0xf0ff
#define INST_CONS_0100mmmm00101010 0x402a

// LDS Rm, PR
// 0100mmmm00101010
void sh4_inst_binary_lds_gen_pr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00101010, INST_CONS_0100mmmm00101010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4->reg[SH4_REG_PR] = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
}

#define INST_MASK_0000nnnn00101010 0xf0ff
#define INST_CONS_0000nnnn00101010 0x002a

// STS PR, Rn
// 0000nnnn00101010
void sh4_inst_binary_sts_pr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn00101010, INST_CONS_0000nnnn00101010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_PR];
}

#define INST_MASK_0100mmmm00000110 0xf0ff
#define INST_CONS_0100mmmm00000110 0x4006

// LDS.L @Rm+, MACH
// 0100mmmm00000110
void sh4_inst_binary_ldsl_indgeninc_mach(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00000110, INST_CONS_0100mmmm00000110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    val = memory_map_read_32(sh4->mem.map, *addr_reg);

    sh4->reg[SH4_REG_MACH] = val;

    *addr_reg += 4;
}

#define INST_MASK_0100mmmm00010110 0xf0ff
#define INST_CONS_0100mmmm00010110 0x4016

// LDS.L @Rm+, MACL
// 0100mmmm00010110
void sh4_inst_binary_ldsl_indgeninc_macl(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00010110, INST_CONS_0100mmmm00010110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    val = memory_map_read_32(sh4->mem.map, *addr_reg);

    sh4->reg[SH4_REG_MACL] = val;

    *addr_reg += 4;
}

#define INST_MASK_0100mmmm00000010 0xf0ff
#define INST_CONS_0100mmmm00000010 0x4002

// STS.L MACH, @-Rn
// 0100mmmm00000010
void sh4_inst_binary_stsl_mach_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00000010, INST_CONS_0100mmmm00000010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_MACH]);

    *addr_reg = addr;
}

#define INST_MASK_0100mmmm00010010 0xf0ff
#define INST_CONS_0100mmmm00010010 0x4012

// STS.L MACL, @-Rn
// 0100mmmm00010010
void sh4_inst_binary_stsl_macl_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00010010, INST_CONS_0100mmmm00010010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_MACL]);

    *addr_reg = addr;
}

#define INST_MASK_0100mmmm00100110 0xf0ff
#define INST_CONS_0100mmmm00100110 0x4026

// LDS.L @Rm+, PR
// 0100mmmm00100110
void sh4_inst_binary_ldsl_indgeninc_pr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm00100110, INST_CONS_0100mmmm00100110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    val = memory_map_read_32(sh4->mem.map, *addr_reg);

    sh4->reg[SH4_REG_PR] = val;

    *addr_reg += 4;
}

#define INST_MASK_0100nnnn00100010 0xf0ff
#define INST_CONS_0100nnnn00100010 0x4022

// STS.L PR, @-Rn
// 0100nnnn00100010
void sh4_inst_binary_stsl_pr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn00100010, INST_CONS_0100nnnn00100010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_PR]);

    *addr_reg = addr;
}

#define INST_MASK_0010nnnnmmmm0000 0xf00f
#define INST_CONS_0010nnnnmmmm0000 0x2000

// MOV.B Rm, @Rn
// 0010nnnnmmmm0000
void sh4_inst_binary_movb_gen_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0000, INST_CONS_0010nnnnmmmm0000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint8_t mem_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_8(sh4->mem.map, addr, mem_val);
}

#define INST_MASK_0010nnnnmmmm0001 0xf00f
#define INST_CONS_0010nnnnmmmm0001 0x2001

// MOV.W Rm, @Rn
// 0010nnnnmmmm0001
void sh4_inst_binary_movw_gen_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0001, INST_CONS_0010nnnnmmmm0001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint16_t mem_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_16(sh4->mem.map, addr, mem_val);
}

#define INST_MASK_0010nnnnmmmm0010 0xf00f
#define INST_CONS_0010nnnnmmmm0010 0x2002

// MOV.L Rm, @Rn
// 0010nnnnmmmm0010
void sh4_inst_binary_movl_gen_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0010, INST_CONS_0010nnnnmmmm0010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint32_t mem_val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_32(sh4->mem.map, addr, mem_val);
}

#define INST_MASK_0110nnnnmmmm0000 0xf00f
#define INST_CONS_0110nnnnmmmm0000 0x6000

// MOV.B @Rm, Rn
// 0110nnnnmmmm0000
void sh4_inst_binary_movb_indgen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0000, INST_CONS_0110nnnnmmmm0000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int8_t mem_val;

    mem_val = memory_map_read_8(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)mem_val;
}

#define INST_MASK_0110nnnnmmmm0001 0xf00f
#define INST_CONS_0110nnnnmmmm0001 0x6001

// MOV.W @Rm, Rn
// 0110nnnnmmmm0001
void sh4_inst_binary_movw_indgen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0001, INST_CONS_0110nnnnmmmm0001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int16_t mem_val;

    mem_val = memory_map_read_16(sh4->mem.map, addr);
    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)mem_val;
}

#define INST_MASK_0110nnnnmmmm0010 0xf00f
#define INST_CONS_0110nnnnmmmm0010 0x6002

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
void sh4_inst_binary_movl_indgen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0010, INST_CONS_0110nnnnmmmm0010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int32_t mem_val;

    mem_val = memory_map_read_32(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = mem_val;
}

#define INST_MASK_0010nnnnmmmm0100 0xf00f
#define INST_CONS_0010nnnnmmmm0100 0x2004

// MOV.B Rm, @-Rn
// 0010nnnnmmmm0100
void sh4_inst_binary_movb_gen_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0100, INST_CONS_0010nnnnmmmm0100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int8_t val;

    reg32_t dst_reg_val = (*dst_reg) - 1;
    val = *src_reg;

    memory_map_write_8(sh4->mem.map, dst_reg_val, val);

    (*dst_reg) = dst_reg_val;
}

#define INST_MASK_0010nnnnmmmm0101 0xf00f
#define INST_CONS_0010nnnnmmmm0101 0x2005

// MOV.W Rm, @-Rn
// 0010nnnnmmmm0101
void sh4_inst_binary_movw_gen_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0101, INST_CONS_0010nnnnmmmm0101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int16_t val;

    reg32_t dst_reg_val = *dst_reg;
    dst_reg_val -= 2;
    val = *src_reg;

    memory_map_write_16(sh4->mem.map, dst_reg_val, val);

    *dst_reg = dst_reg_val;
}

#define INST_MASK_0010nnnnmmmm0110 0xf00f
#define INST_CONS_0010nnnnmmmm0110 0x2006

// MOV.L Rm, @-Rn
// 0010nnnnmmmm0110
void sh4_inst_binary_movl_gen_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0010nnnnmmmm0110, INST_CONS_0010nnnnmmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *dst_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *src_reg = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int32_t val;

    reg32_t dst_reg_val = *dst_reg;
    dst_reg_val -= 4;
    val = *src_reg;

    memory_map_write_32(sh4->mem.map, dst_reg_val, val);

    *dst_reg = dst_reg_val;
}

#define INST_MASK_0110nnnnmmmm0100 0xf00f
#define INST_CONS_0110nnnnmmmm0100 0x6004

// MOV.B @Rm+, Rn
// 0110nnnnmmmm0100
void sh4_inst_binary_movb_indgeninc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0100, INST_CONS_0110nnnnmmmm0100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned src_reg_no = (inst >> 4) & 0xf;
    unsigned dst_reg_no = (inst >> 8) & 0xf;

    reg32_t *src_reg = sh4_gen_reg(sh4, src_reg_no);
    reg32_t *dst_reg = sh4_gen_reg(sh4, dst_reg_no);
    int8_t val;

    reg32_t src_addr = *src_reg;
    val = memory_map_read_8(sh4->mem.map, src_addr);

    *dst_reg = (int32_t)val;

    if (src_reg_no != dst_reg_no)
        (*src_reg)++;
}

#define INST_MASK_0110nnnnmmmm0101 0xf00f
#define INST_CONS_0110nnnnmmmm0101 0x6005

// MOV.W @Rm+, Rn
// 0110nnnnmmmm0101
void sh4_inst_binary_movw_indgeninc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0101, INST_CONS_0110nnnnmmmm0101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned src_reg_no = (inst >> 4) & 0xf;
    unsigned dst_reg_no = (inst >> 8) & 0xf;

    reg32_t *src_reg = sh4_gen_reg(sh4, src_reg_no);
    reg32_t *dst_reg = sh4_gen_reg(sh4, dst_reg_no);
    int16_t val;

    reg32_t src_addr = *src_reg;
    val = memory_map_read_16(sh4->mem.map, src_addr);

    *dst_reg = (int32_t)val;

    if (src_reg_no != dst_reg_no)
        (*src_reg) += 2;
}

#define INST_MASK_0110nnnnmmmm0110 0xf00f
#define INST_CONS_0110nnnnmmmm0110 0x6006

// MOV.L @Rm+, Rn
// 0110nnnnmmmm0110
void sh4_inst_binary_movl_indgeninc_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0110nnnnmmmm0110, INST_CONS_0110nnnnmmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    unsigned src_reg_no = (inst >> 4) & 0xf;
    unsigned dst_reg_no = (inst >> 8) & 0xf;

    reg32_t *src_reg = sh4_gen_reg(sh4, src_reg_no);
    reg32_t *dst_reg = sh4_gen_reg(sh4, dst_reg_no);
    int32_t val;

    reg32_t src_addr = *src_reg;
    val = memory_map_read_32(sh4->mem.map, src_addr);

    *dst_reg = (int32_t)val;

    if (src_reg_no != dst_reg_no)
        (*src_reg) += 4;
}

#define INST_MASK_0000nnnnmmmm1111 0xf00f
#define INST_CONS_0000nnnnmmmm1111 0x000f

// MAC.L @Rm+, @Rn+
// 0000nnnnmmmm1111
void sh4_inst_binary_macl_indgeninc_indgeninc(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm1111, INST_CONS_0000nnnnmmmm1111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    static const int64_t MAX48 = 0x7fffffffffff;
    static const int64_t MIN48 = 0xffff800000000000;
    reg32_t *dst_addrp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *src_addrp = sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    reg32_t lhs, rhs;
    lhs = memory_map_read_32(sh4->mem.map, *dst_addrp);
    rhs = memory_map_read_32(sh4->mem.map, *src_addrp);

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
}

#define INST_MASK_0100nnnnmmmm1111 0xf00f
#define INST_CONS_0100nnnnmmmm1111 0x400f

// MAC.W @Rm+, @Rn+
// 0100nnnnmmmm1111
void sh4_inst_binary_macw_indgeninc_indgeninc(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnnmmmm1111, INST_CONS_0100nnnnmmmm1111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    static const int32_t MAX32 = 0x7fffffff;
    static const int32_t MIN32 = 0x80000000;
    reg32_t *dst_addrp = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t *src_addrp = sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    int16_t lhs, rhs;
    lhs = memory_map_read_16(sh4->mem.map, *dst_addrp);
    rhs = memory_map_read_16(sh4->mem.map, *src_addrp);

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
}

#define INST_MASK_10000000nnnndddd 0xff00
#define INST_CONS_10000000nnnndddd 0x8000

// MOV.B R0, @(disp, Rn)
// 10000000nnnndddd
void sh4_inst_binary_movb_r0_binind_disp_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10000000nnnndddd, INST_CONS_10000000nnnndddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst & 0xf) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int8_t val = *sh4_gen_reg(sh4, 0);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_10000001nnnndddd 0xff00
#define INST_CONS_10000001nnnndddd 0x8100

// MOV.W R0, @(disp, Rn)
// 10000001nnnndddd
void sh4_inst_binary_movw_r0_binind_disp_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10000001nnnndddd, INST_CONS_10000001nnnndddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = ((inst & 0xf) << 1) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int16_t val = *sh4_gen_reg(sh4, 0);

    memory_map_write_16(sh4->mem.map, addr, val);
}

#define INST_MASK_0001nnnnmmmmdddd 0xf000
#define INST_CONS_0001nnnnmmmmdddd 0x1000

// MOV.L Rm, @(disp, Rn)
// 0001nnnnmmmmdddd
void sh4_inst_binary_movl_gen_binind_disp_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0001nnnnmmmmdddd, INST_CONS_0001nnnnmmmmdddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = ((inst & 0xf) << 2) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    int32_t val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_32(sh4->mem.map, addr, val);
}

#define INST_MASK_10000100mmmmdddd 0xff00
#define INST_CONS_10000100mmmmdddd 0x8400

// MOV.B @(disp, Rm), R0
// 10000100mmmmdddd
void sh4_inst_binary_movb_binind_disp_gen_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10000100mmmmdddd, INST_CONS_10000100mmmmdddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst & 0xf) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int8_t val;

    val = memory_map_read_8(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, 0) = (int32_t)val;
}

#define INST_MASK_10000101mmmmdddd 0xff00
#define INST_CONS_10000101mmmmdddd 0x8500

// MOV.W @(disp, Rm), R0
// 10000101mmmmdddd
void sh4_inst_binary_movw_binind_disp_gen_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_10000101mmmmdddd, INST_CONS_10000101mmmmdddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = ((inst & 0xf) << 1) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int16_t val;

    val = memory_map_read_16(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, 0) = (int32_t)val;
}

#define INST_MASK_0101nnnnmmmmdddd 0xf000
#define INST_CONS_0101nnnnmmmmdddd 0x5000

// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
void sh4_inst_binary_movl_binind_disp_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0101nnnnmmmmdddd, INST_CONS_0101nnnnmmmmdddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = ((inst & 0xf) << 2) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int32_t val;

    val = memory_map_read_32(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = val;
}

#define INST_MASK_0000nnnnmmmm0100 0xf00f
#define INST_CONS_0000nnnnmmmm0100 0x0004

// MOV.B Rm, @(R0, Rn)
// 0000nnnnmmmm0100
void sh4_inst_binary_movb_gen_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm0100, INST_CONS_0000nnnnmmmm0100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint8_t val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_0000nnnnmmmm0101 0xf00f
#define INST_CONS_0000nnnnmmmm0101 0x0005

// MOV.W Rm, @(R0, Rn)
// 0000nnnnmmmm0101
void sh4_inst_binary_movw_gen_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm0101, INST_CONS_0000nnnnmmmm0101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint16_t val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_16(sh4->mem.map, addr, val);
}

#define INST_MASK_0000nnnnmmmm0110 0xf00f
#define INST_CONS_0000nnnnmmmm0110 0x0006

// MOV.L Rm, @(R0, Rn)
// 0000nnnnmmmm0110
void sh4_inst_binary_movl_gen_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm0110, INST_CONS_0000nnnnmmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    uint32_t val = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);

    memory_map_write_32(sh4->mem.map, addr, val);
}

#define INST_MASK_0000nnnnmmmm1100 0xf00f
#define INST_CONS_0000nnnnmmmm1100 0x000c

// MOV.B @(R0, Rm), Rn
// 0000nnnnmmmm1100
void sh4_inst_binary_movb_binind_r0_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm1100, INST_CONS_0000nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int8_t val;

    val = memory_map_read_8(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)val;
}

#define INST_MASK_0000nnnnmmmm1101 0xf00f
#define INST_CONS_0000nnnnmmmm1101 0x000d

// MOV.W @(R0, Rm), Rn
// 0000nnnnmmmm1101
void sh4_inst_binary_movw_binind_r0_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm1101, INST_CONS_0000nnnnmmmm1101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int16_t val;

    val = memory_map_read_16(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = (int32_t)val;
}

#define INST_MASK_0000nnnnmmmm1110 0xf00f
#define INST_CONS_0000nnnnmmmm1110 0x000e

// MOV.L @(R0, Rm), Rn
// 0000nnnnmmmm1110
void sh4_inst_binary_movl_binind_r0_gen_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnnmmmm1110, INST_CONS_0000nnnnmmmm1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    int32_t val;

    val = memory_map_read_32(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = val;
}

#define INST_MASK_11000000dddddddd 0xff00
#define INST_CONS_11000000dddddddd 0xc000

// MOV.B R0, @(disp, GBR)
// 11000000dddddddd
void sh4_inst_binary_movb_r0_binind_disp_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000000dddddddd, INST_CONS_11000000dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = inst_imm8(inst) + sh4->reg[SH4_REG_GBR];
    int8_t val = *sh4_gen_reg(sh4, 0);

    memory_map_write_8(sh4->mem.map, addr, val);
}

#define INST_MASK_11000001dddddddd 0xff00
#define INST_CONS_11000001dddddddd 0xc100

// MOV.W R0, @(disp, GBR)
// 11000001dddddddd
void sh4_inst_binary_movw_r0_binind_disp_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000001dddddddd, INST_CONS_11000001dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 1) + sh4->reg[SH4_REG_GBR];
    int16_t val = *sh4_gen_reg(sh4, 0);

    memory_map_write_16(sh4->mem.map, addr, val);
}

#define INST_MASK_11000010dddddddd 0xff00
#define INST_CONS_11000010dddddddd 0xc200

// MOV.L R0, @(disp, GBR)
// 11000010dddddddd
void sh4_inst_binary_movl_r0_binind_disp_gbr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000010dddddddd, INST_CONS_11000010dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 2) + sh4->reg[SH4_REG_GBR];
    int32_t val = *sh4_gen_reg(sh4, 0);

    memory_map_write_32(sh4->mem.map, addr, val);
}

#define INST_MASK_11000100dddddddd 0xff00
#define INST_CONS_11000100dddddddd 0xc400

// MOV.B @(disp, GBR), R0
// 11000100dddddddd
void sh4_inst_binary_movb_binind_disp_gbr_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000100dddddddd, INST_CONS_11000100dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = inst_imm8(inst) + sh4->reg[SH4_REG_GBR];
    int8_t val;

    val = memory_map_read_8(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, 0) = (int32_t)val;
}

#define INST_MASK_11000101dddddddd 0xff00
#define INST_CONS_11000101dddddddd 0xc500

// MOV.W @(disp, GBR), R0
// 11000101dddddddd
void sh4_inst_binary_movw_binind_disp_gbr_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000101dddddddd, INST_CONS_11000101dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 1) + sh4->reg[SH4_REG_GBR];
    int16_t val;

    val = memory_map_read_16(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, 0) = (int32_t)val;
}

#define INST_MASK_11000110dddddddd 0xff00
#define INST_CONS_11000110dddddddd 0xc600

// MOV.L @(disp, GBR), R0
// 11000110dddddddd
void sh4_inst_binary_movl_binind_disp_gbr_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000110dddddddd, INST_CONS_11000110dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    addr32_t addr = (inst_imm8(inst) << 2) + sh4->reg[SH4_REG_GBR];
    int32_t val;

    val = memory_map_read_32(sh4->mem.map, addr);

    *sh4_gen_reg(sh4, 0) = val;
}

#define INST_MASK_11000111dddddddd 0xff00
#define INST_CONS_11000111dddddddd 0xc700

// MOVA @(disp, PC), R0
// 11000111dddddddd
void sh4_inst_binary_mova_binind_disp_pc_r0(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_11000111dddddddd, INST_CONS_11000111dddddddd);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    /*
     * The assembly for this one is a bit of a misnomer.
     * even though it has the @ indirection symbol around (disp, PC), it
     * actually just loads that address into R0 instead of the value at that
     * address.  It is roughly analagous to the x86 architectures lea family of
     * opcodes.
     */
    *sh4_gen_reg(sh4, 0) = (inst_imm8(inst) << 2) + (sh4->reg[SH4_REG_PC] & ~3) + 4;
}

#define INST_MASK_0000nnnn11000011 0xf0ff
#define INST_CONS_0000nnnn11000011 0x00c3

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
void sh4_inst_binary_movcal_r0_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn11000011, INST_CONS_0000nnnn11000011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t src_val = *sh4_gen_reg(sh4, 0);
    addr32_t vaddr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    memory_map_write_32(sh4->mem.map, vaddr, src_val);
}

#define INST_MASK_1111nnnn10001101 0xf0ff
#define INST_CONS_1111nnnn10001101 0xf08d

// FLDI0 FRn
// 1111nnnn10001101
void sh4_inst_unary_fldi0_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn10001101, INST_CONS_1111nnnn10001101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    *sh4_fpu_fr(sh4, (inst >> 8) & 0xf) = 0.0f;
}

#define INST_MASK_1111nnnn10011101 0xf0ff
#define INST_CONS_1111nnnn10011101 0xf09d

// FLDI1 Frn
// 1111nnnn10011101
void sh4_inst_unary_fldi1_fr(void *cpu, cpu_inst_param inst) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_INST(inst, INST_MASK_1111nnnn10011101, INST_CONS_1111nnnn10011101);
    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    *sh4_fpu_fr(sh4, (inst >> 8) & 0xf) = 1.0f;
}

#define INST_MASK_1111nnnnmmmm1100 0xf00f
#define INST_CONS_1111nnnnmmmm1100 0xf00c

// FMOV FRm, FRn
// 1111nnnnmmmm1100
void sh4_inst_binary_fmov_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1100, INST_CONS_1111nnnnmmmm1100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    *sh4_fpu_fr(sh4, (inst >> 8) & 0xf) = *sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
}

#define INST_MASK_1111nnnnmmmm1000 0xf00f
#define INST_CONS_1111nnnnmmmm1000 0xf008

// FMOV.S @Rm, FRn
// 1111nnnnmmmm1000
void sh4_inst_binary_fmovs_indgen_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1000, INST_CONS_1111nnnnmmmm1000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    reg32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    float *dst_ptr = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    *dst_ptr = memory_map_read_float(sh4->mem.map, addr);
}

#define INST_MASK_1111nnnnmmmm0110 0xf00f
#define INST_CONS_1111nnnnmmmm0110 0xf006

// FMOV.S @(R0,Rm), FRn
// 1111nnnnmmmm0110
void sh4_inst_binary_fmovs_binind_r0_gen_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0110, INST_CONS_1111nnnnmmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    reg32_t addr = *sh4_gen_reg(sh4, 0) + * sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    float *dst_ptr = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    *dst_ptr = memory_map_read_float(sh4->mem.map, addr);
}

#define INST_MASK_1111nnnnmmmm1001 0xf00f
#define INST_CONS_1111nnnnmmmm1001 0xf009

// FMOV.S @Rm+, FRn
// 1111nnnnmmmm1001
void sh4_inst_binary_fmovs_indgeninc_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1001, INST_CONS_1111nnnnmmmm1001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    float *dst_ptr = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    *dst_ptr = memory_map_read_float(sh4->mem.map, *addr_p);

    *addr_p += 4;
}

#define INST_MASK_1111nnnnmmmm1010 0xf00f
#define INST_CONS_1111nnnnmmmm1010 0xf00a

// FMOV.S FRm, @Rn
// 1111nnnnmmmm1010
void sh4_inst_binary_fmovs_fr_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1010, INST_CONS_1111nnnnmmmm1010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    reg32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    float *src_p = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);

    memory_map_write_float(sh4->mem.map, addr, *src_p);
}

#define INST_MASK_1111nnnnmmmm1011 0xf00f
#define INST_CONS_1111nnnnmmmm1011 0xf00b

// FMOV.S FRm, @-Rn
// 1111nnnnmmmm1011
void sh4_inst_binary_fmovs_fr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1011, INST_CONS_1111nnnnmmmm1011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t addr = *addr_p - 4;
    float *src_p = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);

    memory_map_write_float(sh4->mem.map, addr, *src_p);

    *addr_p = addr;
}

#define INST_MASK_1111nnnnmmmm0111 0xf00f
#define INST_CONS_1111nnnnmmmm0111 0xf007

// FMOV.S FRm, @(R0, Rn)
// 1111nnnnmmmm0111
void sh4_inst_binary_fmovs_fr_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0111, INST_CONS_1111nnnnmmmm0111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, 0);

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    float *src_p = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);

    memory_map_write_float(sh4->mem.map, addr, *src_p);
}

#define INST_MASK_1111nnn0mmm01100 0xf11f
#define INST_CONS_1111nnn0mmm01100 0xf00c

// FMOV DRm, DRn
// 1111nnn0mmm01100
void sh4_inst_binary_fmov_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm01100, INST_CONS_1111nnn0mmm01100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    *sh4_fpu_dr(sh4, (inst >> 9) & 0x7) = *sh4_fpu_dr(sh4, (inst >> 5) & 0x7);
}

#define INST_MASK_1111nnn0mmmm1000 0xf10f
#define INST_CONS_1111nnn0mmmm1000 0xf008

// FMOV @Rm, DRn
// 1111nnn0mmmm1000
void sh4_inst_binary_fmov_indgen_dr(void *cpu, cpu_inst_param inst) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_INST(inst, INST_MASK_1111nnn0mmmm1000, INST_CONS_1111nnn0mmmm1000);
    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    double *dst_ptr = sh4_fpu_dr(sh4, (inst >> 9) & 0x7);

    *dst_ptr = memory_map_read_double(sh4->mem.map, addr);
}

#define INST_MASK_1111nnn0mmmm0110 0xf10f
#define INST_CONS_1111nnn0mmmm0110 0xf006

// FMOV @(R0, Rm), DRn
// 1111nnn0mmmm0110
void sh4_inst_binary_fmov_binind_r0_gen_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmmm0110, INST_CONS_1111nnn0mmmm0110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t addr = *sh4_gen_reg(sh4, 0) + * sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    double *dst_ptr = sh4_fpu_dr(sh4, (inst >> 9) & 0x7);

    *dst_ptr = memory_map_read_double(sh4->mem.map, addr);
}

#define INST_MASK_1111nnn0mmmm1001 0xf10f
#define INST_CONS_1111nnn0mmmm1001 0xf009

// FMOV @Rm+, DRn
// 1111nnn0mmmm1001
void sh4_inst_binary_fmov_indgeninc_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmmm1001, INST_CONS_1111nnn0mmmm1001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    double *dst_ptr = sh4_fpu_dr(sh4, (inst >> 9) & 0x7);

    *dst_ptr = memory_map_read_double(sh4->mem.map, *addr_p);

    *addr_p += 8;
}

#define INST_MASK_1111nnnnmmm01010 0xf01f
#define INST_CONS_1111nnnnmmm01010 0xf00a

// FMOV DRm, @Rn
// 1111nnnnmmm01010
void sh4_inst_binary_fmov_dr_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm01010, INST_CONS_1111nnnnmmm01010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t addr = *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    double *src_p = sh4_fpu_dr(sh4, (inst >> 5) & 0x7);

    memory_map_write_double(sh4->mem.map, addr, *src_p);
}

#define INST_MASK_1111nnnnmmm01011 0xf01f
#define INST_CONS_1111nnnnmmm01011 0xf00b

// FMOV DRm, @-Rn
// 1111nnnnmmm01011
void sh4_inst_binary_fmov_dr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm01011, INST_CONS_1111nnnnmmm01011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t addr = *addr_p - 8;
    double *src_p = sh4_fpu_dr(sh4, (inst >> 5) & 0x7);

    memory_map_write_double(sh4->mem.map, addr, *src_p);

    *addr_p = addr;
}

#define INST_MASK_1111nnnnmmm00111 0xf01f
#define INST_CONS_1111nnnnmmm00111 0xf007

// FMOV DRm, @(R0, Rn)
// 1111nnnnmmm00111
void sh4_inst_binary_fmov_dr_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm00111, INST_CONS_1111nnnnmmm00111);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    addr32_t addr = *sh4_gen_reg(sh4, 0) + *sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    double *src_p = sh4_fpu_dr(sh4, (inst >> 5) & 0x7);

    memory_map_write_double(sh4->mem.map, addr, *src_p);
}

#define INST_MASK_1111mmmm00011101 0xf0ff
#define INST_CONS_1111mmmm00011101 0xf01d

// FLDS FRm, FPUL
// 1111mmmm00011101
void sh4_inst_binary_flds_fr_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111mmmm00011101, INST_CONS_1111mmmm00011101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    float *src_reg = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    memcpy(sh4->reg + SH4_REG_FPUL, src_reg, sizeof(sh4->reg[SH4_REG_FPUL]));
}

#define INST_MASK_1111nnnn00001101 0xf0ff
#define INST_CONS_1111nnnn00001101 0xf00d

// FSTS FPUL, FRn
// 1111nnnn00001101
void sh4_inst_binary_fsts_fpul_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn00001101, INST_CONS_1111nnnn00001101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    float *dst_reg = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    memcpy(dst_reg, sh4->reg + SH4_REG_FPUL, sizeof(*dst_reg));
}

#define INST_MASK_1111nnnn01011101 0xf0ff
#define INST_CONS_1111nnnn01011101 0xf05d

// FABS FRn
// 1111nnnn01011101
void sh4_inst_unary_fabs_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn01011101, INST_CONS_1111nnnn01011101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    float *regp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);
    *regp = fabs(*regp);
}

#define INST_MASK_1111nnnnmmmm0000 0xf00f
#define INST_CONS_1111nnnnmmmm0000 0xf000

// FADD FRm, FRn
// 1111nnnnmmmm0000
void sh4_inst_binary_fadd_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0000, INST_CONS_1111nnnnmmmm0000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float *srcp = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
    float *dstp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC

    if (issignaling(src) || issignaling(dst)) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }

    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_SUBNORMAL || dst_class == FP_SUBNORMAL) {
        sh4_fpu_error(sh4);
        return;
    }

    if (src_class == FP_INFINITE && dst_class == FP_INFINITE) {
        sh4_fpu_error(sh4);
        return;
    }

#endif

    *dstp = dst + src;
}

#define INST_MASK_1111nnnnmmmm0100 0xf00f
#define INST_CONS_1111nnnnmmmm0100 0xf004

// FCMP/EQ FRm, FRn
// 1111nnnnmmmm0100
void sh4_inst_binary_fcmpeq_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0100, INST_CONS_1111nnnnmmmm0100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float *srcp = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
    float *dstp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC
    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_NAN || dst_class == FP_NAN) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }
#endif

    unsigned t_flag = (dst == src);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (t_flag << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_1111nnnnmmmm0101 0xf00f
#define INST_CONS_1111nnnnmmmm0101 0xf005

// FCMP/GT FRm, FRn
// 1111nnnnmmmm0101
void sh4_inst_binary_fcmpgt_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0101, INST_CONS_1111nnnnmmmm0101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float *srcp = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
    float *dstp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC
    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_NAN || dst_class == FP_NAN) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }
#endif

    unsigned t_flag = (dst > src);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (t_flag << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_1111nnnnmmmm0011 0xf00f
#define INST_CONS_1111nnnnmmmm0011 0xf003

// FDIV FRm, FRn
// 1111nnnnmmmm0011
void sh4_inst_binary_fdiv_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0011, INST_CONS_1111nnnnmmmm0011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float *srcp = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
    float *dstp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC
    if (issignaling(src) || issignaling(dst)) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }

    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_SUBNORMAL || dst_class == FP_SUBNORMAL) {
        sh4_fpu_error(sh4);
        return;
    }

    if (src_class == FP_ZERO && dst_class == FP_ZERO) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }

    if (src_class == FP_ZERO) {
        sh4->reg[SH4_REG_FPSCR] |=
            (SH4_FPSCR_FLAG_Z_MASK | SH4_FPSCR_CAUSE_Z_MASK);
        if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_ENABLE_Z_MASK) {
            sh4_set_exception(sh4, SH4_EXCP_FPU);
            return;
        }
    }
#endif

    *dstp = dst / src;
}

#define INST_MASK_1111nnnn00101101 0xf0ff
#define INST_CONS_1111nnnn00101101 0xf02d

// FLOAT FPUL, FRn
// 1111nnnn00101101
void sh4_inst_binary_float_fpul_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn00101101, INST_CONS_1111nnnn00101101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    float *dst_reg = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    *dst_reg = (float)((int32_t)sh4->reg[SH4_REG_FPUL]);
}

#define INST_MASK_1111nnnnmmmm1110 0xf00f
#define INST_CONS_1111nnnnmmmm1110 0xf00e

// FMAC FR0, FRm, FRn
// 1111nnnnmmmm1110
void sh4_inst_trinary_fmac_fr0_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm1110, INST_CONS_1111nnnnmmmm1110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float in0, in1, in2;

    memcpy(&in0, sh4->reg + SH4_REG_FR0, sizeof(in0));
    memcpy(&in1, sh4->reg + SH4_REG_FR0 + ((inst >> 4) & 0xf), sizeof(in1));
    memcpy(&in2, sh4->reg + SH4_REG_FR0 + ((inst >> 8) & 0xf), sizeof(in2));

    in2 = in0 * in1 + in2;

    memcpy(sh4->reg + SH4_REG_FR0 + ((inst >> 8) & 0xf), &in2, sizeof(in2));
}

#define INST_MASK_1111nnnnmmmm0010 0xf00f
#define INST_CONS_1111nnnnmmmm0010 0xf002

// FMUL FRm, FRn
// 1111nnnnmmmm0010
void sh4_inst_binary_fmul_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0010, INST_CONS_1111nnnnmmmm0010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    float *srcp = sh4_fpu_fr(sh4, (inst >> 4) & 0xf);
    float *dstp = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC
    if (issignaling(src) || issignaling(dst)) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }

    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_SUBNORMAL || dst_class == FP_SUBNORMAL) {
        sh4_fpu_error(sh4);
        return;
    }

    if ((src_class == FP_ZERO && dst_class == FP_INFINITE) ||
        (src_class == FP_INFINITE && dst_class == FP_ZERO)) {
        sh4_fr_invalid(sh4, inst.fr_dst);
        return;
    }
#endif

    *dstp = src * dst;
}

#define INST_MASK_1111nnnn01001101 0xf0ff
#define INST_CONS_1111nnnn01001101 0xf04d

// FNEG FRn
// 1111nnnn01001101
void sh4_inst_unary_fneg_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn01001101, INST_CONS_1111nnnn01001101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    int fr_reg = (inst >> 8) & 0xf;

    *sh4_fpu_fr(sh4, fr_reg) = -*sh4_fpu_fr(sh4, fr_reg);
}

#define INST_MASK_1111nnnn01101101 0xf0ff
#define INST_CONS_1111nnnn01101101 0xf06d

// FSQRT FRn
// 1111nnnn01101101
void sh4_inst_unary_fsqrt_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn01101101, INST_CONS_1111nnnn01101101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    int fr_reg = (inst >> 8) & 0xf;

    // TODO: check for negative input and raise an FPU exception when it happens
    float in;
    memcpy(&in, sh4->reg + SH4_REG_FR0 + fr_reg, sizeof(in));

    float out = sqrt(in);

    memcpy(sh4->reg + SH4_REG_FR0 + fr_reg, &out, sizeof(out));
}

#define INST_MASK_1111nnnnmmmm0001 0xf00f
#define INST_CONS_1111nnnnmmmm0001 0xf001

// FSUB FRm, FRn
// 1111nnnnmmmm0001
void sh4_inst_binary_fsub_fr_fr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmmm0001, INST_CONS_1111nnnnmmmm0001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    int fr_src = (inst >> 4) & 0xf;
    int fr_dst = (inst >> 8) & 0xf;

    float *srcp = sh4_fpu_fr(sh4, fr_src);
    float *dstp = sh4_fpu_fr(sh4, fr_dst);

    float src = *srcp;
    float dst = *dstp;

#ifdef SH4_FPU_PEDANTIC

    if (issignaling(src) || issignaling(dst)) {
        sh4_fr_invalid(sh4, fr_dst);
        return;
    }

    int src_class = fpclassify(src);
    int dst_class = fpclassify(dst);

    if (src_class == FP_SUBNORMAL || dst_class == FP_SUBNORMAL) {
        sh4_fpu_error(sh4);
        return;
    }

    if (src_class == FP_INFINITE && dst_class == FP_INFINITE) {
        sh4_fpu_error(sh4);
        return;
    }

#endif

    *dstp = dst - src;
}

#define INST_MASK_1111mmmm00111101 0xf0ff
#define INST_CONS_1111mmmm00111101 0xf03d

// FTRC FRm, FPUL
// 1111mmmm00111101
void sh4_inst_binary_ftrc_fr_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111mmmm00111101, INST_CONS_1111mmmm00111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    float const *val_in_p = sh4_fpu_fr(sh4, (inst >> 8) & 0xf);
    float val = *val_in_p;
    int32_t val_int;

    sh4_fpu_clear_cause(sh4);

    int round_mode = fegetround();
    fesetround(FE_TOWARDZERO);

    val_int = val;
    memcpy(sh4->reg + SH4_REG_FPUL, &val_int, sizeof(sh4->reg[SH4_REG_FPUL]));

    fesetround(round_mode);
}

#define INST_MASK_1111nnn001011101 0xf1ff
#define INST_CONS_1111nnn001011101 0xf05d

// FABS DRn
// 1111nnn001011101
void sh4_inst_unary_fabs_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn001011101, INST_CONS_1111nnn001011101);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001011101");
    error_set_opcode_name("FABS DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnn0mmm00000 0xf11f
#define INST_CONS_1111nnn0mmm00000 0xf000

// FADD DRm, DRn
// 1111nnn0mmm00000
void sh4_inst_binary_fadd_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00000, INST_CONS_1111nnn0mmm00000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    sh4_fpu_clear_cause(sh4);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    double src = sh4_read_double(sh4, dr_src * 2);
    double dst = sh4_read_double(sh4, dr_dst * 2);

    dst += src;

    sh4_write_double(sh4, dr_dst * 2, dst);
}

#define INST_MASK_1111nnn0mmm00100 0xf11f
#define INST_CONS_1111nnn0mmm00100 0xf004

// FCMP/EQ DRm, DRn
// 1111nnn0mmm00100
void sh4_inst_binary_fcmpeq_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00100, INST_CONS_1111nnn0mmm00100);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    sh4_fpu_clear_cause(sh4);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    double src = sh4_read_double(sh4, dr_src * 2);
    double dst = sh4_read_double(sh4, dr_dst * 2);

    unsigned t_flag = (dst == src);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (t_flag << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_1111nnn0mmm00101 0xf11f
#define INST_CONS_1111nnn0mmm00101 0xf005

// FCMP/GT DRm, DRn
// 1111nnn0mmm00101
void sh4_inst_binary_fcmpgt_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00101, INST_CONS_1111nnn0mmm00101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    sh4_fpu_clear_cause(sh4);

    double src = sh4_read_double(sh4, ((inst >> 5) & 0x7) * 2);
    double dst = sh4_read_double(sh4, ((inst >> 9) & 0x7) * 2);

    unsigned t_flag = (dst > src);
    sh4->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
    sh4->reg[SH4_REG_SR] |= (t_flag << SH4_SR_FLAG_T_SHIFT);
}

#define INST_MASK_1111nnn0mmm00011 0xf11f
#define INST_CONS_1111nnn0mmm00011 0xf003

// FDIV DRm, DRn
// 1111nnn0mmm00011
void sh4_inst_binary_fdiv_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00011, INST_CONS_1111nnn0mmm00011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    sh4_fpu_clear_cause(sh4);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    double src = sh4_read_double(sh4, dr_src * 2);
    double dst = sh4_read_double(sh4, dr_dst * 2);

    dst /= src;

    sh4_write_double(sh4, dr_dst * 2, dst);
}

#define INST_MASK_1111mmm010111101 0xf1ff
#define INST_CONS_1111mmm010111101 0xf0bd

// FCNVDS DRm, FPUL
// 1111mmm010111101
void sh4_inst_binary_fcnvds_dr_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111mmm010111101, INST_CONS_1111mmm010111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    sh4_fpu_clear_cause(sh4);

    double in_val = sh4_read_double(sh4, ((inst >> 9) & 0x7) * 2);
    float out_val = in_val;

    memcpy(sh4->reg + SH4_REG_FPUL, &out_val, sizeof(sh4->reg[SH4_REG_FPUL]));
}

#define INST_MASK_1111nnn010101101 0xf1ff
#define INST_CONS_1111nnn010101101 0xf0ad

// FCNVSD FPUL, DRn
// 1111nnn010101101
void sh4_inst_binary_fcnvsd_fpul_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn010101101, INST_CONS_1111nnn010101101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    sh4_fpu_clear_cause(sh4);

    float in_val;
    memcpy(&in_val, sh4->reg + SH4_REG_FPUL, sizeof(in_val));
    double out_val = in_val;

    sh4_write_double(sh4, ((inst >> 9) & 0x7) * 2, out_val);
}

#define INST_MASK_1111nnn000101101 0xf1ff
#define INST_CONS_1111nnn000101101 0xf02d

// FLOAT FPUL, DRn
// 1111nnn000101101
void sh4_inst_binary_float_fpul_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn000101101, INST_CONS_1111nnn000101101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    sh4_write_double(sh4, ((inst >> 9) & 0x7) * 2,
                     (double)((int64_t)(int32_t)sh4->reg[SH4_REG_FPUL]));
}

#define INST_MASK_1111nnn0mmm00010 0xf11f
#define INST_CONS_1111nnn0mmm00010 0xf002

// FMUL DRm, DRn
// 1111nnn0mmm00010
void sh4_inst_binary_fmul_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00010, INST_CONS_1111nnn0mmm00010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);


    sh4_fpu_clear_cause(sh4);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    double src = sh4_read_double(sh4, dr_src * 2);
    double dst = sh4_read_double(sh4, dr_dst * 2);

    dst *= src;

    sh4_write_double(sh4, dr_dst * 2, dst);
}

#define INST_MASK_1111nnn001001101 0xf1ff
#define INST_CONS_1111nnn001001101 0xf04d

// FNEG DRn
// 1111nnn001001101
void sh4_inst_unary_fneg_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn001001101, INST_CONS_1111nnn001001101);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001001101");
    error_set_opcode_name("FNEG DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnn001101101 0xf1ff
#define INST_CONS_1111nnn001101101 0xf06d

// FSQRT DRn
// 1111nnn001101101
void sh4_inst_unary_fsqrt_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn001101101, INST_CONS_1111nnn001101101);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn001101101");
    error_set_opcode_name("FSQRT DRn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnn0mmm00001 0xf11f
#define INST_CONS_1111nnn0mmm00001 0xf001

// FSUB DRm, DRn
// 1111nnn0mmm00001
void sh4_inst_binary_fsub_dr_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn0mmm00001, INST_CONS_1111nnn0mmm00001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);


    sh4_fpu_clear_cause(sh4);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    double src = sh4_read_double(sh4, dr_src * 2);
    double dst = sh4_read_double(sh4, dr_dst * 2);

    dst -= src;

    sh4_write_double(sh4, dr_dst * 2, dst);
}

#define INST_MASK_1111mmm000111101 0xf1ff
#define INST_CONS_1111mmm000111101 0xf03d

// FTRC DRm, FPUL
// 1111mmm000111101
void sh4_inst_binary_ftrc_dr_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111mmm000111101, INST_CONS_1111mmm000111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, SH4_FPSCR_PR_MASK);

    /*
     * TODO: The spec says there's some pretty complicated error-checking that
     * should be done here.  I'm just going to implement this the naive way
     * instead
     */
    double val_in = sh4_read_double(sh4, ((inst >> 9) & 0x7) * 2);
    int32_t val_int = val_in;

    int round_mode = fegetround();
    fesetround(FE_TOWARDZERO);

    sh4->reg[SH4_REG_FPUL] = val_int;

    fesetround(round_mode);
}

#define INST_MASK_1111nnn011111101 0xf1ff
#define INST_CONS_1111nnn011111101 0xf0fd

// FSCA FPUL, DRn
// 1111nnn011111101
void sh4_inst_binary_fsca_fpul_dr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn011111101, INST_CONS_1111nnn011111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    // TODO: should I really be calling sh4_fpu_clear_cause here ?
    sh4_fpu_clear_cause(sh4);

#ifdef SH4_FPU_PEDANTIC
    sh4->reg[SH4_REG_FPSCR] |= (SH4_FPSCR_CAUSE_I_MASK | SH4_FPSCR_FLAG_I_MASK);
#endif

    unsigned sin_reg_no = ((inst >> 9) & 0x7) * 2;
    unsigned cos_reg_no = sin_reg_no + 1;
    unsigned angle = sh4->reg[SH4_REG_FPUL] & (FSCA_TBL_LEN - 1);

    memcpy(sh4_fpu_fr(sh4, sin_reg_no), sh4_fsca_sin_tbl + angle,
           sizeof(float));
    memcpy(sh4_fpu_fr(sh4, cos_reg_no), sh4_fsca_cos_tbl + angle,
           sizeof(float));
}

#define INST_MASK_0100mmmm01101010 0xf0ff
#define INST_CONS_0100mmmm01101010 0x406a

// LDS Rm, FPSCR
// 0100mmmm01101010
void sh4_inst_binary_lds_gen_fpscr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01101010, INST_CONS_0100mmmm01101010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4_set_fpscr(sh4, *sh4_gen_reg(sh4, (inst >> 8) & 0xf));
}

#define INST_MASK_0100mmmm01011010 0xf0ff
#define INST_CONS_0100mmmm01011010 0x405a

// LDS Rm, FPUL
// 0100mmmm01011010
void sh4_inst_binary_gen_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01011010, INST_CONS_0100mmmm01011010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    memcpy(sh4->reg + SH4_REG_FPUL, sh4_gen_reg(sh4, (inst >> 8) & 0xf),
           sizeof(sh4->reg[SH4_REG_FPUL]));
}

#define INST_MASK_0100mmmm01100110 0xf0ff
#define INST_CONS_0100mmmm01100110 0x4066

// LDS.L @Rm+, FPSCR
// 0100mmmm01100110
void sh4_inst_binary_ldsl_indgeninc_fpscr(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01100110, INST_CONS_0100mmmm01100110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    val = memory_map_read_32(sh4->mem.map, *addr_reg);

    sh4_set_fpscr(sh4, val);

    *addr_reg += 4;
}

#define INST_MASK_0100mmmm01010110 0xf0ff
#define INST_CONS_0100mmmm01010110 0x4056

// LDS.L @Rm+, FPUL
// 0100mmmm01010110
void sh4_inst_binary_ldsl_indgeninc_fpul(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100mmmm01010110, INST_CONS_0100mmmm01010110);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    uint32_t val;
    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);

    val = memory_map_read_32(sh4->mem.map, *addr_reg);

    memcpy(sh4->reg + SH4_REG_FPUL, &val, sizeof(sh4->reg[SH4_REG_FPUL]));

    *addr_reg += 4;
}

#define INST_MASK_0000nnnn01101010 0xf0ff
#define INST_CONS_0000nnnn01101010 0x006a

// STS FPSCR, Rn
// 0000nnnn01101010
void sh4_inst_binary_sts_fpscr_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn01101010, INST_CONS_0000nnnn01101010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    *sh4_gen_reg(sh4, (inst >> 8) & 0xf) = sh4->reg[SH4_REG_FPSCR];
}

#define INST_MASK_0000nnnn01011010 0xf0ff
#define INST_CONS_0000nnnn01011010 0x005a

// STS FPUL, Rn
// 0000nnnn01011010
void sh4_inst_binary_sts_fpul_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0000nnnn01011010, INST_CONS_0000nnnn01011010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    memcpy(sh4_gen_reg(sh4, (inst >> 8) & 0xf), sh4->reg + SH4_REG_FPUL,
           sizeof(sh4->reg[SH4_REG_FPUL]));
}

#define INST_MASK_0100nnnn01100010 0xf0ff
#define INST_CONS_0100nnnn01100010 0x4062

// STS.L FPSCR, @-Rn
// 0100nnnn01100010
void sh4_inst_binary_stsl_fpscr_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn01100010, INST_CONS_0100nnnn01100010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_FPSCR]);

    *addr_reg = addr;
}

#define INST_MASK_0100nnnn01010010 0xf0ff
#define INST_CONS_0100nnnn01010010 0x4052

// STS.L FPUL, @-Rn
// 0100nnnn01010010
void sh4_inst_binary_stsl_fpul_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_0100nnnn01010010, INST_CONS_0100nnnn01010010);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    reg32_t *addr_reg = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    addr32_t addr = *addr_reg - 4;

    memory_map_write_32(sh4->mem.map, addr, sh4->reg[SH4_REG_FPUL]);

    *addr_reg = addr;
}

#define INST_MASK_1111nnn1mmm01100 0xf11f
#define INST_CONS_1111nnn1mmm01100 0xf10c

// FMOV DRm, XDn
// 1111nnn1mmm01100
void sh4_inst_binary_fmov_dr_xd(void *cpu, cpu_inst_param inst) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_INST(inst, INST_MASK_1111nnn1mmm01100, INST_CONS_1111nnn1mmm01100);
    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    *sh4_fpu_xd(sh4, dr_dst) = *sh4_fpu_dr(sh4, dr_src);
}

#define INST_MASK_1111nnn0mmm11100 0xf11f
#define INST_CONS_1111nnn0mmm11100 0xf01c

// FMOV XDm, DRn
// 1111nnn0mmm11100
void sh4_inst_binary_fmov_xd_dr(void *cpu, cpu_inst_param inst) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_INST(inst, INST_MASK_1111nnn0mmm11100, INST_CONS_1111nnn0mmm11100);
    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    int dr_src = (inst >> 5) & 0x7;
    int dr_dst = (inst >> 9) & 0x7;

    *sh4_fpu_dr(sh4, dr_dst) = *sh4_fpu_xd(sh4, dr_src);
}

#define INST_MASK_1111nnn1mmm11100 0xf11f
#define INST_CONS_1111nnn1mmm11100 0xf11c

// FMOV XDm, XDn
// 1111nnn1mmm11100
void sh4_inst_binary_fmov_xd_xd(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn1mmm11100, INST_CONS_1111nnn1mmm11100);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmm11100");
    error_set_opcode_name("FMOV XDm, XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnn1mmmm1000 0xf10f
#define INST_CONS_1111nnn1mmmm1000 0xf108

// FMOV @Rm, XDn
// 1111nnn1mmmm1000
void sh4_inst_binary_fmov_indgen_xd(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn1mmmm1000, INST_CONS_1111nnn1mmmm1000);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t addr = *sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    double *dst_ptr = sh4_fpu_xd(sh4, (inst >> 9) & 0x7);

    *dst_ptr = memory_map_read_double(sh4->mem.map, addr);
}

#define INST_MASK_1111nnn1mmmm1001 0xf10f
#define INST_CONS_1111nnn1mmmm1001 0xf109

// FMOV @Rm+, XDn
// 1111nnn1mmmm1001
void sh4_inst_binary_fmov_indgeninc_xd(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn1mmmm1001, INST_CONS_1111nnn1mmmm1001);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 4) & 0xf);
    double *dst_ptr = sh4_fpu_xd(sh4, (inst >> 9) & 0x7);

    *dst_ptr = memory_map_read_double(sh4->mem.map, *addr_p);

    *addr_p += 8;
}

#define INST_MASK_1111nnn1mmmm0110 0xf10f
#define INST_CONS_1111nnn1mmmm0110 0xf106

// FMOV @(R0, Rn), XDn
// 1111nnn1mmmm0110
void sh4_inst_binary_fmov_binind_r0_gen_xd(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnn1mmmm0110, INST_CONS_1111nnn1mmmm0110);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnn1mmmm0110");
    error_set_opcode_name("FMOV @(R0, Rn), XDn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnnnmmm11010 0xf01f
#define INST_CONS_1111nnnnmmm11010 0xf01a

// FMOV XDm, @Rn
// 1111nnnnmmm11010
void sh4_inst_binary_fmov_xd_indgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm11010, INST_CONS_1111nnnnmmm11010);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmm11010");
    error_set_opcode_name("FMOV XDm, @Rn");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnnnmmm11011 0xf01f
#define INST_CONS_1111nnnnmmm11011 0xf01b

// FMOV XDm, @-Rn
// 1111nnnnmmm11011
void sh4_inst_binary_fmov_xd_inddecgen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm11011, INST_CONS_1111nnnnmmm11011);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    reg32_t *addr_p = sh4_gen_reg(sh4, (inst >> 8) & 0xf);
    reg32_t addr = *addr_p - 8;
    double *src_p = sh4_fpu_xd(sh4, (inst >> 5) & 0x7);

    memory_map_write_double(sh4->mem.map, addr, *src_p);

    *addr_p = addr;
}

#define INST_MASK_1111nnnnmmm10111 0xf01f
#define INST_CONS_1111nnnnmmm10111 0xf017

// FMOV XDm, @(R0, Rn)
// 1111nnnnmmm10111
void sh4_inst_binary_fmov_xs_binind_r0_gen(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnnmmm10111, INST_CONS_1111nnnnmmm10111);

#ifdef INVARIANTS
    struct Sh4 *sh4 = (struct Sh4*)cpu;
#endif

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_SZ_MASK, SH4_FPSCR_SZ_MASK);

    error_set_feature("opcode implementation");
    error_set_opcode_format("1111nnnnmmm10111");
    error_set_opcode_name("FMOV XDm, @(R0, Rn)");
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
}

#define INST_MASK_1111nnmm11101101 0xf0ff
#define INST_CONS_1111nnmm11101101 0xf0ed

// FIPR FVm, FVn - vector dot product
// 1111nnmm11101101
void sh4_inst_binary_fipr_fv_fv(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnmm11101101, INST_CONS_1111nnmm11101101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4_fpu_clear_cause(sh4);

#ifdef SH4_FPU_PEDANTIC
    if (sh4->reg[SH4_REG_FPSCR] & (SH4_FPSCR_ENABLE_V_MASK |
                                   SH4_FPSCR_ENABLE_O_MASK |
                                   SH4_FPSCR_ENABLE_U_MASK |
                                   SH4_FPSCR_ENABLE_I_MASK)) {
        sh4_set_exception(sh4, SH4_EXCP_FPU);
        return;
    }
    /*
     * TODO:
     * There's quite alot of error-checking/exception-raising/flag-setting to
     * be done here.  For now I'm committing without it becuase it looks like a
     * real headache to write, and I'm honestly of the opinion that going this
     * deep with the pedantry is a waste of time anyways.
     */
#endif
    unsigned reg_src_idx = ((inst >> 8) & 0x3) * 4;
    unsigned reg_dst_idx = ((inst >> 10) & 0x3) * 4;

    reg32_t *src1_ptr = sh4->reg + SH4_REG_FR0 + reg_src_idx;
    reg32_t *src2_ptr = sh4->reg + SH4_REG_FR0 + reg_dst_idx;

    float src1[4], src2[4], dst;
    memcpy(src1, src1_ptr, sizeof(src1));
    memcpy(src2, src2_ptr, sizeof(src2));

    dst = src1[0] * src2[0] + src1[1] * src2[1] + src1[2] * src2[2] + src1[3] * src2[3];
    memcpy(sh4->reg + SH4_REG_FR0 + reg_dst_idx + 3, &dst, sizeof(dst));
}

#define INST_MASK_1111nn0111111101 0xf3ff
#define INST_CONS_1111nn0111111101 0xf1fd

// FTRV XMTRX, FVn - multiple vector by matrix
// 1111nn0111111101
void sh4_inst_binary_fitrv_mxtrx_fv(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nn0111111101, INST_CONS_1111nn0111111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    sh4_fpu_clear_cause(sh4);

#ifdef SH4_FPU_PEDANTIC
    if (sh4->reg[SH4_REG_FPSCR] & (SH4_FPSCR_ENABLE_V_MASK |
                                   SH4_FPSCR_ENABLE_O_MASK |
                                   SH4_FPSCR_ENABLE_U_MASK |
                                   SH4_FPSCR_ENABLE_I_MASK)) {
        sh4_set_exception(sh4, SH4_EXCP_FPU);
        return;
    }
    /*
     * TODO:
     * There's quite alot of error-checking/exception-raising/flag-setting to
     * be done here.  For now I'm committing without it becuase it looks like a
     * real headache to write, and I'm honestly of the opinion that going this
     * deep with the pedantry is a waste of time anyways.
     */
#endif

    unsigned reg_idx = ((inst >> 10) & 0x3) * 4 + SH4_REG_FR0;
    float tmp[4];
    memcpy(tmp, sh4->reg + reg_idx, sizeof(tmp));

    float tmp_out[4];

    float row0[4], row1[4], row2[4], row3[4];

    memcpy(row0, sh4->reg+SH4_REG_XF0, sizeof(float));
    memcpy(row0 + 1, sh4->reg+SH4_REG_XF4, sizeof(float));
    memcpy(row0 + 2, sh4->reg+SH4_REG_XF8, sizeof(float));
    memcpy(row0 + 3, sh4->reg+SH4_REG_XF12, sizeof(float));

    memcpy(row1, sh4->reg+SH4_REG_XF1, sizeof(float));
    memcpy(row1+1, sh4->reg+SH4_REG_XF5, sizeof(float));
    memcpy(row1+2, sh4->reg+SH4_REG_XF9, sizeof(float));
    memcpy(row1+3, sh4->reg+SH4_REG_XF13, sizeof(float));

    memcpy(row2, sh4->reg+SH4_REG_XF2, sizeof(float));
    memcpy(row2+1, sh4->reg+SH4_REG_XF6, sizeof(float));
    memcpy(row2+2, sh4->reg+SH4_REG_XF10, sizeof(float));
    memcpy(row2+3, sh4->reg+SH4_REG_XF14, sizeof(float));

    memcpy(row3, sh4->reg+SH4_REG_XF3, sizeof(float));
    memcpy(row3+1, sh4->reg+SH4_REG_XF7, sizeof(float));
    memcpy(row3+2, sh4->reg+SH4_REG_XF11, sizeof(float));
    memcpy(row3+3, sh4->reg+SH4_REG_XF15, sizeof(float));

    tmp_out[0] = tmp[0] * row0[0] +
        tmp[1] * row0[1] +
        tmp[2] * row0[2] +
        tmp[3] * row0[3];
    tmp_out[1] = tmp[0] * row1[0] +
        tmp[1] * row1[1] +
        tmp[2] * row1[2] +
        tmp[3] * row1[3];
    tmp_out[2] = tmp[0] * row2[0] +
        tmp[1] * row2[1] +
        tmp[2] * row2[2] +
        tmp[3] * row2[3];
    tmp_out[3] = tmp[0] * row3[0] +
        tmp[1] * row3[1] +
        tmp[2] * row3[2] +
        tmp[3] * row3[3];

    memcpy(sh4->reg + reg_idx, tmp_out, sizeof(tmp_out));
}

#define INST_MASK_1111nnnn01111101 0xf0ff
#define INST_CONS_1111nnnn01111101 0xf07d

// FSRRA FRn
// 1111nnnn01111101
void sh4_inst_unary_fsrra_frn(void *cpu, cpu_inst_param inst) {

    CHECK_INST(inst, INST_MASK_1111nnnn01111101, INST_CONS_1111nnnn01111101);

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    CHECK_FPSCR(sh4->reg[SH4_REG_FPSCR], SH4_FPSCR_PR_MASK, 0);

    sh4_fpu_clear_cause(sh4);

    int fr_reg = (inst >> 8) & 0xf;
    float *srcp = sh4_fpu_fr(sh4, fr_reg);
    float src = *srcp;

#ifdef SH4_FPU_PEDANTIC
    if ((src < 0.0f) || issignaling(src)) {
        sh4_fr_invalid(sh4, fr_reg);
        return;
    }

    int class = fpclassify(src);

    if (class == FP_SUBNORMAL) {
        // TODO: do I raise an exception here?
        sh4->reg[SH4_REG_FPSCR] |= (SH4_FPSCR_CAUSE_E_MASK |
                                    SH4_FPSCR_FLAG_E_MASK);
        return;
    }

    sh4->reg[SH4_REG_FPSCR] |= (SH4_FPSCR_ENABLE_I_MASK | SH4_FPSCR_CAUSE_I_MASK);
    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_ENABLE_I_MASK)
        sh4_set_exception(sh4, SH4_EXCP_FPU);
#endif

    *srcp = 1.0 / sqrt(src);
}

void sh4_inst_invalid(void *cpu, cpu_inst_param inst) {
    struct Sh4 *sh4 = (struct Sh4*)cpu;

    LOG_ERROR("ERROR - unrecognized opcode at PC=0x%08x\n",
              sh4->reg[SH4_REG_PC]);

#ifdef DBG_EXIT_ON_UNDEFINED_OPCODE

    error_set_feature("SH4 CPU exception for unrecognized opcode");
    error_set_inst_bin(inst);
    SH4_INST_RAISE_ERROR(sh4, ERROR_UNIMPLEMENTED);
#else
    /*
     * raise an sh4 CPU exception, this case is
     * what's actually supposed to happen on real hardware.
     */

    /*
     * TODO - SH4_EXCP_SLOT_ILLEGAL_INST should supersede
     * SH4_EXCP_GEN_ILLEGAL_INST if the sh4 is in a branch slot.
     * Currently there's no way to know if this function is being
     * called from the context of a delay slot.
     */
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

// FMOV FRm, FRn
// 1111nnnnmmmm1100
// FMOV DRm, DRn
// 1111nnn0mmm01100
// FMOV XDm, DRn
// 1111nnn0mmm11100
// FMOV DRm, XDn
// 1111nnn1mmm01100
// FMOV XDm, XDn
// 1111nnn1mmm11100
DEF_FPU_HANDLER_CUSTOM(fmov_gen) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge all four of these into a single
         * opcode handler and use the (1 << 8) and (1 << 4) bits to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & ((1 << 8) | (1 << 4))) {
        case 0:
            sh4_inst_binary_fmov_dr_dr(sh4, inst);
            break;
        case (1 << 4):
            sh4_inst_binary_fmov_xd_dr(sh4, inst);
            break;
        case (1 << 8):
            sh4_inst_binary_fmov_dr_xd(sh4, inst);
            break;
        case (1 << 8) | (1 << 4):
            sh4_inst_binary_fmov_xd_xd(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmov_fr_fr(sh4, inst);
    }
}

// FMOV.S @Rm, FRn
// 1111nnnnmmmm1000
// FMOV @Rm, DRn
// 1111nnn0mmmm1000
// FMOV @Rm, XDn
// 1111nnn1mmmm1000
DEF_FPU_HANDLER_CUSTOM(fmovs_ind_gen) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 8) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 8)) {
        case 0:
            sh4_inst_binary_fmov_indgen_dr(sh4, inst);
            break;
        case (1 << 8):
            sh4_inst_binary_fmov_indgen_xd(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_indgen_fr(sh4, inst);
    }
}

// FMOV.S @(R0, Rm), FRn
// 1111nnnnmmmm0110
// FMOV @(R0, Rm), DRn
// 1111nnn0mmmm0110
// FMOV @(R0, Rm), XDn
// 1111nnn1mmmm0110
DEF_FPU_HANDLER_CUSTOM(fmov_binind_r0_gen_fpu) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 8) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 8)) {
        case 0:
            sh4_inst_binary_fmov_binind_r0_gen_dr(sh4, inst);
            break;
        case (1 << 8):
            sh4_inst_binary_fmov_binind_r0_gen_xd(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_binind_r0_gen_fr(sh4, inst);
    }
}

// FMOV.S @Rm+, FRn
// 1111nnnnmmmm1001
// FMOV @Rm+, DRn
// 1111nnn0mmmm1001
// FMOV @Rm+, XDn
// 1111nnn1mmmm1001
DEF_FPU_HANDLER_CUSTOM(fmov_indgeninc_fpu) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 8) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 8)) {
        case 0:
            sh4_inst_binary_fmov_indgeninc_dr(sh4, inst);
            break;
        case (1 << 8):
            sh4_inst_binary_fmov_indgeninc_xd(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_indgeninc_fr(sh4, inst);
    }
}

// FMOV.S FRm, @Rn
// 1111nnnnmmmm1010
// FMOV DRm, @Rn
// 1111nnnnmmm01010
// FMOV XDm, @Rn
// 1111nnnnmmm11010
DEF_FPU_HANDLER_CUSTOM(fmov_fpu_indgen) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 4) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 4)) {
        case 0:
            sh4_inst_binary_fmov_dr_indgen(sh4, inst);
            break;
        case (1 << 4):
            sh4_inst_binary_fmov_xd_indgen(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_fr_indgen(sh4, inst);
    }
}

// FMOV.S FRm, @-Rn
// 1111nnnnmmmm1011
// FMOV DRm, @-Rn
// 1111nnnnmmm01011
// FMOV XDm, @-Rn
// 1111nnnnmmm11011
DEF_FPU_HANDLER_CUSTOM(fmov_fpu_inddecgen) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 4) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 4)) {
        case 0:
            sh4_inst_binary_fmov_dr_inddecgen(sh4, inst);
            break;
        case (1 << 4):
            sh4_inst_binary_fmov_xd_inddecgen(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_fr_inddecgen(sh4, inst);
    }
}


// FMOV.S FRm, @(R0, Rn)
// 1111nnnnmmmm0111
// FMOV DRm, @(R0, Rn)
// 1111nnnnmmm00111
// FMOV XDm, @(R0, Rn)
// 1111nnnnmmm10111
DEF_FPU_HANDLER_CUSTOM(fmov_fpu_binind_r0_gen) {

    struct Sh4 *sh4 = (struct Sh4*)cpu;

    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_SZ_MASK) {

        /*
         * TODO: I ought to be able to merge both of these into a single
         * opcode handler and use the (1 << 4) bit to control
         * which register banks get used for the source and destination
         * operands.
         */
        switch (inst & (1 << 4)) {
        case 0:
            sh4_inst_binary_fmov_dr_binind_r0_gen(sh4, inst);
            break;
        case (1 << 4):
            sh4_inst_binary_fmov_xs_binind_r0_gen(sh4, inst);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY); // should never happen
        }
    } else {
        sh4_inst_binary_fmovs_fr_binind_r0_gen(sh4, inst);
    }
}

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

// FSCA FPUL, DRn
// 1111nnn011111101
DEF_FPU_HANDLER(fsca_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_binary_fsca_fpul_dr,
                sh4_inst_invalid);

// FSRRA FRn
// 1111nnnn01111101
DEF_FPU_HANDLER(fsrra_fpu, SH4_FPSCR_PR_MASK,
                sh4_inst_unary_fsrra_frn, sh4_inst_invalid);
