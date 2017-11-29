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

#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "parser.h"

#define MAX_TOKENS 32
struct tok tokens[MAX_TOKENS];
unsigned n_tokens;

#define CHECK_(cond, func, line, file) do_check((cond), #cond, func, line, file)
#define CHECK(cond) CHECK_((cond), __func__, __LINE__, __FILE__)

static void do_check(bool success, char const *cond, char const *func,
                     unsigned line, char const *file) {
    if (!success)
        errx(1, "%s - sanity check failed: \"%s\" (line %d of %s)\n",
             func, cond, line, file);
}

#define CHECK_RN_(tok_idx, func, line, file)                    \
    CHECK_(tokens[(tok_idx)].tp == TOK_RN, func, line, file)
#define CHECK_RN(tok_idx) CHECK_RN_(tok_idx, __func__, __LINE__, __FILE__)
#define CHECK_RN_BANK(tok_idx) CHECK_(tokens[(tok_idx)].tp == TOK_RN_BANK, \
                                      __func__, __LINE__, __FILE__)
#define CHECK_FRN(tok_idx) CHECK_FRN_(tok_idx, __func__, __LINE__, __FILE__)
#define CHECK_FRN_(tok_idx, func, line, file)                           \
    CHECK_(tokens[(tok_idx)].tp == TOK_FRN, func, line, file)
#define CHECK_DRN(tok_idx) CHECK_DRN_((tok_idx), __func__, __LINE__, __FILE__)
#define CHECK_DRN_(tok_idx, func, line, file) do_check_drn((tok_idx), func, line, file)
#define CHECK_XDN(tok_idx) do_check_xdn((tok_idx), __LINE__, __FILE__)
#define CHECK_FVN(tok_idx) do_check_fvn((tok_idx), __LINE__, __FILE__)
#define CHECK_IMM(tok_idx) CHECK_(tokens[(tok_idx)].tp == TOK_IMM,  \
                                  __func__, __LINE__, __FILE__)
#define CHECK_DISP(tok_idx) CHECK_(tokens[(tok_idx)].tp == TOK_DISP,    \
                                   __func__, __LINE__, __FILE__)

#define CHECK_R0(tok_idx) do_check_r0((tok_idx), __func__, __LINE__, __FILE__)
#define CHECK_FR0(tok_idx) do_check_fr0((tok_idx), __func__, __LINE__, __FILE__)

static void do_check_r0(unsigned tok_idx, char const *func,
                        unsigned line, char const *file) {
    CHECK_RN_(tok_idx, func, line, file);
    CHECK_(tokens[tok_idx].val.reg_idx == 0, func, line, file);
}

static void do_check_fr0(unsigned tok_idx, char const *func,
                         unsigned line, char const *file) {
    CHECK_FRN_(tok_idx, func, line, file);
    CHECK_(tokens[tok_idx].val.reg_idx == 0, func, line, file);
}

static void do_check_drn(unsigned tok_idx, char const *func,
                         unsigned line, char const *file) {
    CHECK_(tokens[tok_idx].tp == TOK_DRN, func, line, file);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    CHECK_(reg_idx < 16, func, line, file);
    CHECK_(!(reg_idx & 1), func, line, file);
}

static void do_check_xdn(unsigned tok_idx, unsigned line, char const *file) {
    CHECK(tokens[tok_idx].tp == TOK_XDN);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    if (reg_idx >= 16)
        errx(1, "invalid out-of-range banked double-precision floating-point "
             "register index %u (see line %d of %s)", reg_idx, line, file);
    if (reg_idx & 1)
        errx(1, "invalid non-even banked double-precision floating-point "
             "register index %u (see line %d of %s)", reg_idx, line, file);
}

static void do_check_fvn(unsigned tok_idx, unsigned line, char const *file) {
    CHECK(tokens[tok_idx].tp == TOK_FVN);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    if (reg_idx >= 16)
        errx(1, "invalid out-of-range double-precision floating-point register "
             "index %u (see line %d of %s)", reg_idx, line, file);
    if (reg_idx & 3)
        errx(1, "invalid non-even double-precision floating-point register "
             "index %u (see line %d of %s)", reg_idx, line, file);
}

static emit_bin_handler_func emit;

typedef void(*parser_emit_func)(void);

#define OP_NOARG(op)                            \
    static void emit_##op(void) {               \
        sh4_bin_##op(emit);                     \
    }

OP_NOARG(div0u)
OP_NOARG(rts)
OP_NOARG(clrmac)
OP_NOARG(clrs)
OP_NOARG(clrt)
OP_NOARG(ldtlb)
OP_NOARG(nop)
OP_NOARG(rte)
OP_NOARG(sets)
OP_NOARG(sett)
OP_NOARG(sleep)
OP_NOARG(frchg)
OP_NOARG(fschg)

#define OP_RN(op)                               \
    static void emit_##op##_rn(void) {          \
        CHECK_RN(1);                            \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4_bin_##op##_rn(emit, reg_no);        \
    }

OP_RN(movt)
OP_RN(cmppz)
OP_RN(cmppl)
OP_RN(dt)
OP_RN(rotl)
OP_RN(rotr)
OP_RN(rotcl)
OP_RN(rotcr)
OP_RN(shal)
OP_RN(shar)
OP_RN(shll)
OP_RN(shlr)
OP_RN(shll2)
OP_RN(shlr2)
OP_RN(shll8)
OP_RN(shlr8)
OP_RN(shll16)
OP_RN(shlr16)
OP_RN(braf)
OP_RN(bsrf)

#define OP_ARN(op)                              \
    static void emit_##op##_arn(void) {         \
        CHECK_RN(2);                            \
        int reg_no = tokens[2].val.reg_idx;     \
        sh4_bin_##op##_arn(emit, reg_no);       \
    }

OP_ARN(tasb)
OP_ARN(ocbi)
OP_ARN(ocbp)
OP_ARN(ocbwb)
OP_ARN(pref)
OP_ARN(jmp)
OP_ARN(jsr)

#define OP_RM_REG(op, reg)                      \
    static void emit_##op##_rm_##reg(void) {    \
        CHECK_RN(1);                            \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4_bin_##op##_rm_##reg(emit, reg_no);  \
    }

OP_RM_REG(ldc, sr)
OP_RM_REG(ldc, gbr)
OP_RM_REG(ldc, vbr)
OP_RM_REG(ldc, ssr)
OP_RM_REG(ldc, spc)
OP_RM_REG(ldc, dbr)
OP_RM_REG(lds, mach)
OP_RM_REG(lds, macl)
OP_RM_REG(lds, pr)
OP_RM_REG(lds, fpscr)
OP_RM_REG(lds, fpul)

#define OP_REG_RN(op, reg)                          \
    static void emit_##op##_##reg##_rn(void) {      \
        CHECK_RN(3);                                \
        int reg_no = tokens[3].val.reg_idx;         \
        sh4_bin_##op##_##reg##_rn(emit, reg_no);    \
    }

OP_REG_RN(stc, sr)
OP_REG_RN(stc, gbr)
OP_REG_RN(stc, vbr)
OP_REG_RN(stc, ssr)
OP_REG_RN(stc, spc)
OP_REG_RN(stc, sgr)
OP_REG_RN(stc, dbr)
OP_REG_RN(sts, mach)
OP_REG_RN(sts, macl)
OP_REG_RN(sts, pr)
OP_REG_RN(sts, fpscr)
OP_REG_RN(sts, fpul)

