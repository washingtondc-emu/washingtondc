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

#ifndef SH4_ASM_EMIT_H_
#define SH4_ASM_EMIT_H_

#include <stdio.h>

typedef void(*asm_emit_handler_func)(char);

void emit_str(asm_emit_handler_func em, char const *txt);

static char const *gen_reg_str(unsigned idx) {
    static char const* names[16] = {
        "r0", "r1",   "r2",  "r3",
        "r4", "r5",   "r6",  "r7",
        "r8", "r9",   "r10", "r11",
        "r12", "r13", "r14", "r15"
    };
    return names[idx & 15];
}

static char const *bank_reg_str(unsigned idx) {
    static char const *names[16] = {
        "r0_bank",  "r1_bank",  "r2_bank",  "r3_bank",
        "r4_bank",  "r5_bank",  "r6_bank",  "r7_bank",
        "r8_bank",  "r9_bank",  "r10_bank", "r11_bank",
        "r12_bank", "r13_bank", "r14_bank", "r15_bank"
    };
    return names[idx & 15];
}

static char const *fr_reg_str(unsigned idx) {
    static char const *names[16] = {
        "fr0",  "fr1",  "fr2",  "fr3",
        "fr4",  "fr5",  "fr6",  "fr7",
        "fr8",  "fr9",  "fr10", "fr11",
        "fr12", "fr13", "fr14", "fr15"
    };
    return names[idx & 15];
}

static char const *dr_reg_str(unsigned idx) {
    static char const *names[8] = {
        "dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14"
    };
    return names[(idx >> 1) & 7];
}

static char const *xd_reg_str(unsigned idx) {
    static char const *names[8] = {
        "xd0", "xd2", "xd4", "xd6", "xd8", "xd10", "xd12", "xd14"
    };
    return names[(idx >> 1) & 7];
}

static char const *fv_reg_str(unsigned idx) {
    static char const *names[4] = {
        "fv0", "fv4", "fv8", "fv12"
    };
    return names[(idx >> 2) & 3];
}

static char const *imm8_str(unsigned imm8, unsigned shift) {
    // TODO: pad output to two digits
    static char buf[8];
    snprintf(buf, sizeof(buf), "0x%x", //"0x%02x",
             imm8 & ((256 << shift) - 1) & ~((1 << shift) - 1));
    buf[7] = '\0';
    return buf;
}

static char const *imm12_str(unsigned imm12, unsigned shift) {
    // TODO: pad output to three digits
    static char buf[8];
    snprintf(buf, sizeof(buf), "0x%x", //"0x%03x",
             imm12 & ((4096 << shift) - 1) & ~((1 << shift) - 1));
    buf[7] = '\0';
    return buf;
}

static char const *disp4_str(unsigned disp4, unsigned shift) {
    // convert to hex
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", //"0x%x",
             disp4 & ((16 << shift) - 1) & ~((1 << shift)-1));
    buf[7] = '\0';
    return buf;
}

static char const *disp8_str(unsigned disp8, unsigned shift) {
    // TODO: pad output to two hex digits
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", //"0x%02x",
             disp8 & ((256 << shift) - 1) & ~((1 << shift)-1));
    buf[7] = '\0';
    return buf;
}

// OP
#define DEF_ASM_NOARG(op, lit)                                          \
    static inline void sh4_asm_##op(asm_emit_handler_func em) {         \
        emit_str(em, lit);                                              \
    }

// OP Rn
#define DEF_ASM_RN(op, lit)                                             \
    static inline void sh4_asm_##op##_rn(asm_emit_handler_func em,      \
                                         unsigned rn) {                 \
        emit_str(em, lit);                                              \
        emit_str(em, " ");                                              \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Rm, REG
#define DEF_ASM_RM_REG(op, lit, reg)                                    \
    static inline void sh4_asm_##op##_rm_##reg(asm_emit_handler_func em, \
                                               unsigned rm) {           \
        emit_str(em, lit" ");                                           \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", " #reg);                                        \
    }

// OP REG, Rn
#define DEF_ASM_REG_RN(op, lit, reg)                                    \
    static inline void sh4_asm_##op##_##reg##_rn(asm_emit_handler_func em, \
                                                 unsigned rm) {         \
        emit_str(em, lit" " #reg ", ");                                 \
        emit_str(em, gen_reg_str(rm));                                  \
    }

// OP @Rn
#define DEF_ASM_ARN(op, lit)                                            \
    static inline void sh4_asm_##op##_arn(asm_emit_handler_func em,     \
                                          unsigned rn) {                \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP @Rm+, REG
#define DEF_ASM_ARMP_REG(op, lit, reg)          \
    static inline void sh4_asm_##op##_armp_##reg(asm_emit_handler_func em, \
                                                 unsigned rm) {         \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "+, " #reg);                                       \
    }

