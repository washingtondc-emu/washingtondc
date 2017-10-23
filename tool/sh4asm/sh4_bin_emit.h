/*******************************************************************************
 *
 * Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef SH4_BIN_EMIT_H_
#define SH4_BIN_EMIT_H_

#include <stdint.h>

#include "sh4_opcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*emit_bin_handler_func)(uint16_t);

uint16_t assemble_bin_noarg(uint16_t opcode);
uint16_t assemble_bin_rn(uint16_t opcode, unsigned rn);
uint16_t assemble_bin_imm8(uint16_t opcode, unsigned imm8);
uint16_t assemble_bin_imm12(uint16_t opcode, unsigned imm12);
uint16_t assemble_bin_rn_imm8(uint16_t opcode, unsigned rn, unsigned imm8);
uint16_t assemble_bin_rm_rn(uint16_t opcode, unsigned rm, unsigned rn);
uint16_t assemble_bin_rm_rnbank(uint16_t opcode, unsigned rm, unsigned rn_bank);
uint16_t assemble_bin_rn_imm4(uint16_t opcode, unsigned rn, unsigned imm4);
uint16_t assemble_bin_rm_rn_imm4(uint16_t opcode, unsigned rm,
                                 unsigned imm4, unsigned rn);
uint16_t assemble_bin_drm_drn(uint16_t opcode, unsigned drm, unsigned drn);
uint16_t assemble_bin_rm_drn(uint16_t opcode, unsigned rm, unsigned drn);
uint16_t assemble_bin_drm_rn(uint16_t opcode, unsigned drm, unsigned rn);
uint16_t assemble_bin_drn(uint16_t opcode, unsigned drn);
uint16_t assemble_bin_fvm_fvn(uint16_t opcode, unsigned fvm, unsigned fvn);
uint16_t assemble_bin_fvn(uint16_t opcode, unsigned fvn);

void emit_bin_inst(emit_bin_handler_func emit, uint16_t inst);

#define EMIT_BIN_NOARG(em, op)                  \
    emit_bin_inst((em), assemble_bin_noarg(op))

#define EMIT_BIN_RN(em, op, rn)                         \
    emit_bin_inst((em), assemble_bin_rn((op), (rn)))

#define EMIT_BIN_IMM8(em, op, imm8)                             \
    emit_bin_inst((em), assemble_bin_imm8((op), (imm8)))

#define EMIT_BIN_IMM12(em, op, imm12)                           \
    emit_bin_inst((em), assemble_bin_imm12((op), (imm12)))

#define EMIT_BIN_RN_IMM8(em, op, rn, imm8)                              \
    emit_bin_inst((em), assemble_bin_rn_imm8((op), (rn), (imm8)))

#define EMIT_BIN_RM_RN(em, op, rm, rn)                          \
    emit_bin_inst((em), assemble_bin_rm_rn((op), (rm), (rn)))

#define EMIT_BIN_RM_RNBANK(em, op, rm, rn_bank)                         \
    emit_bin_inst((em), assemble_bin_rm_rnbank((op), (rm), (rn_bank)))

#define EMIT_BIN_RN_IMM4(em, op, rn, imm4)                              \
    emit_bin_inst((em), assemble_bin_rn_imm4((op), (rn), (imm4)))

#define EMIT_BIN_RM_RN_IMM4(em, op, rm, rn, imm4)                       \
    emit_bin_inst((em), assemble_bin_rm_rn_imm4((op), (rm), (rn), (imm4)))

#define EMIT_BIN_DRM_DRN(em, op, drm, drn)                              \
    emit_bin_inst((em), assemble_bin_drm_drn((op), (drm), (drn)))

#define EMIT_BIN_RM_DRN(em, op, rm, drn)                        \
    emit_bin_inst((em), aseemble_bin_rm_drn((op), (rm), (drn)))

#define EMIT_BIN_DRM_RN(em, op, drm, rn)                        \
    emit_bin_inst((em), assemble_bin_drm_rn((op), (drm), (rn)))

#define EMIT_BIN_DRN(em, op, drn)                       \
    emit_bin_inst((em), assemble_bin_drn((op), (drn))

#define EMIT_BIN_FVM_FVN(em, fvm, fvn)                                  \
    emit_bin_inst((em), assemble_bin_fvm_fvn((op), (fvm), (fvn)))

#define EMIT_BIN_FVN(em, fvn)                           \
    emit_bin_inst((em), assemble_bin_fvn((op), (fvn)))

// opcodes which take no arguments (noarg)
#define MASK_OPCODE_NOARG 0xffff

// opcodes which take no arguments (noarg)
#define BIN_DIV0U(em) EMIT_BIN_NOARG(em, OPCODE_DIV0U)
#define BIN_RTS(em) EMIT_BIN_NORARG(em, OPCODE_RTS)
#define BIN_CLRMAC(em) EMIT_BIN_NOARG(em, OPCODE_CLRMAC)
#define BIN_CLRS(em) EMIT_BIN_NOARG(em, OPCODE_CLRS)
#define BIN_CLRT(em) EMIT_BIN_NOARG(em, OPCODE_CLRT)
#define BIN_LDTLB(em) EMIT_BIN_NOARG(em, OPCODE_LDTLB)
#define BIN_NOP(em) EMIT_BIN_NOARG(em, OPCODE_NOP)
#define BIN_RTE(em) EMIT_BIN_NOARG(em, OPCODE_RTE)
#define BIN_SETS(em) EMIT_BIN_NOARG(em, OPCODE_SETS)
#define BIN_SETT(em) EMIT_BIN_NOARG(em, OPCODE_SETT)
#define BIN_SLEEP(em) EMIT_BIN_NOARG(em, OPCODE_SLEEP)
#define BIN_FRCHG(em) EMIT_BIN_NOARG(em, OPCODE_FRCHG)
#define BIN_FSCHG(em) EMIT_BIN_NOARG(em, OPCODE_FSCHG)

// opcodes which take a general-purpose register as the sole argument
#define MASK_OPCODE_RN 0xf0ff

#define BIN_MOVT_RN(em, rn) EMIT_BIN_RN(em, OPCODE_MOVT_RN, (rn))
#define BIN_CMPPZ_RN(em, rn) EMIT_BIN_RN(em, OPCODE_CMPPZ_RN, (rn))
#define BIN_CMPPL_RN(em, rn) EMIT_BIN_RN(em, OPCODE_CMPPL_RN, (rn))
#define BIN_DT_RN(em, rn) EMIT_BIN_RN(em, OPCODE_DT_RN, (rn))
#define BIN_ROTL_RN(em, rn) EMIT_BIN_RN(em, OPCODE_RTL_RN, (rn))
#define BIN_ROTR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_ROTR_RN, (rn))
#define BIN_ROTCL_RN(em, rn) EMIT_BIN_RN(em, OPCODE_ROTCL_RN, (rn))
#define BIN_ROTCR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_ROTCR_RN, (rn))
#define BIN_SHAL_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHAL_RN, (rn))
#define BIN_SHLL_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLL_RN, (rn))
#define BIN_SHLR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLR_RN, (rn))
#define BIN_SHLL2_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLL2_RN, (rn))
#define BIN_SHLR2_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLR2_RN, (rn))
#define BIN_SHLL8_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLL8_RN, (rn))
#define BIN_SHLR8_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLR8_RN, (rn))
#define BIN_SHLL16_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLL16_RN, (rn))
#define BIN_SHLR16_RN(em, rn) EMIT_BIN_RN(em, OPCODE_SHLR15_RN, (rn))
#define BIN_BRAF_RN(em, rn) EMIT_BIN_RN(em, OPCODE_BRAF_RN, (rn))
#define BIN_BSRF_RN(em, rn) EMIT_BIN_RN(em, OPCODE_BSRF_RN, (rn))

#define BIN_TASB_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_TASB_ARN, (rn))
#define BIN_OCBI_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_OCBI_ARN, (rn))
#define BIN_OCBP_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_OCBP_ARN, (rn))
#define BIN_PREF_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_PREF_ARN, (rn))
#define BIN_JMP_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_JMP_ARN, (rn))
#define BIN_JSR_ARN(em, rn) EMIT_BIN_RN(em, OPCODE_JSR_ARN, (rn))

#define BIN_LDC_RM_SR(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_SR, (rn))
#define BIN_LDC_RM_GBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_GBR, (rn))
#define BIN_LDC_RM_VBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_VBR, (rn))
#define BIN_LDC_RM_SSR(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_SSR, (rn))
#define BIN_LDC_RM_SPC(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_SPC, (rn))
#define BIN_LDC_RM_DBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDC_RM_DBR, (rn))

#define BIN_STC_SR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_SR_RN, (rn))
#define BIN_STC_GBR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_GBR_RN, (rn))
#define BIN_STC_VBR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_VBR_RN, (rn))
#define BIN_STC_SSR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_SSR_RN, (rn))
#define BIN_STC_SPC_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_SPC_RN, (rn))
#define BIN_STC_SGR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_SGR_RN, (rn))
#define BIN_STC_DBR_RN(em, rn) EMIT_BIN_RN(em, OPCODE_STC_DBR_RN, (rn))

#define BIN_LDCL_ARMP_SR(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_SR, (rn))
#define BIN_LDCL_ARMP_GBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_GBR, (rn))
#define BIN_LDCL_ARMP_VBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_VBR, (rn))
#define BIN_LDCL_ARMP_SSR(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_SSR, (rn))
#define BIN_LDCL_ARMP_SPC(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_SPC, (rn))
#define BIN_LDCL_ARMP_DBR(em, rn) EMIT_BIN_RN(em, OPCODE_LDCL_ARMP_DBR, (rn))

#define BIN_STCL_SR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_SR_AMRN, (rn))
#define BIN_STCL_GBR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_GBR_AMRN, (rn))
#define BIN_STCL_VBR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_VBR_AMRN, (rn))
#define BIN_STCL_SSR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_SSR_AMRN, (rn))
#define BIN_STCL_SPC_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_SPC_AMRN, (rn))
#define BIN_STCL_SGR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_SGR_AMRN, (rn))
#define BIN_STCL_DBR_AMRN(em, rn) EMIT_BIN_RN(em, OPCODE_STCL_DBR_AMRN, (rn))

#define BIN_LDS_RM_MACH(em, rm) EMIT_BIN_RN((em), OPCODE_LDS_RM_MACH, (rm))
#define BIN_LDS_RM_MACL(em, rm) EMIT_BIN_RN((em), OPCODE_LDS_RM_MACL, (rm))
#define BIN_STS_MACH_RN(em, rn) EMIT_BIN_RN((em), OPCODE_STS_MACH_RN, (rn))
#define BIN_STS_MACL_RN(em, rn) EMIT_BIN_RN((em), OPCODE_STS_MACL_RN, (rn))
#define BIN_LDS_RM_PR(em, rm) EMIT_BIN_RN((em), OPCODE_LDS_RM_PR, (rm))
#define BIN_STS_PR_RN(em, rn) EMIT_BIN_RN((em), OPCODE_STS_PR_RN, (rn))
#define BIN_LDSL_ARMP_MACH(em, rm)                      \
    EMIT_BIN_RN((em), OPCODE_LDSL_ARMP_MACH, (rm))
#define BIN_LDSL_ARMP_MACL(em, rm)                      \
    EMIT_BIN_RN((em), OPCODE_LDSL_ARMP_MACL, (rm))
#define BIN_STSL_MACH_AMRN(em, rn)                      \
    EMIT_BIN_RN((em), OPCODE_STSL_MACH_AMRN, (rn))
#define BIN_STSL_MACL_AMRN(em, rn)                      \
    EMIT_BIN_RN((em), OPCODE_STSL_MACL_AMRN, (rn))
#define BIN_LDSL_ARMP_PR(em, rm)                        \
    EMIT_BIN_RN((em), OPCODE_LDSL_ARMP_PR, (rm))
#define BIN_STSL_PR_AMRN(em, rn)                        \
    EMIT_BIN_RN((em), OPCODE_STSL_PR_AMRN, (rn))
#define BIN_LDS_RM_FPSCR(em, rm)                        \
    EMIT_BIN_RN((em), OPCODE_LDS_RM_FPSCR, (rm))
#define BIN_LDS_RM_FPUL(em, rm)                 \
    EMIT_BIN_RN((em), OPCODE_LDS_RM_FPUL, (rm))
#define BIN_LDSL_ARMP_FPSCR(em, rm)                     \
    EMIT_BIN_RN((em), OPCODE_LDSL_ARMP_PFSCR, (rm))
#define BIN_LDSL_ARMP_FPUL(em, rm)                      \
    EMIT_BIN_RN((em), OPCODE_LDSL_ARMP_FPUL, (rm))
#define BIN_STS_FPSCR_RN(em, rn)                        \
    EMIT_BIN_RN((em), OPCODE_STS_FPSCR_RN, (rn))
#define BIN_STS_FPUL_RN(em, rn)                 \
    EMIT_BIN_RN((em), OPCODE_STS_FPUL_RN, (rn))
#define BIN_STSL_FPSCR_AMRN(em, rn)                     \
    EMIT_BIN_RN((em), OPCODE_STSL_FPSCR_AMRN, (rn))
#define BIN_STSL_FPUL_AMRN(em, rn)                      \
    EMIT_BIN_RN((em), OPCODE_STSL_FPUL_AMRN, (rn))


#define BIN_MOVCAL_R0_ARN(em, rn)                       \
    EMIT_BIN_RN((em), OPCODE_MOVCAL_R0_ARN, (rn))

#define BIN_FLDI0_FRN(em, frn) EMIT_BIN_RN((em), OPCODE_FLDIO_FRN, (frn))
#define BIN_FLDI1_FRN(em, frn) EMIT_BIN_RN((em), OPCODE_FLDI1_FRN, (frn))

#define BIN_FLDS_FRM_FPUL(em, frm)                      \
    EMIT_BIN_RN((em), OPCODE_FLDS_FRM_FPUL, (frm))
#define BIN_FSTS_FPUL_FRN(em, frn)                      \
    EMIT_BIN_RN((em), OPCODE_FSTS_FPUL_FRN, (frn))
#define BIN_FABS_FRN(em, frn)                   \
    EMIT_BIN_RN((em), OPCODE_FABS_FRN, (frn))
#define BIN_FLOAT_FPUL_FRN(em, frn)                     \
    EMIT_BIN_RN((em), OPCODE_FLOAT_FPUL_FRN, (frn))
#define BIN_FNEG_FRN(em, frn)                   \
    EMIT_BIN_RN((em), OPCODE_FNEG_FRN, (frn))
#define BIN_FSQRT_FRN(em, frn)                  \
    EMIT_BIN_RN((em), OPCODE_FSQRT_FRN, (frn))
#define BIN_FTRC_FRM_FPUL(em, frm)              \
    EMIT_BIN_RN((em), OPCODE_FTRC_FRM_FPUL, (frm))
#define BIN_FSRRA_FRN(em, frn)                  \
    EMIT_BIN_RN((em), OPCODE_FSRRA_FRN, (frn))

/*
 * opcodes which take an 8-bit immediate value as input
 *
 * Some of these also take in specific registers as implied opcodes; this
 * factors into the text-based assembly (and also these macros), but it does
 * not have any impact on the binary form of the instructions.
 */
