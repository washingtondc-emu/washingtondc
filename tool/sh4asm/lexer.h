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

#ifndef LEXER_H_
#define LEXER_H_

enum tok_tp {
    TOK_COMMA,
    TOK_OPENPAREN,
    TOK_CLOSEPAREN,
    TOK_AT,
    TOK_NEWLINE,
    TOK_PLUS,
    TOK_MINUS,

    // opcodes
    TOK_DIV0U,
    TOK_RTS,
    TOK_CLRMAC,
    TOK_CLRS,
    TOK_CLRT,
    TOK_LDTLB,
    TOK_NOP,
    TOK_RTE,
    TOK_SETS,
    TOK_SETT,
    TOK_SLEEP,
    TOK_FRCHG,
    TOK_FSCHG,
    TOK_MOVT,
    TOK_CMPPZ,
    TOK_CMPPL,
    TOK_DT,
    TOK_ROTL,
    TOK_ROTR,
    TOK_ROTCL,
    TOK_ROTCR,
    TOK_SHAL,
    TOK_SHAR,
    TOK_SHLL,
    TOK_SHLR,
    TOK_SHLL2,
    TOK_SHLR2,
    TOK_SHLL8,
    TOK_SHLR8,
    TOK_SHLL16,
    TOK_SHLR16,
    TOK_BRAF,
    TOK_BSRF,
    TOK_CMPEQ,
    TOK_ANDB,
    TOK_AND,
    TOK_ORB,
    TOK_OR,
    TOK_TST,
    TOK_TSTB,
    TOK_XOR,
    TOK_XORB,
    TOK_BF,
    TOK_BFS,
    TOK_BT,
    TOK_BTS,
    TOK_BRA,
    TOK_BSR,
    TOK_TRAPA,
    TOK_TASB,
    TOK_OCBI,
    TOK_OCBP,
    TOK_OCBWB,
    TOK_PREF,
    TOK_JMP,
    TOK_JSR,
    TOK_LDC,
    TOK_STC,
    TOK_LDCL,
    TOK_STCL,
    TOK_MOV,
    TOK_ADD,
    TOK_MOVW,
    TOK_MOVL,
    TOK_SWAPB,
    TOK_SWAPW,
    TOK_XTRCT,
    TOK_ADDC,
    TOK_ADDV,
    TOK_CMPHS,
    TOK_CMPGE,
    TOK_CMPHI,
    TOK_CMPGT,
    TOK_CMPSTR,
    TOK_DIV1,
    TOK_DIV0S,
    TOK_DMULSL,
    TOK_DMULUL,
    TOK_EXTSB,
    TOK_EXTSW,
    TOK_EXTUB,
    TOK_EXTUW,
    TOK_MULL,
    TOK_MULSW,
    TOK_MULUW,
    TOK_NEG,
    TOK_NEGC,
    TOK_SUB,
    TOK_SUBC,
    TOK_SUBV,
    TOK_NOT,
    TOK_SHAD,
    TOK_SHLD,
    TOK_LDS,
    TOK_STS,
    TOK_LDSL,
    TOK_STSL,
    TOK_MOVB,
    TOK_MOVA,
    TOK_MOVCAL,
    TOK_MAC_DOT_L,
    TOK_MAC_DOT_W,
    TOK_FLDI0,
    TOK_FLDI1,
    TOK_FMOV,
    TOK_FMOVS,
    TOK_FLDS,
    TOK_FSTS,
    TOK_FABS,
    TOK_FADD,
    TOK_FCMPEQ,
    TOK_FCMPGT,
    TOK_FDIV,
    TOK_FLOAT,
    TOK_FMAC,
    TOK_FMUL,
    TOK_FNEG,
    TOK_FSQRT,
    TOK_FSUB,
    TOK_FTRC,
    TOK_FCNVDS,
    TOK_FCNVSD,
    TOK_FIPR,
    TOK_FTRV,
    TOK_FSCA,
    TOK_FSRRA,

    // registers
    TOK_SR,
    TOK_GBR,
    TOK_VBR,
    TOK_SSR,
    TOK_SPC,
    TOK_SGR,
    TOK_DBR,
    TOK_PC,
    TOK_MACH,
    TOK_MACL,
    TOK_PR,
    TOK_FPUL,
    TOK_FPSCR,

    TOK_RN,
    TOK_RN_BANK,
    TOK_FRN,
    TOK_DRN,
    TOK_XDN,
    TOK_FVN,

    TOK_XMTRX,

    TOK_IMM,
    TOK_DISP
};

union tok_val {
    int as_int;
    int reg_idx;
};

struct tok {
    enum tok_tp tp;

    // this only has meaning for certain types of tokens
    union tok_val val;
};

typedef void(*emit_tok_func)(struct tok const*);

/*
 * input the given character to the lexer.  If the character is successfully
 * tokenized, then it is input to the emitter function.
 *
 * the struct tok pointer input to the emitter is not persistent, and should not
 * be accessed after the emitter function returns.
 */
void lexer_input_char(char ch, emit_tok_func emit);

/*
 * return a text-based representation of the given token.
 * the string returned by this function is not persistent,
 * and therefore should not be referenced again aftetr the next
 * time this function is called.
 */
char const *tok_as_str(struct tok const *tk);

#endif
