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

#ifndef SH4_OPCODES_H_
#define SH4_OPCODES_H_

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xffff
 *
 ******************************************************************************/

#define OPCODE_DIV0U  0x0019 // DIV0U
#define OPCODE_RTS    0x000b // RTS
#define OPCODE_CLRMAC 0x0028 // CLRMAC
#define OPCODE_CLRS   0x0048 // CLRS
#define OPCODE_CLRT   0x0008 // CLRT
#define OPCODE_LDTLB  0x0038 // LDTLB
#define OPCODE_NOP    0x0009 // NOP
#define OPCODE_RTE    0x002b // RTE
#define OPCODE_SETS   0x0058 // SETS
#define OPCODE_SETT   0x0018 // SETT
#define OPCODE_SLEEP  0x001b // SLEEP
#define OPCODE_FRCHG  0xfbfd // FRCHG
#define OPCODE_FSCHG  0xf3fd // FSCHG

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf0ff
 *
 ******************************************************************************/

#define OPCODE_MOVT_RN   0x0029 // MOVT Rn
#define OPCODE_CMPPZ_RN  0x4011 // CMP/PZ Rn
#define OPCODE_CMPPL_RN  0x4015 // CMP/PL Rn
#define OPCODE_DT_RN     0x4010 // DT Rn
#define OPCODE_ROTL_RN   0x4004 // ROTL Rn
#define OPCODE_ROTR_RN   0x4005 // ROTR Rn
#define OPCODE_ROTCL_RN  0x4024 // ROTCL Rn
#define OPCODE_ROTCR_RN  0x4025 // ROTCR Rn
#define OPCODE_SHAL_RN   0x4020 // SHAL Rn
#define OPCODE_SHAR_RN   0x4021 // SHAR Rn
#define OPCODE_SHLL_RN   0x4000 // SHLL Rn
#define OPCODE_SHLR_RN   0x4001 // SHLR Rn
#define OPCODE_SHLL2_RN  0x4008 // SHLL2 Rn
#define OPCODE_SHLR2_RN  0x4009 // SHLR2 Rn
#define OPCODE_SHLL8_RN  0x4018 // SHLL8 Rn
#define OPCODE_SHLR8_RN  0x4019 // SHLR8 Rn
#define OPCODE_SHLL16_RN 0x4028 // SHLL16 Rn
#define OPCODE_SHLR16_RN 0x4029 // SHLR16 Rn
#define OPCODE_BRAF_RN   0x0023 // BRAF Rn
#define OPCODE_BSRF_RN   0x0003 // BSRF Rn

#define OPCODE_TASB_ARN  0x401b // TAS.B @Rn
#define OPCODE_OCBI_ARN  0x0093 // OCBI @Rn
#define OPCODE_OCBP_ARN  0x00a3 // OCBP @Rn
#define OPCODE_OCBWB_ARN 0x00b3 // OCBWB @Rn
#define OPCODE_PREF_ARN  0x0083 // PREF @Rn
#define OPCODE_JMP_ARN   0x402b // JMP @Rn
#define OPCODE_JSR_ARN   0x400b // JSR @Rn

#define OPCODE_LDC_RM_SR  0x400e // LDC Rm, SR
#define OPCODE_LDC_RM_GBR 0x401e // LDC Rm, GBR
#define OPCODE_LDC_RM_VBR 0x402e // LDC Rm, VBR
#define OPCODE_LDC_RM_SSR 0x403e // LDC Rm, SSR
#define OPCODE_LDC_RM_SPC 0x404e // LDC Rm, SPC
#define OPCODE_LDC_RM_DBR 0x40fa // LDC Rm, DBR