#define MASK_OPCODE_IMM8 0xff00

#define BIN_CMPEQ_IMM8(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_CMPEQ_IMM8_R0, (imm8))
#define BIN_ANDB_IMM8_A_R0_GBR(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_ANDB_IMM8_A_R0_GBR, (imm8))
#define BIN_AND_IMM8_R0(em, imm8)                  \
    EMIT_BIN_IMM8(em, OPCODE_AND_IMM8_R0, (imm8))
#define BIN_ORB_IMM8_A_R0_GBR(em, imm8)                    \
    EMIT_BIN_IMM8(em, OPCODE_ORB_IMM8_A_R0_GBR, (imm8))
#define BIN_OR_IMM8_R0(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_OR_IMM8_R0, (imm8))
#define BIN_TST_IMM8_R0(em, imm8)                  \
    EMIT_BIN_IMM8(em, OPCODE_TST_IMM8_R0, (imm8))
#define BIN_TSTB_IMM8_A_R0_GBR(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_TSTB_IMM8_A_R0_GBR, (imm8))
#define BIN_XOR_IMM8_R0(em, imm8)                  \
    EMIT_BIN_IMM8(em, OPCODE_XOR_IMM8_R0, (imm8))
#define BIN_XORB_IMM8_A_R0_GBR(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_XORB_IMM8_A_R0_GBR, (imm8))
#define BIN_BF_IMM8(em, imm8)                      \
    EMIT_BIN_IMM8(em, OPCODE_BF_IMM8, (imm8))