#define OP_ARMP_REG(op, reg)                        \
    static void emit_##op##_armp_##reg(void) {      \
        CHECK_RN(2);                                \
        int reg_no = tokens[2].val.reg_idx;         \
        sh4_bin_##op##_armp_##reg(emit, reg_no);    \
    }

OP_ARMP_REG(ldcl, sr)
OP_ARMP_REG(ldcl, gbr)
OP_ARMP_REG(ldcl, vbr)
OP_ARMP_REG(ldcl, ssr)
OP_ARMP_REG(ldcl, spc)
OP_ARMP_REG(ldcl, dbr)
OP_ARMP_REG(ldsl, mach)
OP_ARMP_REG(ldsl, macl)
OP_ARMP_REG(ldsl, pr)
OP_ARMP_REG(ldsl, fpscr)
OP_ARMP_REG(ldsl, fpul)

#define OP_REG_AMRN(op, reg)                        \
    static void emit_##op##_##reg##_amrn(void)  {   \
        CHECK_RN(5);                                \
        int reg_no = tokens[5].val.reg_idx;         \
        sh4_bin_##op##_##reg##_amrn(emit, reg_no);  \
    }

OP_REG_AMRN(stcl, sr)
OP_REG_AMRN(stcl, gbr)
OP_REG_AMRN(stcl, vbr)
OP_REG_AMRN(stcl, ssr)
OP_REG_AMRN(stcl, spc)
OP_REG_AMRN(stcl, sgr)
OP_REG_AMRN(stcl, dbr)
OP_REG_AMRN(stsl, mach)
OP_REG_AMRN(stsl, macl)
OP_REG_AMRN(stsl, pr)
OP_REG_AMRN(stsl, fpscr)
OP_REG_AMRN(stsl, fpul)

#define OP_R0_ARN(op)                                                   \
    static void emit_##op##_r0_arn(void) {                              \
        CHECK_RN(4);                                                    \
        CHECK_R0(1);                                                    \
        int reg_no = tokens[4].val.reg_idx;                             \
        sh4_bin_##op##_r0_arn(emit, reg_no);                            \
    }

OP_R0_ARN(movcal)

#define OP_FRN(op)                                                      \
    static void emit_##op##_frn(void) {                                 \
        CHECK_FRN(1);                                                   \
        int reg_no = tokens[1].val.reg_idx;                             \
        sh4_bin_##op##_frn(emit, reg_no);                               \
    }

OP_FRN(fldi0)
OP_FRN(fldi1)
OP_FRN(fabs)
OP_FRN(fneg)
OP_FRN(fsqrt)
OP_FRN(fsrra)

#define OP_FRM_REG(op, reg)                     \
    static void emit_##op##_frm_##reg(void) {   \
        CHECK_FRN(1);                           \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4_bin_##op##_frm_##reg(emit, reg_no); \
    }

OP_FRM_REG(flds, fpul)
OP_FRM_REG(ftrc, fpul)

#define OP_REG_FRN(op, reg)                         \
    static void emit_##op##_##reg##_frn(void) {     \
        CHECK_FRN(3);                               \
        int reg_no = tokens[3].val.reg_idx;         \
        sh4_bin_##op##_##reg##_frn(emit, reg_no);   \
    }

OP_REG_FRN(fsts, fpul)
OP_REG_FRN(float, fpul)

#define OP_IMM8_R0(op)                          \
    static void emit_##op##_imm8_r0(void) {     \
        CHECK_R0(3);                            \
        CHECK_IMM(1);                           \
        int imm_val = tokens[1].val.as_int;     \
        sh4_bin_##op##_imm8_r0(emit, imm_val);  \
    }

OP_IMM8_R0(cmpeq)
OP_IMM8_R0(and)
OP_IMM8_R0(or)
OP_IMM8_R0(tst)
OP_IMM8_R0(xor)

#define OP_IMM8_A_R0_REG(op, reg)                       \
    static void emit_##op##_imm8_a_r0_##reg(void) {     \
        CHECK_IMM(1);                                   \
        CHECK_R0(5);                                    \
        int imm_val = tokens[1].val.as_int;             \
        sh4_bin_##op##_imm8_a_r0_##reg(emit, imm_val);  \
    }

OP_IMM8_A_R0_REG(andb, gbr)
OP_IMM8_A_R0_REG(orb, gbr)
OP_IMM8_A_R0_REG(tstb, gbr)
OP_IMM8_A_R0_REG(xorb, gbr)

#define OP_OFFS8(op)                            \
    static void emit_##op##_offs8(void) {       \
        CHECK_DISP(1);                          \
        int disp_val = tokens[1].val.as_int;    \
        sh4_bin_##op##_offs8(emit, disp_val);   \
    }

OP_OFFS8(bf)
OP_OFFS8(bfs)
OP_OFFS8(bt)
OP_OFFS8(bts)

#define OP_IMM8(op)                             \
    static void emit_##op##_imm8(void) {        \
        CHECK_IMM(1);                           \
        int imm_val = tokens[1].val.as_int;     \
        sh4_bin_##op##_imm8(emit, imm_val);     \
    }

OP_IMM8(trapa);

#define OP_R0_A_DISP8_REG(op, reg)                          \
    static void emit_##op##_r0_a_disp8_##reg(void) {        \
        CHECK_R0(1);                                        \
        CHECK_DISP(5);                                      \
        int disp_val = tokens[5].val.as_int;                \
        sh4_bin_##op##_r0_a_disp8_##reg(emit, disp_val);    \
    }

OP_R0_A_DISP8_REG(movb, gbr)
OP_R0_A_DISP8_REG(movw, gbr)
OP_R0_A_DISP8_REG(movl, gbr)

#define OP_A_DISP8_REG_R0(op, reg)                          \
    static void emit_##op##_a_disp8_##reg##_r0(void) {      \
        CHECK_R0(8);                                        \
        CHECK_DISP(3);                                      \
        int disp_val = tokens[3].val.as_int;                \
        sh4_bin_##op##_a_disp8_##reg##_r0(emit, disp_val);  \
    }

OP_A_DISP8_REG_R0(movb, gbr)
OP_A_DISP8_REG_R0(movw, gbr)
OP_A_DISP8_REG_R0(movl, gbr)

#define OP_A_OFFS8_REG_R0(op, reg)              \
    static void emit_##op##_a_offs8_##reg##_r0(void) {      \
        CHECK_R0(8);                                        \
        CHECK_DISP(3);                                      \
        int disp_val = tokens[3].val.as_int;                \
        sh4_bin_##op##_a_offs8_##reg##_r0(emit, disp_val);  \
    }

OP_A_OFFS8_REG_R0(mova, pc)

#define OP_OFFS12(op)                           \
    static void emit_##op##_offs12(void) {      \
        CHECK_DISP(1);                          \
        int disp_val = tokens[1].val.as_int;    \
        sh4_bin_##op##_offs12(emit, disp_val);  \
    }

OP_OFFS12(bra)
OP_OFFS12(bsr)

#define OP_IMM8_RN(op)                                  \
    static void emit_##op##_imm8_rn(void) {             \
        CHECK_IMM(1);                                   \
        CHECK_RN(3);                                    \
        int imm_val = tokens[1].val.as_int;             \
        int reg_no = tokens[3].val.reg_idx;             \
        sh4_bin_##op##_imm8_rn(emit, imm_val, reg_no);  \
    }

OP_IMM8_RN(mov)
OP_IMM8_RN(add)

#define OP_A_OFFS8_REG_RN(op, reg)                                  \
    static void emit_##op##_a_offs8_##reg##_rn(void) {              \
        CHECK_DISP(3);                                              \
        CHECK_RN(8);                                                \
        int disp_val = tokens[3].val.as_int;                        \
        int reg_no = tokens[8].val.reg_idx;                         \
        sh4_bin_##op##_a_offs8_##reg##_rn(emit, disp_val, reg_no);  \
    }