#define OPCODE_STC_SR_RN  0x0002 // STC SR, Rn
#define OPCODE_STC_GBR_RN 0x0012 // STC GBR, Rn
#define OPCODE_STC_VBR_RN 0x0022 // STC VBR, Rn
#define OPCODE_STC_SSR_RN 0x0032 // STC SSR, Rn
#define OPCODE_STC_SPC_RN 0x0042 // STC SPC, Rn
#define OPCODE_STC_SGR_RN 0x003a // STC SGR, Rn
#define OPCODE_STC_DBR_RN 0x00fa // STC DBR, Rn

#define OPCODE_LDCL_ARMP_SR 0x4007  // LDC.L @Rm+, SR
#define OPCODE_LDCL_ARMP_GBR 0x4017 // LDC.L @Rm+, GBR
#define OPCODE_LDCL_ARMP_VBR 0x4027 // LDC.L @Rm+, VBR
#define OPCODE_LDCL_ARMP_SSR 0x4037 // LDC.L @Rm+, SSR
#define OPCODE_LDCL_ARMP_SPC 0x4047 // LDC.L @Rm+, SPC
#define OPCODE_LDCL_ARMP_DBR 0x40f6 // LDC.L @Rm+, DBR

#define OPCODE_STCL_SR_AMRN 0x4003  // STC.L SR, @-Rn
#define OPCODE_STCL_GBR_AMRN 0x4013 // STC.L GBR, @-Rn
#define OPCODE_STCL_VBR_AMRN 0x4023 // STC.L VBR, @-Rn
#define OPCODE_STCL_SSR_AMRN 0x4033 // STC.L SSR, @-Rn
#define OPCODE_STCL_SPC_AMRN 0x4043 // STC.L SPC, @-Rn
#define OPCODE_STCL_SGR_AMRN 0x4032 // STC.L SGR, @-Rn
#define OPCODE_STCL_DBR_AMRN 0x40f2 // STC.L DBR, @-Rn

#define OPCODE_LDS_RM_MACH     0x400a // LDS Rm, MACH
#define OPCODE_LDS_RM_MACL     0x401a // LDS Rm, MACL
#define OPCODE_STS_MACH_RN     0x000a // STS MACH, Rn
#define OPCODE_STS_MACL_RN     0x001a // STS MACL, Rn
#define OPCODE_LDS_RM_PR       0x402a // LDS Rm, PR
#define OPCODE_STS_PR_RN       0x002a // STS PR, Rn
#define OPCODE_LDSL_ARMP_MACH  0x4006 // LDS.L @Rm+, MACH
#define OPCODE_LDSL_ARMP_MACL  0x4016 // LDS.L @Rm+, MACL
#define OPCODE_STSL_MACH_AMRN  0x4002 // STS.L MACH, @-Rn
#define OPCODE_STSL_MACL_AMRN  0x4012 // STS.L MACL, @-Rn
#define OPCODE_LDSL_ARMP_PR    0x4026 // LDS.L @Rm+, PR
#define OPCODE_STSL_PR_AMRN    0x4022 // STS.L PR, @-Rn
#define OPCODE_LDS_RM_FPSCR    0x406a // LDS Rm, FPSCR
#define OPCODE_LDS_RM_FPUL     0x405a // LDS Rm, FPUL
#define OPCODE_LDSL_ARMP_FPSCR 0x4066 // LDS.L @Rm+, FPSCR
#define OPCODE_LDSL_ARMP_FPUL  0x4056 // LDS.L @Rm+, FPUL
#define OPCODE_STS_FPSCR_RN    0x006a // STS FPSCR, Rn
#define OPCODE_STS_FPUL_RN     0x005a // STS FPUL, Rn
#define OPCODE_STSL_FPSCR_AMRN 0x4062 // STS.L FPSCR, @-Rn
#define OPCODE_STSL_FPUL_AMRN  0x4052 // STS.L FPUL, @-Rn

#define OPCODE_MOVCAL_R0_ARN 0x00c3

#define OPCODE_FLDI0_FRN 0xf08d      // FLDI0 FRn
#define OPCODE_FLDI1_FRN 0xf09d      // FLDI1 FRn