#define BIN_BFS_IMM8(em, imm8)                     \
    EMIT_BIN_IMM8(em, OPCODE_BFS_IMM8, (imm8))
#define BIN_BT_IMM8(em, imm8)                      \
    EMIT_BIN_IMM8(em, OPCODE_BT_IMM8, (imm8))
#define BIN_BTS_IMM8(em, imm8)                     \
    EMIT_BIN_IMM8(em, OPCODE_BTS_IMM8, (imm8))
#define BIN_TRAPA_IMM8(em, imm8)                   \
    EMIT_BIN_IMM8(em, OPCODE_TRAPA_IMM8, (imm8))

#define BIN_MOVB_R0_A_DISP_GBR(em, disp)        \
    EMIT_BIN_IMM8((em), OPCODE_MOVB_R0_A_DISP_GBR, (disp))
#define BIN_MOVW_R0_A_DISP_GBR(em, disp)        \
    EMIT_BIN_IMM8((em), OPCODE_MOVW_R0_A_DISP_GBR, (disp) / 2)
#define BIN_MOVL_R0_A_DISP_GBR(em, disp)        \
    EMIT_BIN_IMM8((em), OPCODE_MOVL_R0_A_DISP_GBR, (disp) / 4)

#define BIN_MOVB_A_DISP_GBR_R0(em, disp)                        \
    EMIT_BIN_IMM8((em), OPCODE_MOVB_A_DISP_GBR_R0, (disp))