OP_A_OFFS8_REG_RN(movw, pc)
OP_A_OFFS8_REG_RN(movl, pc)

#define OP_RM_RN(op)                                \
    static void emit_##op##_rm_rn(void) {           \
        CHECK_RN(1);                                \
        CHECK_RN(3);                                \
        int rm_no = tokens[1].val.as_int;           \
        int rn_no = tokens[3].val.as_int;           \
        sh4_bin_##op##_rm_rn(emit, rm_no, rn_no);   \
    }

OP_RM_RN(mov)
OP_RM_RN(swapb)
OP_RM_RN(swapw)
OP_RM_RN(xtrct)
OP_RM_RN(add)
OP_RM_RN(addc)
OP_RM_RN(addv)
OP_RM_RN(cmpeq)
OP_RM_RN(cmphs)
OP_RM_RN(cmpge)
OP_RM_RN(cmphi)
OP_RM_RN(cmpgt)
OP_RM_RN(cmpstr)
OP_RM_RN(div1)
OP_RM_RN(div0s)
OP_RM_RN(dmulsl)
OP_RM_RN(dmulul)
OP_RM_RN(extsb)
OP_RM_RN(extsw)
OP_RM_RN(extub)
OP_RM_RN(extuw)
OP_RM_RN(mull)
OP_RM_RN(mulsw)
OP_RM_RN(muluw)
OP_RM_RN(neg)
OP_RM_RN(negc)
OP_RM_RN(sub)
OP_RM_RN(subc)
OP_RM_RN(subv)
OP_RM_RN(and)
OP_RM_RN(not)
OP_RM_RN(or)
OP_RM_RN(tst)
OP_RM_RN(xor)
OP_RM_RN(shad)
OP_RM_RN(shld)

#define OP_RM_A_R0_RN(op)                                \
    static void emit_##op##_rm_a_r0_rn(void) {           \
        CHECK_RN(1);                                     \
        CHECK_R0(5);                                     \
        CHECK_RN(7);                                     \
        int rm_no = tokens[1].val.reg_idx;               \
        int rn_no = tokens[7].val.reg_idx;               \
        sh4_bin_##op##_rm_a_r0_rn(emit, rm_no, rn_no);   \
    }

OP_RM_A_R0_RN(movb)
OP_RM_A_R0_RN(movw)
OP_RM_A_R0_RN(movl)

#define OP_A_R0_RM_RN(op)                                \
    static void emit_##op##_a_r0_rm_rn(void) {           \
        CHECK_R0(3);                                     \
        CHECK_RN(5);                                     \
        CHECK_RN(8);                                     \
        int rm_no = tokens[5].val.reg_idx;               \
        int rn_no = tokens[8].val.reg_idx;               \
        sh4_bin_##op##_a_r0_rm_rn(emit, rm_no, rn_no);   \
    }

OP_A_R0_RM_RN(movb)
OP_A_R0_RM_RN(movw)
OP_A_R0_RM_RN(movl)

#define OP_RM_ARN(op)                                    \
    static void emit_##op##_rm_arn(void) {               \
        CHECK_RN(1);                                     \
        CHECK_RN(4);                                     \
        int rm_no = tokens[1].val.reg_idx;               \
        int rn_no = tokens[4].val.reg_idx;               \
        sh4_bin_##op##_rm_arn(emit, rm_no, rn_no);       \
    }

OP_RM_ARN(movb)
OP_RM_ARN(movw)
OP_RM_ARN(movl)

#define OP_ARM_RN(op)                                    \
    static void emit_##op##_arm_rn(void) {               \
        CHECK_RN(2);                                     \
        CHECK_RN(4);                                     \
        int rm_no = tokens[2].val.reg_idx;               \
        int rn_no = tokens[4].val.reg_idx;               \
        sh4_bin_##op##_arm_rn(emit, rm_no, rn_no);       \
    }

OP_ARM_RN(movb)
OP_ARM_RN(movw)
OP_ARM_RN(movl)

#define OP_RM_AMRN(op)                                    \
    static void emit_##op##_rm_amrn(void) {               \
        CHECK_RN(1);                                      \
        CHECK_RN(5);                                      \
        int rm_no = tokens[1].val.reg_idx;                \
        int rn_no = tokens[5].val.reg_idx;                \
        sh4_bin_##op##_rm_amrn(emit, rm_no, rn_no);       \
    }

OP_RM_AMRN(movb)
OP_RM_AMRN(movw)
OP_RM_AMRN(movl)

#define OP_ARMP_RN(op)                                    \
    static void emit_##op##_armp_rn(void) {               \
        CHECK_RN(2);                                      \
        CHECK_RN(5);                                      \
        int rm_no = tokens[2].val.reg_idx;                \
        int rn_no = tokens[5].val.reg_idx;                \
        sh4_bin_##op##_armp_rn(emit, rm_no, rn_no);       \
    }

OP_ARMP_RN(movb)
OP_ARMP_RN(movw)
OP_ARMP_RN(movl)

#define OP_ARMP_ARNP(op)                                    \
    static void emit_##op##_armp_arnp(void) {               \
        CHECK_RN(2);                                        \
        CHECK_RN(6);                                        \
        int rm_no = tokens[2].val.reg_idx;                  \
        int rn_no = tokens[6].val.reg_idx;                  \
        sh4_bin_##op##_armp_arnp(emit, rm_no, rn_no);       \
    }

OP_ARMP_ARNP(macw)
OP_ARMP_ARNP(macl)

#define OP_FRM_FRN(op)                                       \
    static void emit_##op##_frm_frn(void) {                  \
        CHECK_FRN(1);                                        \
        CHECK_FRN(3);                                        \
        int frm_no = tokens[1].val.reg_idx;                  \
        int frn_no = tokens[3].val.reg_idx;                  \
        sh4_bin_##op##_frm_frn(emit, frm_no, frn_no);        \
    }

OP_FRM_FRN(fmov)
OP_FRM_FRN(fadd)
OP_FRM_FRN(fcmpeq)
OP_FRM_FRN(fcmpgt)
OP_FRM_FRN(fdiv)
OP_FRM_FRN(fmul)
OP_FRM_FRN(fsub)

#define OP_ARM_FRN(op)                              \
    static void emit_##op##_arm_frn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_FRN(4);                                   \
        int rm_no = tokens[2].val.reg_idx;              \
        int frn_no = tokens[4].val.reg_idx;             \
        sh4_bin_##op##_arm_frn(emit, rm_no, frn_no);    \
    }

OP_ARM_FRN(fmovs)

#define OP_A_R0_RM_FRN(op)                              \
    static void emit_##op##_a_r0_rm_frn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_FRN(8);                                       \
        int rm_no = tokens[5].val.reg_idx;                  \
        int frn_no = tokens[8].val.reg_idx;                 \
        sh4_bin_##op##_a_r0_rm_frn(emit, rm_no, frn_no);    \
    }

OP_A_R0_RM_FRN(fmovs)

#define OP_ARMP_FRN(op)                             \
    static void emit_##op##_armp_frn(void) {            \
        CHECK_RN(2);                                    \
        CHECK_FRN(5);                                   \
        int rm_no = tokens[2].val.reg_idx;              \
        int frn_no = tokens[5].val.reg_idx;             \
        sh4_bin_##op##_armp_frn(emit, rm_no, frn_no);   \
    }

OP_ARMP_FRN(fmovs)