#define OPCODE_FLDS_FRM_FPUL  0xf01d // FLDS FRm, FPUL
#define OPCODE_FSTS_FPUL_FRN  0xf00d // FSTS FPUL, FRn
#define OPCODE_FABS_FRN       0xf05d // FABS FRn
#define OPCODE_FLOAT_FPUL_FRN 0xf02d // FLOAT FPUL, FRn
#define OPCODE_FNEG_FRN       0xf04d // FNEG FRn
#define OPCODE_FSQRT_FRN      0xf06d // FSQRT FRn
#define OPCODE_FTRC_FRM_FPUL  0xf03d // FTRC FRm, FPUL
#define OPCODE_FSRRA_FRN      0xf07d // FSRRA FRn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xff00
 *
 ******************************************************************************/

#define OPCODE_CMPEQ_IMM8_R0      0x8800 // CMP/EQ #imm, R0
#define OPCODE_ANDB_IMM8_A_R0_GBR 0xcd00 // AND.B #imm, @(R0, GBR)
#define OPCODE_AND_IMM8_R0        0xc900 // AND #imm, R0
#define OPCODE_ORB_IMM8_A_R0_GBR  0xcf00 // OR.B #imm, @(R0, GBR)
#define OPCODE_OR_IMM8_R0         0xcb00 // OR #imm, R0
#define OPCODE_TST_IMM8_R0        0xc800 // TST #imm, R0
#define OPCODE_TSTB_IMM8_A_R0_GBR 0xcc00 // TST.B #imm, @(R0, GBR)
#define OPCODE_XOR_IMM8_R0        0xca00 // XOR #imm, R0
#define OPCODE_XORB_IMM8_A_R0_GBR 0xce00 // XOR.B #imm, @(R0, GBR)
#define OPCODE_BF_DISP8           0x8b00 // BF label
#define OPCODE_BFS_DISP8          0x8f00 // BF/S label
#define OPCODE_BT_DISP8           0x8900 // BT label
#define OPCODE_BTS_DISP8          0x8d00 // BT/S label
#define OPCODE_TRAPA_IMM8         0xc300 // TRAPA #immed

#define OPCODE_MOVB_R0_A_DISP8_GBR 0xc000 // MOV.B R0, @(disp, GBR)
#define OPCODE_MOVW_R0_A_DISP8_GBR 0xc100 // MOV.W R0, @(disp, GBR)
#define OPCODE_MOVL_R0_A_DISP8_GBR 0xc200 // MOV.L R0, @(disp, GBR)

#define OPCODE_MOVB_A_DISP8_GBR_R0 0xc400 // MOV.B @(disp, GBR), R0
#define OPCODE_MOVW_A_DISP8_GBR_R0 0xc500 // MOV.W @(disp, GBR), R0
#define OPCODE_MOVL_A_DISP8_GBR_R0 0xc600 // MOV.L @(disp, GBR), R0

#define OPCODE_MOVA_A_DISP8_PC_R0 0xc700 // MOVA @(disp, PC), R0

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf000
 *
 ******************************************************************************/

#define OPCODE_BRA_DISP12 0xa000 // BRA label
#define OPCODE_BSR_DISP12 0xb000 // BSR label

#define OPCODE_MOV_IMM8_RN 0xe000 // MOV #imm, Rn
#define OPCODE_ADD_IMM8_RN 0x7000 // ADD #imm, Rn

#define OPCODE_MOVW_A_DISP8_PC_RN 0x9000 // MOV.W @(disp, PC), Rn
#define OPCODE_MOVL_A_DISP8_PC_RN 0xd000 // MOV.L @(disp, PC), Rn

#define OPCODE_MOVL_RM_A_DISP4_RN 0x1000 // MOV.L Rm, @(disp, Rn)

#define OPCODE_MOVL_A_DISP4_RM_RN 0x5000 // MOV.L @(disp, Rm), Rn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf00f
 *
 ******************************************************************************/