#define BIN_MOVW_A_DISP_GBR_R0(em, disp)                        \
    EMIT_BIN_IMM8((em), OPCODE_MOVW_A_DISP_GBR_R0, (disp) / 2)
#define BIN_MOVL_A_DISP_GBR_R0(em, disp)                        \
    EMIT_BIN_IMM8((em), OPCODE_MOVL_A_DISP_GBR_R0, (disp) / 4)

#define BIN_MOVA_A_DISP_PC_R0(em, disp)         \
    EMIT_BIN_IMM8((em), OPCODE_MOVA_A_DISP_PC_R0, (disp) / 4)

// opcodes which take a 12-bit immediate as an input
#define MASK_OPCODE_IMM12 0xf000

#define BIN_BRA_DISP12(em, disp12)           \
    EMIT_BIN_IMM12(em, OPCODE_BRA_IMM12, (disp12) / 2 - 2)
#define BIN_BSR_IMM12(em, disp12)           \
    EMIT_BIN_IMM12(em, OPCODE_BSR_IMM12, (disp12) / 2 - 2)

// opcodes which take a general-purpose register and an 8-bit immediate as input
#define MASK_OPCODE_RN_IMM8 0xf000

#define BIN_MOV_IMM8_RN(em, imm8, rn)                   \
    EMIT_BIN_RN_IMM8(em, OPCODE_MOV_IMM8_RN, (rn), (imm8))
