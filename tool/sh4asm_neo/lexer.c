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
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#include "lexer.h"

static struct tok_mapping {
    char const *txt;
    enum tok_tp tok;
} const tok_map[] = {
    { ",", TOK_COMMA },
    { "(", TOK_OPENPAREN },
    { ")", TOK_CLOSEPAREN },
    { "@", TOK_AT },
    { "div0u", TOK_DIV0U },
    { "rts", TOK_RTS },
    { "clrmac", TOK_CLRMAC },
    { "clrs", TOK_CLRS },
    { "clrt", TOK_CLRT },
    { "ldtlb", TOK_LDTLB },
    { "nop", TOK_NOP },
    { "rte", TOK_RTE },
    { "sets", TOK_SETS },
    { "sett", TOK_SETT },
    { "sleep", TOK_SLEEP },
    { "frchg", TOK_FRCHG },
    { "fschg", TOK_FSCHG },
    { "movt", TOK_MOVT },
    { "cmp/pz", TOK_CMPPZ },
    { "cmp/pl", TOK_CMPPL },
    { "dt", TOK_DT },
    { "rotl", TOK_ROTL },
    { "rotr", TOK_ROTR },
    { "rotcl", TOK_ROTCL },
    { "rotcr", TOK_ROTCR },
    { "shal", TOK_SHAL },
    { "shar", TOK_SHAR },
    { "shll", TOK_SHLL },
    { "shlr", TOK_SHLR },
    { "shll2", TOK_SHLL2 },
    { "shlr2", TOK_SHLR2 },
    { "shll8", TOK_SHLL8 },
    { "shlr8", TOK_SHLR8 },
    { "shll16", TOK_SHLL16 },
    { "shlr16", TOK_SHLR16 },
    { "braf", TOK_BRAF },
    { "bsrf", TOK_BSRF },
    { "cmp/eq", TOK_CMPEQ },
    { "and.b", TOK_ANDB },
    { "and", TOK_AND },
    { "or.b", TOK_ORB },
    { "or", TOK_OR },
    { "tst", TOK_TST },
    { "tst.b", TOK_TSTB },
    { "xor", TOK_XOR },
    { "xor.b", TOK_XORB },
    { "bf", TOK_BF },
    { "bf/s", TOK_BFS },
    { "bt", TOK_BT },
    { "bt/s", TOK_BTS },
    { "bra", TOK_BRA },
    { "bsr", TOK_BSR },
    { "trapa", TOK_TRAPA },
    { "tas.b", TOK_TASB },
    { "ocbi", TOK_OCBI },
    { "ocbp", TOK_OCBP },
    { "ocbwb", TOK_OCBWB },
    { "pref", TOK_PREF },
    { "jmp", TOK_JMP },
    { "jsr", TOK_JSR },
    { "ldc", TOK_LDC },
    { "stc", TOK_STC },
    { "ldc.l", TOK_LDCL },
    { "stc.l", TOK_STCL },
    { "mov", TOK_MOV },
    { "add", TOK_ADD },
    { "mov.w", TOK_MOVW },
    { "mov.l", TOK_MOVL },
    { "swap.b", TOK_SWAPB },
    { "swap.w", TOK_SWAPW },
    { "xtrct", TOK_XTRCT },
    { "addc", TOK_ADDC },
    { "addv", TOK_ADDV },
    { "cmp/hs", TOK_CMPHS },
    { "cmp/ge", TOK_CMPGE },
    { "cmp/hi", TOK_CMPHI },
    { "cmp/gt", TOK_CMPGT },
    { "cmp/str", TOK_CMPSTR },
    { "div1", TOK_DIV1 },
    { "div0s", TOK_DIV0S },
    { "dmuls.l", TOK_DMULSL },
    { "dmulu.l", TOK_DMULUL },
    { "exts.b", TOK_EXTSB },
    { "exts.w", TOK_EXTSW },
    { "extu.b", TOK_EXTUB },
    { "extu.w", TOK_EXTUW },
    { "mul.l", TOK_MULL },
    { "muls.w", TOK_MULSW },
    { "mulu.w", TOK_MULUW },
    { "neg", TOK_NEG },
    { "negc", TOK_NEGC },
    { "sub", TOK_SUB },
    { "subc", TOK_SUBC },
    { "subv", TOK_SUBV },
    { "not", TOK_NOT },
    { "shad", TOK_SHAD },
    { "shld", TOK_SHLD },
    { "lds", TOK_LDS },
    { "sts", TOK_STS },
    { "lds.l", TOK_LDSL },
    { "sts.l", TOK_STSL },
    { "mov.b", TOK_MOVB },
    { "mova", TOK_MOVA },
    { "movca.l", TOK_MOVCAL },
    { "fldi0", TOK_FLDI0 },
    { "fldi1", TOK_FLDI1 },
    { "fmov", TOK_FMOV },
    { "fmov.s", TOK_FMOVS },
    { "flds", TOK_FLDS },
    { "fsts", TOK_FSTS },
    { "fabs", TOK_FABS },
    { "fadd", TOK_FADD },
    { "fcmp/eq", TOK_FCMPEQ },
    { "fcmp/gt", TOK_FCMPGT },
    { "fdiv", TOK_FDIV },
    { "float", TOK_FLOAT },
    { "fmac", TOK_FMAC },
    { "fmul", TOK_FMUL },
    { "fneg", TOK_FNEG },
    { "fsqrt", TOK_FSQRT },
    { "fsub", TOK_FSUB },
    { "ftrc", TOK_FTRC },
    { "fcnvds", TOK_FCNVDS },
    { "fcnvsd", TOK_FCNVSD },
    { "fipr", TOK_FIPR },
    { "ftrv", TOK_FTRV },

    { "sr", TOK_SR },
    { "gbr", TOK_GBR },
    { "vbr", TOK_VBR },
    { "ssr", TOK_SSR },
    { "spc", TOK_SPC },
    { "sgr", TOK_SGR },
    { "dbr", TOK_DBR },
    { "pc", TOK_PC },
    { "pr", TOK_PR },
    { "fpul", TOK_FPUL },
    { "fpscr", TOK_FPSCR },

    { "r0", TOK_R0 },
    { "r1", TOK_R1 },
    { "r2", TOK_R2 },
    { "r3", TOK_R3 },
    { "r4", TOK_R4 },
    { "r5", TOK_R5 },
    { "r6", TOK_R6 },
    { "r7", TOK_R7 },
    { "r8", TOK_R8 },
    { "r9", TOK_R9 },
    { "r10", TOK_R10 },
    { "r11", TOK_R11 },
    { "r12", TOK_R12 },
    { "r13", TOK_R13 },
    { "r14", TOK_R14 },
    { "r15", TOK_R15 },

    { "fr0", TOK_FR0 },
    { "fr1", TOK_FR1 },
    { "fr2", TOK_FR2 },
    { "fr3", TOK_FR3 },
    { "fr4", TOK_FR4 },
    { "fr5", TOK_FR5 },
    { "fr6", TOK_FR6 },
    { "fr7", TOK_FR7 },
    { "fr8", TOK_FR8 },
    { "fr9", TOK_FR9 },
    { "fr10", TOK_FR10 },
    { "fr11", TOK_FR11 },
    { "fr12", TOK_FR12 },
    { "fr13", TOK_FR13 },
    { "fr14", TOK_FR14 },
    { "fr15", TOK_FR15 },

    { "dr0", TOK_DR0 },
    { "dr2", TOK_DR2 },
    { "dr4", TOK_DR4 },
    { "dr6", TOK_DR6 },
    { "dr8", TOK_DR8 },
    { "dr10", TOK_DR10 },
    { "dr12", TOK_DR12 },
    { "dr14", TOK_DR14 },

    { "xd0", TOK_XD0 },
    { "xd2", TOK_XD2 },
    { "xd4", TOK_XD4 },
    { "xd6", TOK_XD6 },
    { "xd8", TOK_XD8 },
    { "xd10", TOK_XD10 },
    { "xd12", TOK_XD12 },
    { "xd14", TOK_XD14 },

    { "fv0", TOK_FV0 },
    { "fv4", TOK_FV4 },
    { "fv8", TOK_FV8 },
    { "fv12", TOK_FV12 },

    { "xmtrx", TOK_XMTRX },

    { NULL }
};
#define TOK_LEN_MAX 32
static char cur_tok[TOK_LEN_MAX];
unsigned tok_len;