// OP REG, @-Rn
#define DEF_ASM_REG_AMRN(op, lit, reg)                                  \
    static inline void sh4_asm_##op##_##reg##_amrn(asm_emit_handler_func em, \
                                                   unsigned rn) {       \
        emit_str(em, lit " " #reg ", @-");                              \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP REG, @Rn
#define DEF_ASM_REG_ARN(op, lit, reg)                                   \
    static inline void sh4_asm_##op##_##reg##_arn(asm_emit_handler_func em, \
                                                   unsigned rn) {       \
        emit_str(em, lit " " #reg ", @");                               \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP FRn
#define DEF_ASM_FRN(op, lit)                                            \
    static inline void sh4_asm_##op##_frn(asm_emit_handler_func em,     \
                                          unsigned frn) {               \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP FRm, REG
#define DEF_ASM_FRM_REG(op, lit, reg)                                   \
    static inline void sh4_asm_##op##_frm_##reg(asm_emit_handler_func em, \
                                                unsigned frm) {         \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", " #reg);                                        \
    }

// OP REG, FRn
#define DEF_ASM_REG_FRN(op, lit, reg)                                   \
    static inline void sh4_asm_##op##_##reg##_frn(asm_emit_handler_func em, \
                                                  unsigned frn) {       \
        emit_str(em, lit " " #reg ", ");                                \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP #imm8, REG
#define DEF_ASM_IMM8_REG(op, lit, reg, imm_shift)                       \
    static inline void sh4_asm_##op##_imm8_##reg(asm_emit_handler_func em, \
                                                 unsigned imm8) {       \
        emit_str(em, lit " #");                                         \
        emit_str(em, imm8_str(imm8, imm_shift));                        \
        emit_str(em, ", " #reg);                                        \
    }

// OP #imm8, @(REG1, REG2)
#define DEF_ASM_IMM8_A_REG_REG(op, lit, reg1, reg2, imm_shift)          \
    static inline void                                                  \
    sh4_asm_##op##_imm8_a_##reg1##_##reg2(asm_emit_handler_func em,     \
                                          unsigned imm8) {              \
        emit_str(em, lit " #");                                         \
        emit_str(em, imm8_str(imm8, imm_shift));                        \
        emit_str(em, ", @(" #reg1 ", " #reg2 ")");                      \
    }

// OP REG1, @(disp8, REG2)
#define DEF_ASM_REG_A_DISP8_REG(op, lit, reg1, reg2, disp_shift)        \
    static inline void                                                  \
    sh4_asm_##op##_##reg1##_a_disp8_##reg2(asm_emit_handler_func em,    \
                                           unsigned disp8) {            \
        emit_str(em, lit " " #reg1 ", @(");                             \
        emit_str(em, disp8_str(disp8, disp_shift));                     \
        emit_str(em, ", " #reg2 ")");                                   \
    }

// OP @(disp8, REG1), REG2
#define DEF_ASM_A_DISP8_REG1_REG2(op, lit, reg1, reg2, disp_shift)      \
    static inline void                                                  \
    sh4_asm_##op##_a_disp8_##reg1##_##reg2(asm_emit_handler_func em,    \
                                         unsigned disp8) {              \
        emit_str(em, lit " @(");                                        \
        emit_str(em, disp8_str(disp8, disp_shift));                     \
        emit_str(em, ", " #reg1 "), " #reg2);                           \
    }

// OP disp8
#define DEF_ASM_DISP8(op, lit, disp_shift)                              \
    static inline void sh4_asm_##op##_disp8(asm_emit_handler_func em,   \
                                            unsigned disp8) {           \
        emit_str(em, lit " ");                                          \
        emit_str(em, disp8_str(disp8, disp_shift));                     \
    }

// OP #imm8
#define DEF_ASM_IMM8(op, lit, imm_shift)                                \
    static inline void sh4_asm_##op##_imm8(asm_emit_handler_func em,    \
                                           unsigned imm8) {             \
        emit_str(em, lit " #");                                         \
        emit_str(em, imm8_str(imm8, imm_shift));                        \
    }

// OP offs12
#define DEF_ASM_OFFS12(op, lit, imm_shift)                              \
    static inline void sh4_asm_##op##_offs12(asm_emit_handler_func em,  \
                                             unsigned imm12) {          \
        emit_str(em, lit " ");                                          \
        emit_str(em, imm12_str(imm12, imm_shift));                      \
    }

// OP #imm8, Rn
#define DEF_ASM_IMM8_RN(op, lit, imm_shift)                             \
    static inline void sh4_asm_##op##_imm8_rn(asm_emit_handler_func em, \
                                              unsigned imm8, unsigned rn) { \
        emit_str(em, lit " #");                                         \
        emit_str(em, imm8_str(imm8, imm_shift));                        \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP @(disp8, REG), Rn
#define DEF_ASM_A_DISP8_REG_RN(op, lit, reg, disp_shift)                \
    static inline void                                                  \
    sh4_asm_##op##_a_disp8_##reg##_rn(asm_emit_handler_func em,         \
                                  unsigned disp8, unsigned rn) {        \
        emit_str(em, lit " @(");                                        \
        emit_str(em, disp8_str(disp8, disp_shift));                     \
        emit_str(em, ", " #reg "), ");                                  \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Rm, Rn
#define DEF_ASM_RM_RN(op, lit)                                          \
    static inline void sh4_asm_##op##_rm_rn(asm_emit_handler_func em,   \
                                            unsigned rm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Rm, Rn_BANK
#define DEF_ASM_RM_RN_BANK(op, lit)                                     \
    static inline void                                                  \
    sh4_asm_##op##_rm_rn_bank(asm_emit_handler_func em,                 \
                              unsigned rm, unsigned rn_bank) {          \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, bank_reg_str(rn_bank));                            \
    }