#define BIN_ADD_IMM8_RN(em, imm8, rn)                   \
    EMIT_BIN_RN_IMM8(em, OPCODE_ADD_IMM8_RN, (rn), (imm8))

#define BIN_MOVW_A_DISP_PC_RN(em, disp, rn)             \
    EMIT_BIN_RN_IMM8(em, OPCODE_MOVW_A_DISP_PC_RN, (rn), (disp) / 2)
#define BIN_MOVL_A_DISP_PC_RN(em, disp, rn)     \
    EMIT_BIN_RN_IMM8(em, OPCODE_MOVL_A_DISP_PC_RN, (rn), (disp) / 4)

// opcodes which take in two general-purpose registers
#define MASK_OPCODE_RM_RN 0xf00f

#define BIN_MOV_RM_RN(em, rm, rn)                       \
    EMIT_BIN_RM_RN((em), OPCODE_MOV_RM_RN, (rm), (rn))
#define BIN_SWAPB_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_SWAPB_RM_RN, (rm), (rn))
#define BIN_SWAPW_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_SWAPW_RM_RN, (rm), (rn))
#define BIN_XTRCT_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_XTRCT_RM_RN, (rm), (rn))
#define BIN_ADD_RM_RN(em, rm, rn)                       \
    EMIT_BIN_RM_RN((em), OPCODE_ADD_RM_RN, (rm), (rn))
#define BIN_ADDC_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_ADDC_RM_RN, (rm), (rn))
#define BIN_ADDV_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_ADDV_RM_RN, (rm), (rn))
#define BIN_CMPEQ_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_CMPEQ_RM_RN, (rm), (rn))
#define BIN_CMPHS_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_CMPHS_RM_RN, (rm), (rn))
#define BIN_CMPGE_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_CMPGE_RM_RN, (rm), (rn))
#define BIN_CMPHI_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_CMPHI_RM_RN, (rm), (rn))
#define BIN_CMPGT_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_CMPGT_RM_RN, (rm), (rn))
#define BIN_CMPSTR_RM_RN(em, rm, rn)                                   \
    EMIT_BIN_RM_RN((em), OPCODE_CMPSTR_RM_RN, (rm), (rn))
#define BIN_DIV1_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_DIV1_RM_RN, (rm), (rn))
#define BIN_DIV0S_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_DIV0S_RM_RN, (rm), (rn))
#define BIN_DMULSL_RM_RN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_DMULSL_RM_RN, (rm), (rn))
#define BIN_DMULUL_RM_RN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_DMULUL_RM_RN, (rm), (rn))
#define BIN_EXTSB_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_EXTSB_RM_RN, (rm), (rn))
#define BIN_EXTSW_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_EXTSW_RM_RN, (rm), (rn))
#define BIN_EXTUB_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_EXTUB_RM_RN, (rm), (rn))
#define BIN_EXTUW_RM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_EXTUW_RM_RN, (rm), (rn))
#define BIN_MULL_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_MULL_RM_RN, (rm), (rn))
#define BIN_MULSW_RM_RN(em, rm, rn)                     \
    EMIT_BIN_RM_RN((em), OPCODE_MULSW_RM_RN, (rm), (rn))
#define BIN_MULUW_RM_RN(em, rm, rn)                     \
    EMIT_BIN_RM_RN((em), OPCODE_MULUW_RM_RN, (rm), (rn))
#define BIN_NEG_RM_RN(em, rm, rn)                       \
    EMIT_BIN_RM_RN((em), OPCODE_NEG_RM_RN, (rm), (rn))