static struct tok_mapping const* check_tok(void) {
    struct tok_mapping const *curs = tok_map;

    while (curs->txt) {
        if (strcmp(cur_tok, curs->txt) == 0)
            return curs;
        curs++;
    }

    return NULL; // no token found
}

void lexer_input_char(char ch, emit_tok_func emit) {
    if (tok_len >= (TOK_LEN_MAX - 1))
        err(1, "Token is too long");

    if (isspace(ch) || ch == ',' || ch == '@' ||
        ch == '(' || ch == ')' || ch == '\0') {
        if (tok_len) {
            cur_tok[tok_len] = '\0';

            struct tok_mapping const *mapping = check_tok();
            if (mapping) {
                // 'normal' single-word token
                struct tok tk = {
                    .tp = mapping->tok
                };
                emit(&tk);
            } else if (cur_tok[0] == '#') {
                // string literal
                errno = 0;
                long val_as_long = strtol(cur_tok + 1, NULL, 0);
                if (errno)
                    err(1, "failed to decode integer literal");
                struct tok tk = {
                    .tp = TOK_INTEGER_LITERAL,
                    .val = { .as_int = val_as_long }
                };
                emit(&tk);
            } else {
                errx(1, "unrecognized token \"%s\"", cur_tok);
            }

            tok_len = 0;
        }

        // don't forget the comma if that was the delimter that brought us here
        if (ch == ',') {
            struct tok tk = {
                .tp = TOK_COMMA
            };
            emit(&tk);
        } else if (ch == '(') {
            struct tok tk = {
                .tp = TOK_OPENPAREN
            };
            emit(&tk);
        } else if (ch == ')') {
            struct tok tk = {
                .tp = TOK_CLOSEPAREN
            };
            emit(&tk);
        } else if (ch == '@') {
            struct tok tk = {
                .tp = TOK_AT
            };
            emit(&tk);
        }
    } else {
        cur_tok[tok_len++] = ch;
    }
}

char const *tok_as_str(struct tok const *tk) {
    static char buf[TOK_LEN_MAX];

    if (tk->tp == TOK_INTEGER_LITERAL) {
        snprintf(buf, TOK_LEN_MAX, "#0x%x", tk->val.as_int);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    }

    struct tok_mapping const *curs = tok_map;
    while (curs->txt) {
        if (curs->tok == tk->tp)
            return curs->txt;
        curs++;
    }

    return NULL;
}