// OP @Rm+, Rn_BANK
#define DEF_ASM_ARMP_RN_BANK(op, lit)                                   \
    static inline void                                                  \
    sh4_asm_##op##_armp_rn_bank(asm_emit_handler_func em,               \
                                unsigned rm, unsigned rn_bank) {        \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "+, ");                                            \
        emit_str(em, bank_reg_str(rn_bank));                            \
    }

// OP Rm_BANK, Rn
#define DEF_ASM_RM_BANK_RN(op, lit)                                   \
    static inline void                                                \
    sh4_asm_##op##_rm_bank_rn(asm_emit_handler_func em,               \
                              unsigned rm_bank, unsigned rn) {        \
        emit_str(em, lit " ");                                        \
        emit_str(em, bank_reg_str(rm_bank));                          \
        emit_str(em, ", ");                                           \
        emit_str(em, gen_reg_str(rn));                                \
    }

// OP Rm_BANK, @-Rn
#define DEF_ASM_RM_BANK_AMRN(op, lit)                                   \
    static inline void                                                  \
    sh4_asm_##op##_rm_bank_amrn(asm_emit_handler_func em,               \
                                unsigned rm_bank, unsigned rn) {        \
        emit_str(em, lit " ");                                          \
        emit_str(em, bank_reg_str(rm_bank));                            \
        emit_str(em, ", @-");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Rm, @(REG, Rn)
#define DEF_ASM_RM_A_REG_RN(op, lit, reg)                               \
    static inline void                                                  \
    sh4_asm_##op##_rm_a_##reg##_rn(asm_emit_handler_func em,            \
                                   unsigned rm, unsigned rn) {          \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", @(" #reg", ");                                  \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP @(REG, Rm), Rn
#define DEF_ASM_A_REG_RM_RN(op, lit, reg)                        \
    static inline void                                           \
    sh4_asm_##op##_a_##reg##_rm_rn(asm_emit_handler_func em,     \
                                   unsigned rm, unsigned rn) {   \
        emit_str(em, lit " @(" #reg ", ");                       \
        emit_str(em, gen_reg_str(rm));                           \
        emit_str(em, "), ");                                     \
        emit_str(em, gen_reg_str(rn));                           \
    }

// OP Rm, @Rn
#define DEF_ASM_RM_ARN(op, lit)                                         \
    static inline void sh4_asm_##op##_rm_arn(asm_emit_handler_func em,  \
                                             unsigned rm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", @");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP @Rm, Rn
#define DEF_ASM_ARM_RN(op, lit)                                         \
    static inline void sh4_asm_##op##_arm_rn(asm_emit_handler_func em,  \
                                             unsigned rm, unsigned rn) { \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Rm, @-Rn
#define DEF_ASM_RM_AMRN(op, lit)                                        \
    static inline void sh4_asm_##op##_rm_amrn(asm_emit_handler_func em, \
                                              unsigned rm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", @-");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP @Rm+, Rn
#define DEF_ASM_ARMP_RN(op, lit)                                        \
    static inline void sh4_asm_##op##_armp_rn(asm_emit_handler_func em, \
                                              unsigned rm, unsigned rn) { \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "+, ");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP @Rm+, @Rn+
#define DEF_ASM_ARMP_ARNP(op, lit)                                      \
    static inline void sh4_asm_##op##_armp_arnp(asm_emit_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "+, @");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, "+");                                              \
    }

// OP FRm, FRn
#define DEF_ASM_FRM_FRN(op, lit)                                        \
    static inline void sh4_asm_##op##_frm_frn(asm_emit_handler_func em, \
                                              unsigned frm, unsigned frn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP @Rm, FRn