#define OP_FRM_ARN(op)                              \
    static void emit_##op##_frm_arn(void) {             \
        CHECK_FRN(1);                                   \
        CHECK_RN(4);                                    \
        int rn_no = tokens[4].val.reg_idx;              \
        int frm_no = tokens[1].val.reg_idx;             \
        sh4_bin_##op##_frm_arn(emit, frm_no, rn_no);    \
    }

OP_FRM_ARN(fmovs)

#define OP_FRM_AMRN(op)                             \
    static void emit_##op##_frm_amrn(void) {            \
        CHECK_FRN(1);                                   \
        CHECK_RN(5);                                    \
        int rn_no = tokens[5].val.reg_idx;              \
        int frm_no = tokens[1].val.reg_idx;             \
        sh4_bin_##op##_frm_amrn(emit, frm_no, rn_no);   \
    }

OP_FRM_AMRN(fmovs)

#define OP_FRM_A_R0_RN(op)                                  \
    static void emit_##op##_frm_a_r0_rn(void) {             \
        CHECK_FRN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int rn_no = tokens[7].val.reg_idx;                  \
        int frm_no = tokens[1].val.reg_idx;                 \
        sh4_bin_##op##_frm_a_r0_rn(emit, frm_no, rn_no);    \
    }

OP_FRM_A_R0_RN(fmovs)

#define OP_FR0_FRM_FRN(op)                                  \
    static void emit_##op##_fr0_frm_frn(void) {             \
        CHECK_FR0(1);                                       \
        CHECK_FRN(3);                                       \
        CHECK_FRN(5);                                       \
        int frm_no = tokens[3].val.reg_idx;                 \
        int frn_no = tokens[5].val.reg_idx;                 \
        sh4_bin_##op##_fr0_frm_frn(emit, frm_no, frn_no);   \
    }

OP_FR0_FRM_FRN(fmac)

#define OP_RM_RN_BANK(op)                                   \
    static void emit_##op##_rm_rn_bank(void) {              \
        CHECK_RN(1);                                        \
        CHECK_RN_BANK(3);                                   \
        int rm_no = tokens[1].val.reg_idx;                  \
        int rn_bank_no = tokens[3].val.reg_idx;             \
        sh4_bin_##op##_rm_rn_bank(emit, rm_no, rn_bank_no); \
    }

OP_RM_RN_BANK(ldc)

#define OP_RM_BANK_RN(op)                                   \
    static void emit_##op##_rm_bank_rn(void) {              \
        CHECK_RN_BANK(1);                                   \
        CHECK_RN(3);                                        \
        int rm_bank_no = tokens[1].val.reg_idx;             \
        int rn_no = tokens[3].val.reg_idx;                  \
        sh4_bin_##op##_rm_bank_rn(emit, rm_bank_no, rn_no); \
    }

OP_RM_BANK_RN(stc)

#define OP_ARMP_RN_BANK(op)                                     \
    static void emit_##op##_armp_rn_bank(void) {                \
        CHECK_RN(2);                                            \
        CHECK_RN_BANK(5);                                       \
        int rm_no = tokens[2].val.reg_idx;                      \
        int rn_bank_no = tokens[5].val.reg_idx;                 \
        sh4_bin_##op##_armp_rn_bank(emit, rm_no, rn_bank_no);   \
    }

OP_ARMP_RN_BANK(ldcl)

#define OP_RM_BANK_AMRN(op)                                     \
    static void emit_##op##_rm_bank_amrn(void) {                \
        CHECK_RN_BANK(1);                                       \
        CHECK_RN(5);                                            \
        int rm_bank_no = tokens[1].val.reg_idx;                 \
        int rn_no = tokens[5].val.reg_idx;                      \
        sh4_bin_##op##_rm_bank_amrn(emit, rm_bank_no, rn_no);   \
    }

OP_RM_BANK_AMRN(stcl)

#define OP_R0_A_DISP4_RN(op)                                        \
    static void emit_##op##_r0_a_disp4_rn(void) {                   \
        CHECK_R0(1);                                                \
        CHECK_DISP(5);                                              \
        CHECK_RN(7);                                                \
        int disp_val = tokens[5].val.as_int;                        \
        int reg_no = tokens[7].val.reg_idx;                         \
        sh4_bin_##op##_r0_a_disp4_rn(emit, disp_val, reg_no);       \
    }

OP_R0_A_DISP4_RN(movb)
OP_R0_A_DISP4_RN(movw)

#define OP_A_DISP4_RM_R0(op)                                    \
    static void emit_##op##_a_disp4_rm_r0(void) {               \
        CHECK_DISP(3);                                          \
        CHECK_RN(5);                                            \
        CHECK_R0(8);                                            \
        int disp_val = tokens[3].val.as_int;                    \
        int reg_no = tokens[5].val.reg_idx;                     \
        sh4_bin_##op##_a_disp4_rm_r0(emit, disp_val, reg_no);   \
    }

OP_A_DISP4_RM_R0(movb)
OP_A_DISP4_RM_R0(movw)

#define OP_RM_A_DISP4_RN(op)                                            \
    static void emit_##op##_rm_a_disp4_rn(void) {                       \
        CHECK_RN(1);                                                    \
        CHECK_DISP(5);                                                  \
        CHECK_RN(7);                                                    \
        int src_reg = tokens[1].val.reg_idx;                            \
        int disp_val = tokens[5].val.as_int;                            \
        int dst_reg = tokens[7].val.reg_idx;                            \
        sh4_bin_##op##_rm_a_disp4_rn(emit, src_reg, disp_val, dst_reg); \
    }

OP_RM_A_DISP4_RN(movl)

#define OP_A_DISP4_RM_RN(op)                                            \
    static void emit_##op##_a_disp4_rm_rn(void) {                       \
        CHECK_DISP(3);                                                  \
        CHECK_RN(5);                                                    \
        CHECK_RN(8);                                                    \
        int disp_val = tokens[3].val.as_int;                            \
        int src_reg = tokens[5].val.reg_idx;                            \
        int dst_reg = tokens[8].val.reg_idx;                            \
        sh4_bin_##op##_a_disp4_rm_rn(emit, disp_val, src_reg, dst_reg); \
    }

OP_A_DISP4_RM_RN(movl)

#define OP_DRM_DRN(op)                                  \
    static void emit_##op##_drm_drn(void) {             \
        CHECK_DRN(1);                                   \
        CHECK_DRN(3);                                   \
        int src_reg = tokens[1].val.reg_idx;            \
        int dst_reg = tokens[3].val.reg_idx;            \
        sh4_bin_##op##_drm_drn(emit, src_reg, dst_reg); \
    }

OP_DRM_DRN(fmov)
OP_DRM_DRN(fadd)
OP_DRM_DRN(fcmpeq)
OP_DRM_DRN(fcmpgt)
OP_DRM_DRN(fdiv)
OP_DRM_DRN(fmul)
OP_DRM_DRN(fsub)

#define OP_DRM_XDN(op)                                      \
    static void emit_##op##_drm_xdn(void) {                 \
        CHECK_DRN(1);                                       \
        CHECK_XDN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4_bin_##op##_drm_xdn(emit, src_reg, dst_reg);     \
    }

OP_DRM_XDN(fmov)

#define OP_XDM_DRN(op)                                      \
    static void emit_##op##_xdm_drn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_DRN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4_bin_##op##_xdm_drn(emit, src_reg, dst_reg);     \
    }

OP_XDM_DRN(fmov)

#define OP_XDM_XDN(op)                                      \
    static void emit_##op##_xdm_xdn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_XDN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4_bin_##op##_xdm_xdn(emit, src_reg, dst_reg);     \
    }