#define OPCODE_MOV_RM_RN    0x6003 // MOV Rm, Rn
#define OPCODE_SWAPB_RM_RN  0x6008 // SWAP.B Rm, Rn
#define OPCODE_SWAPW_RM_RN  0x6009 // SWAP.W Rm, Rn
#define OPCODE_XTRCT_RM_RN  0x200d // XTRCT Rm, Rn
#define OPCODE_ADD_RM_RN    0x300c // ADD Rm, Rn
#define OPCODE_ADDC_RM_RN   0x300e // ADDC Rm, Rn
#define OPCODE_ADDV_RM_RN   0x300f // ADDV Rm, Rn
#define OPCODE_CMPEQ_RM_RN  0x3000 // CMP/EQ Rm, Rn
#define OPCODE_CMPHS_RM_RN  0x3002 // CMP/HS Rm, Rn
#define OPCODE_CMPGE_RM_RN  0x3003 // CMP/GE Rm, Rn
#define OPCODE_CMPHI_RM_RN  0x3006 // CMP/HI Rm, Rn
#define OPCODE_CMPGT_RM_RN  0x3007 // CMP/GT Rm, Rn
#define OPCODE_CMPSTR_RM_RN 0x200c // CMP/STR Rm, Rn
#define OPCODE_DIV1_RM_RN   0x3004 // DIV1 Rm, Rn
#define OPCODE_DIV0S_RM_RN  0x2007 // DIV0S Rm, Rn
#define OPCODE_DMULSL_RM_RN 0x300d // DMULS.L Rm, Rn
#define OPCODE_DMULUL_RM_RN 0x3005 // DMULU.L Rm, Rn
#define OPCODE_EXTSB_RM_RN  0x600e // EXTS.B Rm, Rn
#define OPCODE_EXTSW_RM_RN  0x600f // EXTS.W Rm, Rn
#define OPCODE_EXTUB_RM_RN  0x600c // EXTU.B Rm, Rn
#define OPCODE_EXTUW_RM_RN  0x600d // EXTU.W Rm, Rn
#define OPCODE_MULL_RM_RN   0x0007 // MUL.L Rm, Rn
#define OPCODE_MULSW_RM_RN  0x200f // MULS.W Rm, Rn
#define OPCODE_MULUW_RM_RN  0x200e // MULU.W Rm, Rn
#define OPCODE_NEG_RM_RN    0x600b // NEG Rm, Rn
#define OPCODE_NEGC_RM_RN   0x600a // NEGC Rm, Rn
#define OPCODE_SUB_RM_RN    0x3008 // SUB Rm, Rn
#define OPCODE_SUBC_RM_RN   0x300a // SUBC Rm, Rn
#define OPCODE_SUBV_RM_RN   0x300b // SUBV Rm, Rn
#define OPCODE_AND_RM_RN    0x2009 // AND Rm, Rn
#define OPCODE_NOT_RM_RN    0x6007 // NOT Rm, Rn
#define OPCODE_OR_RM_RN     0x200b // OR Rm, Rn
#define OPCODE_TST_RM_RN    0x2008 // TST Rm, Rn
#define OPCODE_XOR_RM_RN    0x200a // XOR Rm, Rn
#define OPCODE_SHAD_RM_RN   0x400c // SHAD Rm, Rn
#define OPCODE_SHLD_RM_RN   0x400d // SHLD Rm, Rn

#define OPCODE_MOVB_RM_A_R0_RN 0x0004 // MOV.B Rm, @(R0, Rn)
#define OPCODE_MOVW_RM_A_R0_RN 0x0005 // MOV.W Rm, @(R0, Rn)
#define OPCODE_MOVL_RM_A_R0_RN 0x0006 // MOV.L Rm, @(R0, Rn)

#define OPCODE_MOVB_A_R0_RM_RN 0x000c // MOV.B @(R0, Rm), Rn
#define OPCODE_MOVW_A_R0_RM_RN 0x000d // MOV.W @(R0, Rm), Rn
#define OPCODE_MOVL_A_R0_RM_RN 0x000e // MOV.L @(R0, Rm), Rn