#define DEF_ASM_ARM_FRN(op, lit)                                        \
    static inline void sh4_asm_##op##_arm_frn(asm_emit_handler_func em, \
                                              unsigned rm, unsigned frn) { \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP @(REG, Rm), FRn
#define DEF_ASM_A_REG_RM_FRN(op, lit, reg)                              \
    static inline void sh4_asm_##op##_a_##reg##_rm_frn(asm_emit_handler_func em, \
                                                       unsigned rm, unsigned frn) { \
        emit_str(em, lit " @(" #reg ", ");                              \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "), ");                                            \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP @Rm+, FRn
#define DEF_ASM_ARMP_FRN(op, lit)                                       \
    static inline void sh4_asm_##op##_armp_frn(asm_emit_handler_func em, \
                                               unsigned rm, unsigned frn) { \
        emit_str(em, lit " @");                                         \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "+, ");                                            \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP FRm, @Rn
#define DEF_ASM_FRM_ARN(op, lit)                                        \
    static inline void sh4_asm_##op##_frm_arn(asm_emit_handler_func em, \
                                              unsigned frm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", @");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP FRm, @-Rn
#define DEF_ASM_FRM_AMRN(op, lit)                                       \
    static inline void sh4_asm_##op##_frm_amrn(asm_emit_handler_func em, \
                                               unsigned frm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", @-");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP FRm, @(REG, Rn)
#define DEF_ASM_FRM_A_REG_RN(op, lit, reg)                              \
    static inline void sh4_asm_##op##_frm_a_##reg##_rn(asm_emit_handler_func em, \
                                                       unsigned frm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", @(" #reg ", ");                                 \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP REG, FRm, Frn
#define DEF_ASM_REG_FRM_FRN(op, lit, reg)                               \
    static inline void                                                  \
    sh4_asm_##op##_##reg##_frm_frn(asm_emit_handler_func em,            \
                                   unsigned frm, unsigned frn) {        \
        emit_str(em, lit " " #reg ", ");                                \
        emit_str(em, fr_reg_str(frm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, fr_reg_str(frn));                                  \
    }

// OP REG, @(disp4, Rn)
#define DEF_ASM_REG_A_DISP4_RN(op, lit, reg, disp_shift)                \
    static inline void                                                  \
    sh4_asm_##op##_##reg##_a_disp4_rn(asm_emit_handler_func em,         \
                                      unsigned disp4, unsigned rn) {    \
        emit_str(em, lit " " #reg ", @(");                              \
        emit_str(em, disp4_str(disp4, disp_shift));                     \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP @(disp4, Rm), REG
#define DEF_ASM_A_DISP4_RM_REG(op, lit, reg, disp_shift)                \
    static inline void                                                  \
    sh4_asm_##op##_a_disp4_rm_##reg(asm_emit_handler_func em,           \
                                    unsigned disp4, unsigned rm) {      \
        emit_str(em, lit " @(");                                        \
        emit_str(em, disp4_str(disp4, disp_shift));                     \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "), " #reg);                                       \
    }

// OP rm, @(disp4, rn)
#define DEF_ASM_RM_A_DISP4_RN(op, lit, disp_shift)                      \
    static inline void                                                  \
    sh4_asm_##op##_rm_a_disp4_rn(asm_emit_handler_func em,              \
                                 unsigned rm, unsigned disp4, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, ", @(");                                           \
        emit_str(em, disp4_str(disp4, disp_shift));                     \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP @(disp4, rm), rn
#define DEF_ASM_A_DISP4_RM_RN(op, lit, disp_shift)                      \
    static inline void                                                  \
    sh4_asm_##op##_a_disp4_rm_rn(asm_emit_handler_func em,              \
                                 unsigned disp4, unsigned rm, unsigned rn) { \
        emit_str(em, lit " @(");                                        \
        emit_str(em, disp4_str(disp4, disp_shift));                     \
        emit_str(em, ", ");                                             \
        emit_str(em, gen_reg_str(rm));                                  \
        emit_str(em, "), ");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP DRm, DRn
#define DEF_ASM_DRM_DRN(op, lit)                                        \
    static inline void sh4_asm_##op##_drm_drn(asm_emit_handler_func em, \
                                              unsigned drm, unsigned drn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, dr_reg_str(drn));                                  \
    }

// OP DRm, XDn
#define DEF_ASM_DRM_XDN(op, lit)                                        \
    static inline void sh4_asm_##op##_drm_xdn(asm_emit_handler_func em, \
                                              unsigned drm, unsigned xdn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, xd_reg_str(xdn));                                  \
    }

// OP XDm, DRn
#define DEF_ASM_XDM_DRN(op, lit)                                        \
    static inline void sh4_asm_##op##_xdm_drn(asm_emit_handler_func em, \
                                              unsigned xdm, unsigned drn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, xd_reg_str(xdm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, dr_reg_str(drn));                                  \
    }

// OP XDm, XDn
#define DEF_ASM_XDM_XDN(op, lit)                                        \
    static inline void sh4_asm_##op##_xdm_xdn(asm_emit_handler_func em, \
                                              unsigned xdm, unsigned xdn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, xd_reg_str(xdm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, xd_reg_str(xdn));                                  \
    }

// OP DRm, @Rn
#define DEF_ASM_DRM_ARN(op, lit)                                        \
    static inline void sh4_asm_##op##_drm_arn(asm_emit_handler_func em, \
                                              unsigned drm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", @");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP DRm, @-Rn