#define BIN_NEGC_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_NEGC_RM_RN, (rm), (rn))
#define BIN_SUB_RM_RN(em, rm, rn)                       \
    EMIT_BIN_RM_RN((em), OPCODE_SUB_RM_RN, (rm), (rn))
#define BIN_SUBC_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_SUBC_RM_RN, (rm), (rn))
#define BIN_SUBV_RM_RN(em, rm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_SUBV_RM_RN, (rm), (rn))
#define BIN_AND_RM_RN(em, rm, rn)               \
    EMIT_BIN_RM_RN((em), OPCODE_AND_RM_RN, (rm), (rn))
#define BIN_NOT_RM_RN(em, rm, rn)               \
    EMIT_BIN_RM_RN((em), OPCODE_NOT_RM_RN, (rm), (rn))
#define BIN_OR_RM_RN(em, rm, rn)                \
    EMIT_BIN_RM_RN((em), OPCODE_OR_RM_RN, (rm), (rn))
#define BIN_TST_RM_RN(em, rm, rn)               \
    EMIT_BIN_RM_RN((em), OPCODE_TST_RM_RN, (rm), (rn))
#define BIN_XOR_RM_RN(em, rm, rn)               \
    EMIT_BIN_RM_RN((em), OPCODE_XOR_RM_RN, (rm), (rn))
#define BIN_SHAD_RM_RN(em, rm, rn)              \
    EMIT_BIN_RM_RN((em), OPCODE_SHAD_RM_RN, (rm), (rn))
#define BIN_SHLD_RM_RN(em, rm, rn)              \
    EMIT_BIN_RM_RN((em), OPCODE_SHLD_RM_DN, (rm), (rn))

#define BIN_MOVB_RM_A_R0_RN(em, rm, rn)         \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_RM_A_R0_RN, (rm), (rn))
#define BIN_MOVW_RM_A_R0_RN(em, rm, rn)         \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_RM_A_R0_RN, (rm), (rn))
#define BIN_MOVL_RM_A_R0_RN(em, rm, rn)         \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_RM_A_R0_RN, (rm), (rn))

#define EMIT_BIN_MOVB_A_R0_RM_RN(em, rm, rn)    \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_A_R0_RM_RN, (rm), (rn))
#define EMIT_BIN_MOVW_A_R0_RM_RN(em, rm, rn)    \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_A_R0_RM_RN, (rm), (rn))
#define EMIT_BIN_MOVL_A_R0_RM_RN(em, rm, rn)    \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_A_R0_RM_RN, (rm), (rn))

#define BIN_MOVB_RM_ARN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_RM_ARN, (rm), (rn))
#define BIN_MOVW_RM_ARN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_RM_ARN, (rm), (rn))
#define BIN_MOVL_RM_ARN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_RM_ARN, (rm), (rn))

#define BIN_MOVB_ARM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_ARM_RN, (rm), (rn))
#define BIN_MOVW_ARM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_ARM_RN, (rm), (rn))
#define BIN_MOVL_ARM_RN(em, rm, rn)                             \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_ARM_RN, (rm), (rn))

#define BIN_MOVB_RM_AMRN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_RM_AMRN, (rm), (rn))
#define BIN_MOVW_RM_AMRN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_RM_AMRN, (rm), (rn))
#define BIN_MOVL_RM_AMRN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_RM_AMRN, (rm), (rn))

#define BIN_MOVB_ARMP_RN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVB_ARMP_RN, (rm), (rn))
#define BIN_MOVW_ARMP_RN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVW_ARMP_RN, (rm), (rn))
#define BIN_MOVL_ARMP_RN(em, rm, rn)                            \
    EMIT_BIN_RM_RN((em), OPCODE_MOVL_ARMP_RN, (rm), (rn))

#define BIN_MACL_ARMP_ARNP(em, rm, rn)          \
    EMIT_BIN_RM_RN((em), OPCODE_MACL_ARMP_ARNP, (rm), (rn))
#define BIN_MACH_ARMP_ARNP(em, rm, rn)          \
    EMIT_BIN_RM_RN((em), OPCODE_MACH_ARMP_ARNP, (rm), (rn))

#define BIN_FMOV_FRM_FRN(em, frm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FMOV_FRM_FRN, (frm), (frn))
#define BIN_FMOVS_ARM_FRN(em, rm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_ARM_FRN, (rm), (frn))
#define BIN_FMOVS_A_R0_RM_FRN(em, rm, frn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_A_R0_RM_FRN, (rm), (frn))
#define BIN_FMOVS_ARMP_FRN(em, rm, frn)                         \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_ARMP_FRN, (rm), (frn))
#define BIN_FMOVS_FRM_ARN(em, frm, rn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_FRM_ARN, (frm), (rn))
#define BIN_FMOVS_FRM_AMRN(em, frm, rn)                         \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_FRM_AMRN, (frm), (rn))
#define BIN_FMOVS_FRM_A_R0_RN(em, frm, rn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_FMOVS_FRM_A_R0_RN, (frm), (rn))