OP_XDM_XDN(fmov)

#define OP_ARM_DRN(op)                                  \
    static void emit_##op##_arm_drn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_DRN(4);                                   \
        int src_reg = tokens[2].val.reg_idx;            \
        int dst_reg = tokens[4].val.reg_idx;            \
        sh4_bin_##op##_arm_drn(emit, src_reg, dst_reg); \
    }

OP_ARM_DRN(fmov)

#define OP_A_R0_RM_DRN(op)                                  \
    static void emit_##op##_a_r0_rm_drn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_DRN(8);                                       \
        int src_reg = tokens[5].val.reg_idx;                \
        int dst_reg = tokens[8].val.reg_idx;                \
        sh4_bin_##op##_a_r0_rm_drn(emit, src_reg, dst_reg); \
    }

OP_A_R0_RM_DRN(fmov)

#define OP_ARMP_DRN(op)                                     \
    static void emit_##op##_armp_drn(void) {                \
        CHECK_RN(2);                                        \
        CHECK_DRN(5);                                       \
        int src_reg = tokens[2].val.reg_idx;                \
        int dst_reg = tokens[5].val.reg_idx;                \
        sh4_bin_##op##_armp_drn(emit, src_reg, dst_reg);    \
    }

OP_ARMP_DRN(fmov)

#define OP_ARM_XDN(op)                                  \
    static void emit_##op##_arm_xdn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_XDN(4);                                   \
        int src_reg = tokens[2].val.reg_idx;            \
        int dst_reg = tokens[4].val.reg_idx;            \
        sh4_bin_##op##_arm_xdn(emit, src_reg, dst_reg); \
    }

OP_ARM_XDN(fmov)

#define OP_ARMP_XDN(op)                                     \
    static void emit_##op##_armp_xdn(void) {                \
        CHECK_RN(2);                                        \
        CHECK_XDN(5);                                       \
        int src_reg = tokens[2].val.reg_idx;                \
        int dst_reg = tokens[5].val.reg_idx;                \
        sh4_bin_##op##_armp_xdn(emit, src_reg, dst_reg);    \
    }

OP_ARMP_XDN(fmov)

#define OP_A_R0_RM_XDN(op)                                  \
    static void emit_##op##_a_r0_rm_xdn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_XDN(8);                                       \
        int src_reg = tokens[5].val.reg_idx;                \
        int dst_reg = tokens[8].val.reg_idx;                \
        sh4_bin_##op##_a_r0_rm_xdn(emit, src_reg, dst_reg); \
    }

OP_A_R0_RM_XDN(fmov)

#define OP_DRM_ARN(op)                                      \
    static void emit_##op##_drm_arn(void) {                 \
        CHECK_DRN(1);                                       \
        CHECK_RN(4);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[4].val.reg_idx;                \
        sh4_bin_##op##_drm_arn(emit, src_reg, dst_reg);     \
    }

OP_DRM_ARN(fmov)

#define OP_DRM_AMRN(op)                                      \
    static void emit_##op##_drm_amrn(void) {                 \
        CHECK_DRN(1);                                        \
        CHECK_RN(5);                                         \
        int src_reg = tokens[1].val.reg_idx;                 \
        int dst_reg = tokens[5].val.reg_idx;                 \
        sh4_bin_##op##_drm_amrn(emit, src_reg, dst_reg);     \
    }

OP_DRM_AMRN(fmov)

#define OP_DRM_A_R0_RN(op)                                  \
    static void emit_##op##_drm_a_r0_rn(void) {             \
        CHECK_DRN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[7].val.reg_idx;                \
        sh4_bin_##op##_drm_a_r0_rn(emit, src_reg, dst_reg); \
    }

OP_DRM_A_R0_RN(fmov)

#define OP_XDM_ARN(op)                                      \
    static void emit_##op##_xdm_arn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_RN(4);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[4].val.reg_idx;                \
        sh4_bin_##op##_xdm_arn(emit, src_reg, dst_reg);     \
    }

OP_XDM_ARN(fmov)

#define OP_XDM_AMRN(op)                                      \
    static void emit_##op##_xdm_amrn(void) {                 \
        CHECK_XDN(1);                                        \
        CHECK_RN(5);                                         \
        int src_reg = tokens[1].val.reg_idx;                 \
        int dst_reg = tokens[5].val.reg_idx;                 \
        sh4_bin_##op##_xdm_amrn(emit, src_reg, dst_reg);     \
    }

OP_XDM_AMRN(fmov)

#define OP_XDM_A_R0_RN(op)                                  \
    static void emit_##op##_xdm_a_r0_rn(void) {             \
        CHECK_XDN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[7].val.reg_idx;                \
        sh4_bin_##op##_xdm_a_r0_rn(emit, src_reg, dst_reg); \
    }

OP_XDM_A_R0_RN(fmov)

#define OP_DRN(op)                              \
    static void emit_##op##_drn(void) {         \
        CHECK_DRN(1);                           \
        int src_reg = tokens[1].val.reg_idx;    \
        sh4_bin_##op##_drn(emit, src_reg);      \
    }

OP_DRN(fabs)
OP_DRN(fneg)
OP_DRN(fsqrt)

#define OP_DRM_FPUL(op)                         \
    static void emit_##op##_drm_fpul(void) {    \
        CHECK_DRN(1);                           \
        int src_reg = tokens[1].val.reg_idx;    \
        sh4_bin_##op##_drm_fpul(emit, src_reg); \
    }

OP_DRM_FPUL(fcnvds)
OP_DRM_FPUL(ftrc)

#define OP_FPUL_DRN(op)                         \
    static void emit_##op##_fpul_drn(void) {    \
        CHECK_DRN(3);                           \
        int src_reg = tokens[3].val.reg_idx;    \
        sh4_bin_##op##_fpul_drn(emit, src_reg); \
    }

OP_FPUL_DRN(fcnvsd)
OP_FPUL_DRN(float)
OP_FPUL_DRN(fsca)

#define OP_FVM_FVN(op)                                  \
    static void emit_##op##_fvm_fvn(void) {             \
        CHECK_FVN(1);                                   \
        CHECK_FVN(3);                                   \
        int src_reg = tokens[1].val.reg_idx;            \
        int dst_reg = tokens[3].val.reg_idx;            \
        sh4_bin_##op##_fvm_fvn(emit, src_reg, dst_reg); \
    }

OP_FVM_FVN(fipr)

#define OP_XMTRX_FVN(op)                        \
    static void emit_##op##_xmtrx_fvn(void) {   \
        CHECK_FVN(3);                           \
        int reg_no = tokens[3].val.reg_idx;     \
        sh4_bin_##op##_xmtrx_fvn(emit, reg_no); \
    }

OP_XMTRX_FVN(ftrv)