#define DEF_ASM_DRM_AMRN(op, lit)                                       \
    static inline void sh4_asm_##op##_drm_amrn(asm_emit_handler_func em, \
                                               unsigned drm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", @-");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP DRm, @(REG, Rn)
#define DEF_ASM_DRM_A_REG_RN(op, lit, reg)                              \
    static inline void                                                  \
    sh4_asm_##op##_drm_a_##reg##_rn(asm_emit_handler_func em,           \
                                    unsigned drm, unsigned rn) {        \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", @(" #reg ", ");                                 \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP Xdm, @Rn
#define DEF_ASM_XDM_ARN(op, lit)                                        \
    static inline void sh4_asm_##op##_xdm_arn(asm_emit_handler_func em, \
                                              unsigned xdm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, xd_reg_str(xdm));                                  \
        emit_str(em, ", @");                                            \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Xdm, @-Rn
#define DEF_ASM_XDM_AMRN(op, lit)                                       \
    static inline void sh4_asm_##op##_xdm_amrn(asm_emit_handler_func em, \
                                               unsigned xdm, unsigned rn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, xd_reg_str(xdm));                                  \
        emit_str(em, ", @-");                                           \
        emit_str(em, gen_reg_str(rn));                                  \
    }

// OP Xdm, @(REG, Rn)
#define DEF_ASM_XDM_A_REG_RN(op, lit, reg)                              \
    static inline void                                                  \
    sh4_asm_##op##_xdm_a_##reg##_rn(asm_emit_handler_func em,           \
                                    unsigned xdm, unsigned rn) {        \
        emit_str(em, lit " ");                                          \
        emit_str(em, xd_reg_str(xdm));                                  \
        emit_str(em, ", @(" #reg ", ");                                 \
        emit_str(em, gen_reg_str(rn));                                  \
        emit_str(em, ")");                                              \
    }

// OP DRn
#define DEF_ASM_DRN(op, lit)                                        \
    static inline void sh4_asm_##op##_drn(asm_emit_handler_func em, \
                                          unsigned drn) {           \
        emit_str(em, lit " ");                                      \
        emit_str(em, dr_reg_str(drn));                              \
    }

// OP DRm, REG
#define DEF_ASM_DRM_REG(op, lit, reg)                                   \
    static inline void sh4_asm_##op##_drm_##reg(asm_emit_handler_func em, \
                                                unsigned drm) {         \
        emit_str(em, lit " ");                                          \
        emit_str(em, dr_reg_str(drm));                                  \
        emit_str(em, ", " #reg);                                        \
    }

// OP REG, DRn
#define DEF_ASM_REG_DRN(op, lit, reg)                                   \
    static inline void sh4_asm_##op##_##reg##_drn(asm_emit_handler_func em, \
                                                  unsigned drn) {       \
        emit_str(em, lit " " #reg ", ");                                \
        emit_str(em, dr_reg_str(drn));                                  \
    }

// OP FVm, FVn
#define DEF_ASM_FVM_FVN(op, lit)                                        \
    static inline void sh4_asm_##op##_fvm_fvn(asm_emit_handler_func em, \
                                              unsigned fvm, unsigned fvn) { \
        emit_str(em, lit " ");                                          \
        emit_str(em, fv_reg_str(fvm));                                  \
        emit_str(em, ", ");                                             \
        emit_str(em, fv_reg_str(fvn));                                  \
    }