#define BIN_FADD_FRM_FRN(em, frm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FADD_FRM_FRN, (frm), (frn))
#define BIN_FCMPEQ_FRM_FRN(em, frm, frn)                        \
    EMIT_BIN_RM_RN((em), OPCODE_FCMPEQ_FRM_FRN, (frm), (frn))
#define BIN_FCMPGT_FRM_FRN(em, frm, frn)                        \
    EMIT_BIN_RM_RN((em), OPCODE_FCMPGT_FRM_FRN, (frm), (frn))
#define BIN_FDIV_FRM_FRN(em, frm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FDIV_FRM_FRN, (frm), (frn))
#define BIN_FMAC_FR0_FRM_FRN(em, frm, frn)                      \
    EMIT_BIN_RM_RN((em), OPCODE_FMAC_FR0_FRM_FRN, (frm), (frn))
#define BIN_FMUL_FRM_FRN(em, frm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FMUL_FRM_FRN, (frm), (frn))
#define BIN_FSUB_FRM_FRN(em, frm, frn)                          \
    EMIT_BIN_RM_RN((em), OPCODE_FSUB_FRM_FRN, (frm), (frn))

// opcodes that take in a general-purpose register and a banked register
#define MASK_OPCODE_RM_RNBANKED 0xf08f

#define BIN_LDC_RM_RNBANK(em, rm, rnbank)                               \
    EMIT_BIN_RM_RNBANK((em), OPCODE_LDC_RM_RNBANK, (rm), (rnbank))
#define BIN_LDCL_ARMP_RNBANK(em, rm, rnbank)                            \
    EMIT_BIN_RM_RNBANK((em), OPCODE_LDCL_ARMP_RNBANK, (rm), (rnbank))
#define BIN_STC_RMBANK_RN(em, rmbank, rn)                               \
    EMIT_BIN_RM_RNBANK((em), OPCODE_STC_RMBANK_RN, (rn), (rmbank))
#define BIN_STCL_RMBANK_AMRN(em, rmbank, rn)                            \
    EMIT_BIN_RM_RNBANK((em), OPCODE_STCL_RMBANK_AMRN, (rn), (rmbank))

// opcodes that take a general-purpose register and a 4-bit immediate value
#define MASK_OPCODE_RN_IMM4 0xff00

#define BIN_MOVB_R0_A_DISP_RN(em, disp, rn)     \
    EMIT_BIN_RN_IMM4((em), OPCODE_MOVB_R0_A_DISP_RN, (rn), (disp))
#define BIN_MOVW_R0_A_DISP_RN(em, disp, rn)                             \
    EMIT_BIN_RN_IMM4((em), OPCODE_MOVW_R0_A_DISP_RN, (rn), (disp) / 2)

#define BIN_MOVB_A_DISP_RM_R0(em, disp4, rm)                            \
    EMIT_BIN_RN_IMM4((em), OPCODE_MOVB_A_DISP_RM_R0, (rm), (disp4))
#define BIN_MOVW_A_DISP_RM_R0(em, disp4, rm)                            \
    EMIT_BIN_RN_IMM4((em), OPCODE_MOVW_A_DISP_RM_R0, (rm), (disp4) / 2)

#define MASK_OPCODE_RM_A_DISP_RN 0xf000

#define BIN_MOVL_RM_A_DISP_RN(em, rm, disp4, rn)                        \
    EMIT_BIN_RM_RN_IMM4((em), OPCODE_MOVL_RM_A_DISP_RN, (rm), (rn), (disp4) / 4)

#define BIN_MOVL_A_DISP_RM_RN(em, disp, rm, rn)                         \
    EMIT_BIN_RM_RN_IMM4((em), OPCODE_MOVL_A_DISP_RM_RN, (rm), (rn), (disp) / 4)

#define MASK_DRM_DRN 0xf11f

#define BIN_FMOV_DRM_DRN(em, drm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FMOV_DRM_DRN, (drm), (drn))
#define BIN_FADD_DRM_DRN(em, drm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FADD_DRM_DRN, (drm), (drn))
#define BIN_FCMPEQ_DRM_DRN(em, drm, drn)                        \
    EMIT_BIN_DRM_DRN((em), OPCODE_FCMPEW_DRM_DRN, (drm), (drn))
#define BIN_FCMPGT_DRM_DRN(em, drm, drn)                        \
    EMIT_BIN_DRM_DRN((em), OPCODE_FCMPGT_DRM_DRN, (drm), (drn))