struct pattern {
    parser_emit_func emit;
    enum tok_tp toks[MAX_TOKENS];
} const tok_ptrns[] = {
    // opcodes which take no arguments (noarg)
    { emit_div0u, { TOK_DIV0U, TOK_NEWLINE } },
    { emit_rts, { TOK_RTS, TOK_NEWLINE } },
    { emit_clrmac, { TOK_CLRMAC, TOK_NEWLINE } },
    { emit_clrs, { TOK_CLRS, TOK_NEWLINE } },
    { emit_clrt, { TOK_CLRT, TOK_NEWLINE } },
    { emit_ldtlb, { TOK_LDTLB, TOK_NEWLINE } },
    { emit_nop, { TOK_NOP, TOK_NEWLINE } },
    { emit_rte, { TOK_RTE, TOK_NEWLINE } },
    { emit_sets, { TOK_SETS, TOK_NEWLINE } },
    { emit_sett, { TOK_SETT, TOK_NEWLINE } },
    { emit_sleep, { TOK_SLEEP, TOK_NEWLINE } },
    { emit_frchg, { TOK_FRCHG, TOK_NEWLINE } },
    { emit_fschg, { TOK_FSCHG, TOK_NEWLINE } },

    { emit_movt_rn, { TOK_MOVT, TOK_RN, TOK_NEWLINE } },
    { emit_cmppz_rn, { TOK_CMPPZ, TOK_RN, TOK_NEWLINE } },
    { emit_cmppl_rn, { TOK_CMPPL, TOK_RN, TOK_NEWLINE } },
    { emit_dt_rn, { TOK_DT, TOK_RN, TOK_NEWLINE } },
    { emit_rotl_rn, { TOK_ROTL, TOK_RN, TOK_NEWLINE } },
    { emit_rotr_rn, { TOK_ROTR, TOK_RN, TOK_NEWLINE } },
    { emit_rotcl_rn, { TOK_ROTCL, TOK_RN, TOK_NEWLINE } },
    { emit_rotcr_rn, { TOK_ROTCR, TOK_RN, TOK_NEWLINE } },
    { emit_shal_rn, { TOK_SHAL, TOK_RN, TOK_NEWLINE } },
    { emit_shar_rn, { TOK_SHAR, TOK_RN, TOK_NEWLINE } },
    { emit_shll_rn, { TOK_SHLL, TOK_RN, TOK_NEWLINE } },
    { emit_shlr_rn, { TOK_SHLR, TOK_RN, TOK_NEWLINE } },
    { emit_shll2_rn, { TOK_SHLL2, TOK_RN, TOK_NEWLINE } },
    { emit_shlr2_rn, { TOK_SHLR2, TOK_RN, TOK_NEWLINE } },
    { emit_shll8_rn, { TOK_SHLL8, TOK_RN, TOK_NEWLINE } },
    { emit_shlr8_rn, { TOK_SHLR8, TOK_RN, TOK_NEWLINE } },
    { emit_shll16_rn, { TOK_SHLL16, TOK_RN, TOK_NEWLINE } },
    { emit_shlr16_rn, { TOK_SHLR16, TOK_RN, TOK_NEWLINE } },
    { emit_braf_rn, { TOK_BRAF, TOK_RN, TOK_NEWLINE } },
    { emit_bsrf_rn, { TOK_BSRF, TOK_RN, TOK_NEWLINE } },

    { emit_tasb_arn, { TOK_TASB, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_ocbi_arn, { TOK_OCBI, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_ocbp_arn, { TOK_OCBP, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_ocbwb_arn, { TOK_OCBWB, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_pref_arn, { TOK_PREF, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_jmp_arn, { TOK_JMP, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_jsr_arn, { TOK_JSR, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_ldc_rm_sr, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_SR, TOK_NEWLINE } },
    { emit_ldc_rm_gbr, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_GBR, TOK_NEWLINE } },
    { emit_ldc_rm_vbr, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_VBR, TOK_NEWLINE } },
    { emit_ldc_rm_ssr, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_SSR, TOK_NEWLINE } },
    { emit_ldc_rm_spc, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_SPC, TOK_NEWLINE } },
    { emit_ldc_rm_dbr, { TOK_LDC, TOK_RN, TOK_COMMA, TOK_DBR, TOK_NEWLINE } },
    { emit_lds_rm_mach, { TOK_LDS, TOK_RN, TOK_COMMA, TOK_MACH, TOK_NEWLINE } },
    { emit_lds_rm_macl, { TOK_LDS, TOK_RN, TOK_COMMA, TOK_MACL, TOK_NEWLINE } },
    { emit_lds_rm_pr, { TOK_LDS, TOK_RN, TOK_COMMA, TOK_PR, TOK_NEWLINE } },
    { emit_lds_rm_fpscr, { TOK_LDS, TOK_RN, TOK_COMMA, TOK_FPSCR, TOK_NEWLINE } },
    { emit_lds_rm_fpul, { TOK_LDS, TOK_RN, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },

    { emit_stc_sr_rn, { TOK_STC, TOK_SR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_gbr_rn, { TOK_STC, TOK_GBR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_vbr_rn, { TOK_STC, TOK_VBR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_ssr_rn, { TOK_STC, TOK_SSR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_spc_rn, { TOK_STC, TOK_SPC, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_sgr_rn, { TOK_STC, TOK_SGR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_stc_dbr_rn, { TOK_STC, TOK_DBR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sts_mach_rn, { TOK_STS, TOK_MACH, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sts_macl_rn, { TOK_STS, TOK_MACL, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sts_pr_rn, { TOK_STS, TOK_PR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sts_fpscr_rn, { TOK_STS, TOK_FPSCR, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sts_fpul_rn, { TOK_STS, TOK_FPUL, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_ldcl_armp_sr, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_SR, TOK_NEWLINE } },
    { emit_ldcl_armp_gbr, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_GBR, TOK_NEWLINE } },
    { emit_ldcl_armp_vbr, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_VBR, TOK_NEWLINE } },
    { emit_ldcl_armp_ssr, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_SSR, TOK_NEWLINE } },
    { emit_ldcl_armp_spc, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_SPC, TOK_NEWLINE } },
    { emit_ldcl_armp_dbr, { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_DBR, TOK_NEWLINE } },
    { emit_ldsl_armp_mach, { TOK_LDSL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_MACH, TOK_NEWLINE } },
    { emit_ldsl_armp_macl, { TOK_LDSL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_MACL, TOK_NEWLINE } },
    { emit_ldsl_armp_pr, { TOK_LDSL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_PR, TOK_NEWLINE } },
    { emit_ldsl_armp_fpscr, { TOK_LDSL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_FPSCR, TOK_NEWLINE } },
    { emit_ldsl_armp_fpul, { TOK_LDSL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },

    { emit_stcl_sr_amrn, { TOK_STCL, TOK_SR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_gbr_amrn, { TOK_STCL, TOK_GBR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_vbr_amrn, { TOK_STCL, TOK_VBR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_ssr_amrn, { TOK_STCL, TOK_SSR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_spc_amrn, { TOK_STCL, TOK_SPC, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_sgr_amrn, { TOK_STCL, TOK_SGR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stcl_dbr_amrn, { TOK_STCL, TOK_DBR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stsl_mach_amrn, { TOK_STSL, TOK_MACH, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stsl_macl_amrn, { TOK_STSL, TOK_MACL, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stsl_pr_amrn, { TOK_STSL, TOK_PR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stsl_fpscr_amrn, { TOK_STSL, TOK_FPSCR, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_stsl_fpul_amrn, { TOK_STSL, TOK_FPUL, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },

    { emit_movcal_r0_arn, { TOK_MOVCAL, TOK_RN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_fldi0_frn, { TOK_FLDI0, TOK_FRN, TOK_NEWLINE } },
    { emit_fldi1_frn, { TOK_FLDI1, TOK_FRN, TOK_NEWLINE } },
    { emit_fabs_frn, { TOK_FABS, TOK_FRN, TOK_NEWLINE } },
    { emit_fneg_frn, { TOK_FNEG, TOK_FRN, TOK_NEWLINE } },
    { emit_fsqrt_frn, { TOK_FSQRT, TOK_FRN, TOK_NEWLINE } },
    { emit_fsrra_frn, { TOK_FSRRA, TOK_FRN, TOK_NEWLINE } },

    { emit_flds_frm_fpul, { TOK_FLDS, TOK_FRN, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },
    { emit_ftrc_frm_fpul, { TOK_FTRC, TOK_FRN, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },

    { emit_fsts_fpul_frn, { TOK_FSTS, TOK_FPUL, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_float_fpul_frn, { TOK_FLOAT, TOK_FPUL, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },

    { emit_cmpeq_imm8_r0, { TOK_CMPEQ, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_and_imm8_r0, { TOK_AND, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_or_imm8_r0, { TOK_OR, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_tst_imm8_r0, { TOK_TST, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_xor_imm8_r0, { TOK_XOR, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_andb_imm8_a_r0_gbr,
      { TOK_ANDB, TOK_IMM, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_orb_imm8_a_r0_gbr,
      { TOK_ORB, TOK_IMM, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_tstb_imm8_a_r0_gbr,
      { TOK_TSTB, TOK_IMM, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_xorb_imm8_a_r0_gbr,
      { TOK_XORB, TOK_IMM, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_bf_offs8, { TOK_BF, TOK_DISP, TOK_NEWLINE } },
    { emit_bfs_offs8, { TOK_BFS, TOK_DISP, TOK_NEWLINE } },
    { emit_bt_offs8, { TOK_BT, TOK_DISP, TOK_NEWLINE } },
    { emit_bts_offs8, { TOK_BTS, TOK_DISP, TOK_NEWLINE } },

    { emit_trapa_imm8, { TOK_TRAPA, TOK_IMM, TOK_NEWLINE } },

    { emit_movb_r0_a_disp8_gbr,
      { TOK_MOVB, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_movw_r0_a_disp8_gbr,
      { TOK_MOVW, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_movl_r0_a_disp8_gbr,
      { TOK_MOVL, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_movb_a_disp8_gbr_r0,
      { TOK_MOVB, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movw_a_disp8_gbr_r0,
      { TOK_MOVW, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movl_a_disp8_gbr_r0,
      { TOK_MOVL, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_GBR, TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_mova_a_offs8_pc_r0,
      { TOK_MOVA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_PC, TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_bra_offs12, { TOK_BRA, TOK_DISP, TOK_NEWLINE } },
    { emit_bsr_offs12, { TOK_BSR, TOK_DISP, TOK_NEWLINE } },

    { emit_mov_imm8_rn, { TOK_MOV, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_add_imm8_rn, { TOK_ADD, TOK_IMM, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_movw_a_offs8_pc_rn,
      { TOK_MOVW, TOK_AT, TOK_OPENPAREN, TOK_DISP, TOK_COMMA, TOK_PC,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movl_a_offs8_pc_rn,
      { TOK_MOVL, TOK_AT, TOK_OPENPAREN, TOK_DISP, TOK_COMMA, TOK_PC,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_mov_rm_rn, { TOK_MOV, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_swapb_rm_rn, { TOK_SWAPB, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_swapw_rm_rn, { TOK_SWAPW, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_xtrct_rm_rn, { TOK_XTRCT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_add_rm_rn, { TOK_ADD, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_addc_rm_rn, { TOK_ADDC, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_addv_rm_rn, { TOK_ADDV, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmpeq_rm_rn, { TOK_CMPEQ, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmphs_rm_rn, { TOK_CMPHS, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmpge_rm_rn, { TOK_CMPGE, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmphi_rm_rn, { TOK_CMPHI, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmpgt_rm_rn, { TOK_CMPGT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_cmpstr_rm_rn, { TOK_CMPSTR, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_div1_rm_rn, { TOK_DIV1, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_div0s_rm_rn, { TOK_DIV0S, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_dmulsl_rm_rn, { TOK_DMULSL, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_dmulul_rm_rn, { TOK_DMULUL, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_extsb_rm_rn, { TOK_EXTSB, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_extsw_rm_rn, { TOK_EXTSW, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_extub_rm_rn, { TOK_EXTUB, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_extuw_rm_rn, { TOK_EXTUW, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_mull_rm_rn, { TOK_MULL, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_mulsw_rm_rn, { TOK_MULSW, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_muluw_rm_rn, { TOK_MULUW, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_neg_rm_rn, { TOK_NEG, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_negc_rm_rn, { TOK_NEGC, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_sub_rm_rn, { TOK_SUB, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_subc_rm_rn, { TOK_SUBC, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_subv_rm_rn, { TOK_SUBV, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_and_rm_rn, { TOK_AND, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_not_rm_rn, { TOK_NOT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_or_rm_rn, { TOK_OR, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_tst_rm_rn, { TOK_TST, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_xor_rm_rn, { TOK_XOR, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_shad_rm_rn, { TOK_SHAD, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_shld_rm_rn, { TOK_SHLD, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_movb_rm_a_r0_rn,
      { TOK_MOVB, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_movw_rm_a_r0_rn,
      { TOK_MOVW, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_movl_rm_a_r0_rn,
      { TOK_MOVL, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA,
        TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_movb_a_r0_rm_rn,
      { TOK_MOVB, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movw_a_r0_rm_rn,
      { TOK_MOVW, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movl_a_r0_rm_rn,
      { TOK_MOVL, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_movb_rm_arn,
      { TOK_MOVB, TOK_RN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_movw_rm_arn,
      { TOK_MOVW, TOK_RN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },
    { emit_movl_rm_arn,
      { TOK_MOVL, TOK_RN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_movb_arm_rn,
      { TOK_MOVB, TOK_AT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movw_arm_rn,
      { TOK_MOVW, TOK_AT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movl_arm_rn,
      { TOK_MOVL, TOK_AT, TOK_RN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_movb_rm_amrn,
      { TOK_MOVB, TOK_RN, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_movw_rm_amrn,
      { TOK_MOVW, TOK_RN, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },
    { emit_movl_rm_amrn,
      { TOK_MOVL, TOK_RN, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN, TOK_NEWLINE } },

    { emit_movb_armp_rn,
      { TOK_MOVB, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movw_armp_rn,
      { TOK_MOVW, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movl_armp_rn,
      { TOK_MOVL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_macl_armp_arnp,
      { TOK_MAC_DOT_L, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA,
        TOK_AT, TOK_RN, TOK_PLUS, TOK_NEWLINE } },
    { emit_macw_armp_arnp,
      { TOK_MAC_DOT_W, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA,
        TOK_AT, TOK_RN, TOK_PLUS, TOK_NEWLINE } },

    { emit_fmov_frm_frn,
      { TOK_FMOV, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fadd_frm_frn,
      { TOK_FADD, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fcmpeq_frm_frn,
      { TOK_FCMPEQ, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fcmpgt_frm_frn,
      { TOK_FCMPGT, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fdiv_frm_frn,
      { TOK_FDIV, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fmul_frm_frn,
      { TOK_FMUL, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },
    { emit_fsub_frm_frn,
      { TOK_FSUB, TOK_FRN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },

    { emit_fmovs_arm_frn,
      { TOK_FMOVS, TOK_AT, TOK_RN, TOK_COMMA, TOK_FRN,
        TOK_NEWLINE } },

    { emit_fmovs_a_r0_rm_frn,
      { TOK_FMOVS, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },

    { emit_fmovs_armp_frn,
      { TOK_FMOVS, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_FRN, TOK_NEWLINE } },

    { emit_fmovs_frm_arn,
      { TOK_FMOVS, TOK_FRN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_fmovs_frm_amrn,
      { TOK_FMOVS, TOK_FRN, TOK_COMMA, TOK_AT,
        TOK_MINUS, TOK_RN, TOK_NEWLINE } },

    { emit_fmovs_frm_a_r0_rn,
      { TOK_FMOVS, TOK_FRN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_fmac_fr0_frm_frn,
      { TOK_FMAC, TOK_FRN, TOK_COMMA, TOK_FRN,
        TOK_COMMA, TOK_FRN, TOK_NEWLINE } },

    { emit_ldc_rm_rn_bank,
      { TOK_LDC, TOK_RN, TOK_COMMA, TOK_RN_BANK, TOK_NEWLINE } },

    { emit_stc_rm_bank_rn,
      { TOK_STC, TOK_RN_BANK, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_ldcl_armp_rn_bank,
      { TOK_LDCL, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_RN_BANK,
        TOK_NEWLINE } },

    { emit_stcl_rm_bank_amrn,
      { TOK_STCL, TOK_RN_BANK, TOK_COMMA, TOK_AT, TOK_MINUS, TOK_RN,
        TOK_NEWLINE } },

    { emit_movb_r0_a_disp4_rn,
      { TOK_MOVB, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },
    { emit_movw_r0_a_disp4_rn,
      { TOK_MOVW, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_movb_a_disp4_rm_r0,
      { TOK_MOVB, TOK_AT, TOK_OPENPAREN, TOK_DISP, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },
    { emit_movw_a_disp4_rm_r0,
      { TOK_MOVW, TOK_AT, TOK_OPENPAREN, TOK_DISP, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_movl_rm_a_disp4_rn,
      { TOK_MOVL, TOK_RN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_DISP,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_movl_a_disp4_rm_rn,
      { TOK_MOVL, TOK_AT, TOK_OPENPAREN, TOK_DISP, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_RN, TOK_NEWLINE } },

    { emit_fmov_drm_drn,
      { TOK_FMOV, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fadd_drm_drn,
      { TOK_FADD, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fcmpeq_drm_drn,
      { TOK_FCMPEQ, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fcmpgt_drm_drn,
      { TOK_FCMPGT, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fdiv_drm_drn,
      { TOK_FDIV, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fmul_drm_drn,
      { TOK_FMUL, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fsub_drm_drn,
      { TOK_FSUB, TOK_DRN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fmov_drm_xdn,
      { TOK_FMOV, TOK_DRN, TOK_COMMA, TOK_XDN, TOK_NEWLINE } },

    { emit_fmov_xdm_drn,
      { TOK_FMOV, TOK_XDN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fmov_xdm_xdn,
      { TOK_FMOV, TOK_XDN, TOK_COMMA, TOK_XDN, TOK_NEWLINE } },

    { emit_fmov_arm_drn,
      { TOK_FMOV, TOK_AT, TOK_RN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fmov_a_r0_rm_drn,
      { TOK_FMOV, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fmov_armp_drn,
      { TOK_FMOV, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fmov_arm_xdn,
      { TOK_FMOV, TOK_AT, TOK_RN, TOK_COMMA, TOK_XDN, TOK_NEWLINE } },

    { emit_fmov_armp_xdn,
      { TOK_FMOV, TOK_AT, TOK_RN, TOK_PLUS, TOK_COMMA, TOK_XDN, TOK_NEWLINE } },

    { emit_fmov_a_r0_rm_xdn,
      { TOK_FMOV, TOK_AT, TOK_OPENPAREN, TOK_RN, TOK_COMMA, TOK_RN,
        TOK_CLOSEPAREN, TOK_COMMA, TOK_XDN, TOK_NEWLINE } },

    { emit_fmov_drm_amrn,
      { TOK_FMOV, TOK_DRN, TOK_COMMA, TOK_AT,
        TOK_MINUS, TOK_RN, TOK_NEWLINE } },

    { emit_fmov_drm_arn,
      { TOK_FMOV, TOK_DRN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_fmov_drm_a_r0_rn,
      { TOK_FMOV, TOK_DRN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_fmov_xdm_arn,
      { TOK_FMOV, TOK_XDN, TOK_COMMA, TOK_AT, TOK_RN, TOK_NEWLINE } },

    { emit_fmov_xdm_amrn,
      { TOK_FMOV, TOK_XDN, TOK_COMMA, TOK_AT,
        TOK_MINUS, TOK_RN, TOK_NEWLINE } },

    { emit_fmov_xdm_a_r0_rn,
      { TOK_FMOV, TOK_XDN, TOK_COMMA, TOK_AT, TOK_OPENPAREN, TOK_RN,
        TOK_COMMA, TOK_RN, TOK_CLOSEPAREN, TOK_NEWLINE } },

    { emit_fabs_drn, { TOK_FABS, TOK_DRN, TOK_NEWLINE } },
    { emit_fneg_drn, { TOK_FNEG, TOK_DRN, TOK_NEWLINE } },
    { emit_fsqrt_drn, { TOK_FSQRT, TOK_DRN, TOK_NEWLINE } },

    { emit_fcnvds_drm_fpul,
      { TOK_FCNVDS, TOK_DRN, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },
    { emit_ftrc_drm_fpul,
      { TOK_FTRC, TOK_DRN, TOK_COMMA, TOK_FPUL, TOK_NEWLINE } },

    { emit_fcnvsd_fpul_drn,
      { TOK_FCNVSD, TOK_FPUL, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_float_fpul_drn,
      { TOK_FLOAT, TOK_FPUL, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },
    { emit_fsca_fpul_drn,
      { TOK_FSCA, TOK_FPUL, TOK_COMMA, TOK_DRN, TOK_NEWLINE } },

    { emit_fipr_fvm_fvn,
      { TOK_FIPR, TOK_FVN, TOK_COMMA, TOK_FVN, TOK_NEWLINE } },

    { emit_ftrv_xmtrx_fvn,
      { TOK_FTRV, TOK_XMTRX, TOK_COMMA, TOK_FVN, TOK_NEWLINE } },

    { NULL }
};

static void push_token(struct tok const *tk);
static void pop_token(struct tok *tk);
static void process_line(void);
static bool check_pattern(struct pattern const *ptrn);

void parser_input_token(struct tok const *tk) {
    if (tk->tp == TOK_NEWLINE) {
        process_line();
        n_tokens = 0;
    } else {
        push_token(tk);
    }
}

void parser_set_emitter(emit_bin_handler_func em) {
    emit = em;
}

static void push_token(struct tok const *tk) {
    if (n_tokens >= MAX_TOKENS)
        errx(1, "too many tokens");
    tokens[n_tokens++] = *tk;
}

static void pop_token(struct tok *tk) {
    if (!n_tokens)
        errx(1, "no more tokens");
    *tk = tokens[n_tokens - 1];
    n_tokens--;
}

static void process_line(void) {
    struct pattern const *curs = tok_ptrns;

    while (curs->emit) {
        if (check_pattern(curs)) {
            curs->emit();
            return;
        }
        curs++;
    }

    printf("%u tokens\n\t", n_tokens);
    unsigned tok_idx;
    for (tok_idx = 0; tok_idx < n_tokens; tok_idx++)
        printf("%s ", tok_as_str(tokens + tok_idx));
    puts("\n");

    errx(1, "unrecognized pattern");
}

static bool check_pattern(struct pattern const *ptrn) {
    unsigned n_matched = 0;
    enum tok_tp const *cur_tok = ptrn->toks;
    unsigned tok_idx = 0;

    while (*cur_tok != TOK_NEWLINE && tok_idx < n_tokens) {
        if (*cur_tok != tokens[tok_idx].tp)
            return false;

        cur_tok++;
        tok_idx++;
    }

    return (*cur_tok == TOK_NEWLINE) && (tok_idx == n_tokens);
}