#define OPCODE_MOVB_RM_ARN 0x2000 // MOV.B Rm, @Rn
#define OPCODE_MOVW_RM_ARN 0x2001 // MOV.W Rm, @Rn
#define OPCODE_MOVL_RM_ARN 0x2002 // MOV.L Rm, @Rn

#define OPCODE_MOVB_ARM_RN 0x6000 // MOV.B @Rm, Rn
#define OPCODE_MOVW_ARM_RN 0x6001 // MOV.W @Rm, Rn
#define OPCODE_MOVL_ARM_RN 0x6002 // MOV.L @Rm, Rn

#define OPCODE_MOVB_RM_AMRN 0x2004 // MOV.B Rm, @-Rn
#define OPCODE_MOVW_RM_AMRN 0x2005 // MOV.W Rm, @-Rn
#define OPCODE_MOVL_RM_AMRN 0x2006 // MOV.L Rm, @-Rn

#define OPCODE_MOVB_ARMP_RN 0x6004 // MOV.B @Rm+, Rn
#define OPCODE_MOVW_ARMP_RN 0x6005 // MOV.W @Rm+, Rn
#define OPCODE_MOVL_ARMP_RN 0x6006 // MOV.L @Rm+, Rn

#define OPCODE_MACL_ARMP_ARNP 0x000f // MAC.L @Rm+, @Rn+
#define OPCODE_MACW_ARMP_ARNP 0x400f // MAC.W @Rm+, @Rn+

#define OPCODE_FMOV_FRM_FRN 0xf00c      // FMOV FRm, FRn
#define OPCODE_FMOVS_ARM_FRN 0xf008     // FMOV.S @Rm, FRn
#define OPCODE_FMOVS_A_R0_RM_FRN 0xf006 // FMOV.S @(R0,Rm), FRn
#define OPCODE_FMOVS_ARMP_FRN 0xf009    // FMOV.S @Rm+, FRn
#define OPCODE_FMOVS_FRM_ARN 0xf00a     // FMOV.S FRm, @Rn
#define OPCODE_FMOVS_FRM_AMRN 0xf00b    // FMOV.S FRm, @-Rn
#define OPCODE_FMOVS_FRM_A_R0_RN 0xf007 // FMOV.S FRm, @(R0, Rn)

#define OPCODE_FADD_FRM_FRN     0xf000 // FADD FRm, FRn
#define OPCODE_FCMPEQ_FRM_FRN   0xf004 // FCMP/EQ FRm, FRn
#define OPCODE_FCMPGT_FRM_FRN   0xf005 // FCMP/GT FRm, FRn
#define OPCODE_FDIV_FRM_FRN     0xf003 // FDIV FRm, FRn
#define OPCODE_FMAC_FR0_FRM_FRN 0xf00e // FMAC FR0, FRm, FRn
#define OPCODE_FMUL_FRM_FRN     0xf002 // FMUL FRm, FRn
#define OPCODE_FSUB_FRM_FRN     0xf001 // FSUB FRm, FRn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf08f
 *
 ******************************************************************************/

#define OPCODE_LDC_RM_RN_BANK 0x408e    // LDC Rm, Rn_BANK
#define OPCODE_LDCL_ARMP_RN_BANK 0x4087 // LDC.L @Rm+, Rn_BANK
#define OPCODE_STC_RM_BANK_RN 0x0082    // STC Rm_BANK, Rn
#define OPCODE_STCL_RM_BANK_AMRN 0x4083 // STC.L Rm_BANK, @-Rn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xff00
 *
 ******************************************************************************/

#define OPCODE_MOVB_R0_A_DISP4_RN 0x8000 // MOV.B R0, @(disp, Rn)
#define OPCODE_MOVW_R0_A_DISP4_RN 0x8100 // MOV.W R0, @(disp, Rn)