// OP REG, FVn
#define DEF_ASM_REG_FVN(op, lit, reg)                           \
    static inline void                                          \
    sh4_asm_##op##_##reg##_fvn(asm_emit_handler_func em,        \
                               unsigned fvn) {                  \
        emit_str(em, lit " " #reg ", ");                        \
        emit_str(em, fv_reg_str(fvn));                          \
    }

DEF_ASM_NOARG(div0u, "div0u")
DEF_ASM_NOARG(rts, "rts")
DEF_ASM_NOARG(clrmac, "clrmac")
DEF_ASM_NOARG(clrs, "clrs")
DEF_ASM_NOARG(clrt, "clrt")
DEF_ASM_NOARG(ldtlb, "ldtlb")
DEF_ASM_NOARG(nop, "nop")
DEF_ASM_NOARG(rte, "rte")
DEF_ASM_NOARG(sets, "sets")
DEF_ASM_NOARG(sett, "sett")
DEF_ASM_NOARG(sleep, "sleep")
DEF_ASM_NOARG(frchg, "frchg")
DEF_ASM_NOARG(fschg, "fschg")

DEF_ASM_RN(movt, "movt")
DEF_ASM_RN(cmppz, "cmp/pz")
DEF_ASM_RN(cmppl, "cmp/pl")
DEF_ASM_RN(dt, "dt")
DEF_ASM_RN(rotl, "rotl")
DEF_ASM_RN(rotr, "rotr")
DEF_ASM_RN(rotcl, "rotcl")
DEF_ASM_RN(rotcr, "rotcr")
DEF_ASM_RN(shal, "shal")
DEF_ASM_RN(shar, "shar")
DEF_ASM_RN(shll, "shll")
DEF_ASM_RN(shlr, "shlr")
DEF_ASM_RN(shll2, "shll2")
DEF_ASM_RN(shlr2, "shlr2")
DEF_ASM_RN(shll8, "shll8")
DEF_ASM_RN(shlr8, "shlr8")
DEF_ASM_RN(shll16, "shll16")
DEF_ASM_RN(shlr16, "shlr16")
DEF_ASM_RN(braf, "braf")
DEF_ASM_RN(bsrf, "bsrf")

DEF_ASM_ARN(tasb, "tas.b")
DEF_ASM_ARN(ocbi, "ocbi")
DEF_ASM_ARN(ocbp, "ocbp")
DEF_ASM_ARN(ocbwb, "ocbwb")
DEF_ASM_ARN(pref, "pref")
DEF_ASM_ARN(jmp, "jmp")
DEF_ASM_ARN(jsr, "jsr")

DEF_ASM_RM_REG(ldc, "ldc", sr)
DEF_ASM_RM_REG(ldc, "ldc", gbr)
DEF_ASM_RM_REG(ldc, "ldc", vbr)
DEF_ASM_RM_REG(ldc, "ldc", ssr)
DEF_ASM_RM_REG(ldc, "ldc", spc)
DEF_ASM_RM_REG(ldc, "ldc", dbr)
DEF_ASM_RM_REG(lds, "lds", mach)
DEF_ASM_RM_REG(lds, "lds", macl)
DEF_ASM_RM_REG(lds, "lds", pr)
DEF_ASM_RM_REG(lds, "lds", fpscr)
DEF_ASM_RM_REG(lds, "lds", fpul)

DEF_ASM_REG_RN(stc, "stc", sr)
DEF_ASM_REG_RN(stc, "stc", gbr)
DEF_ASM_REG_RN(stc, "stc", vbr)
DEF_ASM_REG_RN(stc, "stc", ssr)
DEF_ASM_REG_RN(stc, "stc", spc)
DEF_ASM_REG_RN(stc, "stc", sgr)
DEF_ASM_REG_RN(stc, "stc", dbr)
DEF_ASM_REG_RN(sts, "sts", mach)
DEF_ASM_REG_RN(sts, "sts", macl)
DEF_ASM_REG_RN(sts, "sts", pr)
DEF_ASM_REG_RN(sts, "sts", fpscr)
DEF_ASM_REG_RN(sts, "sts", fpul)

DEF_ASM_ARMP_REG(ldcl, "ldc.l", sr)
DEF_ASM_ARMP_REG(ldcl, "ldc.l", gbr)
DEF_ASM_ARMP_REG(ldcl, "ldc.l", vbr)
DEF_ASM_ARMP_REG(ldcl, "ldc.l", ssr)
DEF_ASM_ARMP_REG(ldcl, "ldc.l", spc)
DEF_ASM_ARMP_REG(ldcl, "ldc.l", dbr)
DEF_ASM_ARMP_REG(ldsl, "lds.l", mach)
DEF_ASM_ARMP_REG(ldsl, "lds.l", macl)
DEF_ASM_ARMP_REG(ldsl, "lds.l", pr)
DEF_ASM_ARMP_REG(ldsl, "lds.l", fpscr)
DEF_ASM_ARMP_REG(ldsl, "lds.l", fpul)

DEF_ASM_REG_AMRN(stcl, "stc.l", sr)
DEF_ASM_REG_AMRN(stcl, "stc.l", gbr)
DEF_ASM_REG_AMRN(stcl, "stc.l", vbr)
DEF_ASM_REG_AMRN(stcl, "stc.l", ssr)
DEF_ASM_REG_AMRN(stcl, "stc.l", spc)
DEF_ASM_REG_AMRN(stcl, "stc.l", sgr)
DEF_ASM_REG_AMRN(stcl, "stc.l", dbr)
DEF_ASM_REG_AMRN(stsl, "sts.l", mach)
DEF_ASM_REG_AMRN(stsl, "sts.l", macl)
DEF_ASM_REG_AMRN(stsl, "sts.l", pr)
DEF_ASM_REG_AMRN(stsl, "sts.l", fpscr)
DEF_ASM_REG_AMRN(stsl, "sts.l", fpul)

DEF_ASM_REG_ARN(movcal, "movca.l", r0)

DEF_ASM_FRN(fldi0, "fldi0")
DEF_ASM_FRN(fldi1, "fldi1")
DEF_ASM_FRN(fabs, "fabs")
DEF_ASM_FRN(fneg, "fneg")
DEF_ASM_FRN(fsqrt, "fsqrt")
DEF_ASM_FRN(fsrra, "fsrra")

DEF_ASM_FRM_REG(flds, "flds", fpul)
DEF_ASM_FRM_REG(ftrc, "ftrc", fpul)

DEF_ASM_REG_FRN(fsts, "fsts", fpul)
DEF_ASM_REG_FRN(float, "float", fpul)

DEF_ASM_IMM8_REG(cmpeq, "cmp/eq", r0, 0)
DEF_ASM_IMM8_REG(and, "and", r0, 0)
DEF_ASM_IMM8_REG(or, "or", r0, 0)
DEF_ASM_IMM8_REG(tst, "tst", r0, 0)
DEF_ASM_IMM8_REG(xor, "xor", r0, 0)

DEF_ASM_IMM8_A_REG_REG(andb, "and.b", r0, gbr, 0)
DEF_ASM_IMM8_A_REG_REG(orb, "or.b", r0, gbr, 0)
DEF_ASM_IMM8_A_REG_REG(tstb, "tst.b", r0, gbr, 0)
DEF_ASM_IMM8_A_REG_REG(xorb, "xor.b", r0, gbr, 0)

DEF_ASM_DISP8(bf, "bf", 1)
DEF_ASM_DISP8(bfs, "bf/s", 1)
DEF_ASM_DISP8(bt, "bt", 1)
DEF_ASM_DISP8(bts, "bt/s", 1)

DEF_ASM_IMM8(trapa, "trapa", 0)

DEF_ASM_REG_A_DISP8_REG(movb, "mov.b", r0, gbr, 0)
DEF_ASM_REG_A_DISP8_REG(movw, "mov.w", r0, gbr, 1)
DEF_ASM_REG_A_DISP8_REG(movl, "mov.l", r0, gbr, 2)

DEF_ASM_A_DISP8_REG1_REG2(movb, "mov.b", gbr, r0, 0)
DEF_ASM_A_DISP8_REG1_REG2(movw, "mov.w", gbr, r0, 1)
DEF_ASM_A_DISP8_REG1_REG2(movl, "mov.l", gbr, r0, 2)
DEF_ASM_A_DISP8_REG1_REG2(mova, "mova", pc, r0, 2)

DEF_ASM_OFFS12(bra, "bra", 1)
DEF_ASM_OFFS12(bsr, "bsr", 1)

DEF_ASM_IMM8_RN(mov, "mov", 0)
DEF_ASM_IMM8_RN(add, "add", 0)

DEF_ASM_A_DISP8_REG_RN(movw, "mov.w", pc, 1)
DEF_ASM_A_DISP8_REG_RN(movl, "mov.l", pc, 2)

DEF_ASM_RM_RN(mov, "mov")
DEF_ASM_RM_RN(swapb, "swap.b")
DEF_ASM_RM_RN(swapw, "swap.w")
DEF_ASM_RM_RN(xtrct, "xtrct")
DEF_ASM_RM_RN(add, "add")
DEF_ASM_RM_RN(addc, "addc")
DEF_ASM_RM_RN(addv, "addv")
DEF_ASM_RM_RN(cmpeq, "cmp/eq")
DEF_ASM_RM_RN(cmphs, "cmp/hs")
DEF_ASM_RM_RN(cmpge, "cmp/ge")
DEF_ASM_RM_RN(cmphi, "cmp/hi")
DEF_ASM_RM_RN(cmpgt, "cmp/gt")
DEF_ASM_RM_RN(cmpstr, "cmp/str")
DEF_ASM_RM_RN(div1, "div1")
DEF_ASM_RM_RN(div0s, "div0s")
DEF_ASM_RM_RN(dmulsl, "dmuls.l")
DEF_ASM_RM_RN(dmulul, "dmulu.l")
DEF_ASM_RM_RN(extsb, "exts.b")
DEF_ASM_RM_RN(extsw, "exts.w")
DEF_ASM_RM_RN(extub, "extu.b")
DEF_ASM_RM_RN(extuw, "extu.w")
DEF_ASM_RM_RN(mull, "mul.l")
DEF_ASM_RM_RN(mulsw, "muls.w")
DEF_ASM_RM_RN(muluw, "mulu.w")
DEF_ASM_RM_RN(neg, "neg")
DEF_ASM_RM_RN(negc, "negc")
DEF_ASM_RM_RN(sub, "sub")
DEF_ASM_RM_RN(subc, "subc")
DEF_ASM_RM_RN(subv, "subv")
DEF_ASM_RM_RN(and, "and")
DEF_ASM_RM_RN(not, "not")
DEF_ASM_RM_RN(or, "or")
DEF_ASM_RM_RN(tst, "tst")
DEF_ASM_RM_RN(xor, "xor")
DEF_ASM_RM_RN(shad, "shad")
DEF_ASM_RM_RN(shld, "shld")

DEF_ASM_RM_RN_BANK(ldc, "ldc")

DEF_ASM_ARMP_RN_BANK(ldcl, "ldc.l")

DEF_ASM_RM_BANK_RN(stc, "stc")

DEF_ASM_RM_BANK_AMRN(stcl, "stc.l")

DEF_ASM_RM_A_REG_RN(movb, "mov.b", r0)
DEF_ASM_RM_A_REG_RN(movw, "mov.w", r0)
DEF_ASM_RM_A_REG_RN(movl, "mov.l", r0)

DEF_ASM_A_REG_RM_RN(movb, "mov.b", r0)
DEF_ASM_A_REG_RM_RN(movw, "mov.w", r0)
DEF_ASM_A_REG_RM_RN(movl, "mov.l", r0)

DEF_ASM_RM_ARN(movb, "mov.b")
DEF_ASM_RM_ARN(movw, "mov.w")
DEF_ASM_RM_ARN(movl, "mov.l")

DEF_ASM_ARM_RN(movb, "mov.b")
DEF_ASM_ARM_RN(movw, "mov.w")
DEF_ASM_ARM_RN(movl, "mov.l")

DEF_ASM_RM_AMRN(movb, "mov.b")
DEF_ASM_RM_AMRN(movw, "mov.w")
DEF_ASM_RM_AMRN(movl, "mov.l")

DEF_ASM_ARMP_RN(movb, "mov.b")
DEF_ASM_ARMP_RN(movw, "mov.w")
DEF_ASM_ARMP_RN(movl, "mov.l")

DEF_ASM_ARMP_ARNP(macl, "mac.l")
DEF_ASM_ARMP_ARNP(macw, "mac.w")

DEF_ASM_FRM_FRN(fmov, "fmov")
DEF_ASM_FRM_FRN(fadd, "fadd")
DEF_ASM_FRM_FRN(fcmpeq, "fcmp/eq")
DEF_ASM_FRM_FRN(fcmpgt, "fcmp/gt")
DEF_ASM_FRM_FRN(fdiv, "fdiv")
DEF_ASM_FRM_FRN(fmul, "fmul")
DEF_ASM_FRM_FRN(fsub, "fsub")

DEF_ASM_REG_FRM_FRN(fmac, "fmac", fr0)

DEF_ASM_ARM_FRN(fmovs, "fmov.s")

DEF_ASM_A_REG_RM_FRN(fmovs, "fmov.s", r0)

DEF_ASM_ARMP_FRN(fmovs, "fmov.s")

DEF_ASM_FRM_ARN(fmovs, "fmov.s")

DEF_ASM_FRM_AMRN(fmovs, "fmov.s")

DEF_ASM_FRM_A_REG_RN(fmovs, "fmov.s", r0)

DEF_ASM_REG_A_DISP4_RN(movb, "mov.b", r0, 0)
DEF_ASM_REG_A_DISP4_RN(movw, "mov.w", r0, 1)

DEF_ASM_A_DISP4_RM_REG(movb, "mov.b", r0, 0)
DEF_ASM_A_DISP4_RM_REG(movw, "mov.w", r0, 1)

DEF_ASM_RM_A_DISP4_RN(movl, "mov.l", 2)

DEF_ASM_A_DISP4_RM_RN(movl, "mov.l", 2)

DEF_ASM_DRM_DRN(fmov, "fmov")
DEF_ASM_DRM_DRN(fadd, "fadd")
DEF_ASM_DRM_DRN(fcmpeq, "fcmp/eq")
DEF_ASM_DRM_DRN(fcmpgt, "fcmp/gt")
DEF_ASM_DRM_DRN(fdiv, "fdiv")
DEF_ASM_DRM_DRN(fmul, "fmul")
DEF_ASM_DRM_DRN(fsub, "fsub")

DEF_ASM_DRM_XDN(fmov, "fmov")

DEF_ASM_XDM_DRN(fmov, "fmov")

DEF_ASM_XDM_XDN(fmov, "fmov")

DEF_ASM_DRM_ARN(fmov, "fmov")

DEF_ASM_DRM_AMRN(fmov, "fmov")

DEF_ASM_DRM_A_REG_RN(fmov, "fmov", r0)

DEF_ASM_XDM_ARN(fmov, "fmov")

DEF_ASM_XDM_AMRN(fmov, "fmov")

DEF_ASM_XDM_A_REG_RN(fmov, "fmov", r0)

DEF_ASM_DRN(fabs, "fabs")
DEF_ASM_DRN(fneg, "fneg")
DEF_ASM_DRN(fsqrt, "fsqrt")

DEF_ASM_DRM_REG(fcnvds, "fcnvds", fpul)
DEF_ASM_DRM_REG(ftrc, "ftrc", fpul)

DEF_ASM_REG_DRN(fcnvsd, "fcnvsd", fpul)
DEF_ASM_REG_DRN(float, "float", fpul)
DEF_ASM_REG_DRN(fsca, "fsca", fpul)

DEF_ASM_FVM_FVN(fipr, "fipr")

DEF_ASM_REG_FVN(ftrv, "ftrv", xmtrx)

#endif