#define BIN_FDIV_DRM_DRN(em, drm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FDIV_DRM_DRN, (drm), (drn))
#define BIN_FMUL_DRM_DRN(em, drm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FMUL_DRM_DRN, (drm), (drn))
#define BIN_FSUB_DRM_DRN(em, drm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FSUB_DRM_DRN, (drm), (drn))
#define BIN_FMOV_DRM_XDN(em, drm, xdn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FMOV_DRM_XDN, (drm), (xdn))
#define BIN_FMOV_XDM_DRN(em, xdm, drn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FMOV_XDM_DRN, (drm), (xdn))
#define BIN_FMOV_XDM_XDN(em, xdm, xdn)                          \
    EMIT_BIN_DRM_DRN((em), OPCODE_FMOV_XDM_XDN, (xdm), (xdn))

#define MASK_RM_DRN 0xf10f

#define BIN_FMOV_ARM_DRN(em, rm, drn)                           \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_RM_DRN, (rm), (drn))
#define BIN_FMOV_A_R0_RM_DRN(em, rm, drn)                       \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_A_R0_RM_DRN, (rm), (drn))
#define BIN_FMOV_ARMP_DRN(em, rm, drn)                          \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_ARMP_DRN, (rm), (drn))
#define BIN_FMOV_ARM_XDN(em, rm, xdn)                           \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_ARM_XDN, (rm), (xdn))
#define BIN_FMOV_ARMP_XDN(em, rm, xdn)                          \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_ARMP_XDN, (rm), (xdn))
#define BIN_FMOV_A_R0_RM_XDN(em, rm, xdn)                       \
    EMIT_BIN_RM_DRN((em), OPCODE_FMOV_A_R0_RM_XDN, (rm), (xdn))

#define MASK_DRM_RN 0xf01f

#define BIN_FMOV_DRM_ARN(em, drm, rn)                           \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_DRM_ARN, (drm), (rn))
#define BIN_FMOV_DRM_AMRN(em, drm, rn)                          \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_DRM_ARN, (drm), (rn))
#define BIN_FMOV_DRM_A_R0_RN(em, drm, rn)                       \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_DRM_A_R0_RN, (drm), (rn))
#define BIN_FMOV_XDM_ARN(em, xdm, rn)                           \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_XDM_ARN, (xdm), (rn))
#define BIN_FMOV_XDM_AMRN(em, xdm, rn)                          \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_XDM_AMRN, (xdm), (rn))
#define BIN_FMOV_XDM_A_R0_RN(em, xdm, rn)                       \
    EMIT_BIN_DRM_RN((em), OPCODE_FMOV_XDM_A_R0_RN, (xdm), (rn))

#define MASK_DRN 0xf1ff

#define BIN_FABS_DRN(em, drn)                   \
    EMIT_BIN_DRN((em), OPCODE_FABS_DRN, (drn))
#define BIN_FCNVDS_DRM_FPUL(em, drm)                    \
    EMIT_BIN_DRN((em), OPCODE_FCNVDS_DRM_FPUL, (drm))
#define BIN_FCNVSD_FPUL_DRN(em, drn)                    \
    EMIT_BIN_DRN((em), OPCODE_FCNVSD_FPUL_DRN, (drn))
#define BIN_FLOAT_FPUL_DRN(em, drn)                     \
    EMIT_BIN_DRN((em), OPCODE_FLOAT_FPUL_DRN, (drn))
#define BIN_FNEG_DRN(em, drn)                   \
    EMIT_BIN_DRN((em), OPCODE_FNEG_DRN, (drn))
#define BIN_FSQRT_DRN(em, drn)                  \
    EMIT_BIN_DRN((em), OPCODE_FSQRT_DRN, (drn))
#define BIN_FTRC_DRM_FPUL(em, drm)                      \
    EMIT_BIN_DRN((em), OPCODE_FTRC_DRM_FPUL, (drm))
#define BIN_FSCA_FPUL_DRN(em, drn)                      \
    EMIT_BIN_DRN((em), OPCODE_FSCA_FPUL_DRN, (drn))

// the only opcode which takes as input two vector registers
#define MASK_FVM_FVN 0xf0ff

#define BIN_FIPR_FVM_FVN(em, fvm, fvn)          \
    EMIT_BIN_FVM_FVN((em), OPCODE_FIPR_FVM_FVN, (fvm), (fvn))

// the only opcode which takes as input a single vector register
#define MASK_FVN 0xf3ff

#define BIN_FTRV_XMTRX_FVN(em, fvn)             \
    EMIT_BIN_FVN((em), OPCODE_FTRV_XMTRX_FVN, (fvn))

#ifdef __cplusplus
}
#endif

#endif