#define OPCODE_MOVB_A_DISP4_RM_R0 0x8400 // MOV.B @(disp, Rm), R0
#define OPCODE_MOVW_A_DISP4_RM_R0 0x8500 // MOV.W @(disp, Rm), R0

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf11f
 *
 ******************************************************************************/

#define OPCODE_FMOV_DRM_DRN   0xf00c // FMOV DRm, DRn
#define OPCODE_FADD_DRM_DRN   0xf000 // FADD DRm, DRn
#define OPCODE_FCMPEQ_DRM_DRN 0xf004 // FCMP/EQ DRm, DRn
#define OPCODE_FCMPGT_DRM_DRN 0xf005 // FCMP/GT DRm, DRn
#define OPCODE_FDIV_DRM_DRN   0xf003 // FDIV DRm, DRn
#define OPCODE_FMUL_DRM_DRN   0xf002 // FMUL DRm, DRn
#define OPCODE_FSUB_DRM_DRN   0xf001 // FSUB DRm, DRn
#define OPCODE_FMOV_DRM_XDN   0xf10c // FMOV DRm, XDn
#define OPCODE_FMOV_XDM_DRN   0xf01c // FMOV XDm, DRn
#define OPCODE_FMOV_XDM_XDN   0xf11c // FMOV XDm, XDn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf10f
 *
 ******************************************************************************/

#define OPCODE_FMOV_ARM_DRN 0xf008     // FMOV @Rm, DRn
#define OPCODE_FMOV_A_R0_RM_DRN 0xf006 // FMOV @(R0, Rm), DRn
#define OPCODE_FMOV_ARMP_DRN 0xf009    // FMOV @Rm+, DRn
#define OPCODE_FMOV_ARM_XDN 0xf108     // FMOV @Rm, XDn
#define OPCODE_FMOV_ARMP_XDN 0xf109    // FMOV @Rm+, XDn
#define OPCODE_FMOV_A_R0_RM_XDN 0xf106 // FMOV @(R0, Rm), XDn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf01f
 *
 ******************************************************************************/

#define OPCODE_FMOV_DRM_ARN 0xf00a     // FMOV DRm, @Rn
#define OPCODE_FMOV_DRM_AMRN 0xf00b    // FMOV DRm, @-Rn
#define OPCODE_FMOV_DRM_A_R0_RN 0xf007 // FMOV DRm, @(R0,Rn)
#define OPCODE_FMOV_XDM_ARN     0xf01a // FMOV XDm, @Rn
#define OPCODE_FMOV_XDM_AMRN    0xf01b // FMOV XDm, @-Rn
#define OPCODE_FMOV_XDM_A_R0_RN 0xf017 // FMOV XDm, @(R0, Rn)

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf1ff
 *
 ******************************************************************************/

#define OPCODE_FABS_DRN        0xf05d // FABS DRn
#define OPCODE_FCNVDS_DRM_FPUL 0xf0bd // FCNVDS DRm, FPUL
#define OPCODE_FCNVSD_FPUL_DRN 0xf0ad // FCNVSD FPUL, DRn
#define OPCODE_FLOAT_FPUL_DRN  0xf02d // FLOAT FPUL, DRn
#define OPCODE_FNEG_DRN        0xf04d // FNEG DRn
#define OPCODE_FSQRT_DRN       0xf06d // FSQRT DRn
#define OPCODE_FTRC_DRM_FPUL   0xf03d // FTRC DRm, FPUL
#define OPCODE_FSCA_FPUL_DRN   0xf0fd // FSCA FPUL, DRn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf0ff
 *
 ******************************************************************************/

#define OPCODE_FIPR_FVM_FVN 0xf0ed // FIPR FVm, FVn

/*******************************************************************************
 *
 * The following opcodes have a mask of 0xf3ff
 *
 ******************************************************************************/

#define OPCODE_FTRV_XMTRX_FVN 0xf1fd // FTRV XMTRX, FVn

#endif
