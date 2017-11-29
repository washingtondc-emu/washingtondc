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
    { "\\n", TOK_NEWLINE },
    { "+", TOK_PLUS },
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
    { "mac.w", TOK_MAC_DOT_W },
    { "mac.l", TOK_MAC_DOT_L },
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
    { "fsca", TOK_FSCA },
    { "fsrra", TOK_FSRRA },


    { "sr", TOK_SR },
    { "gbr", TOK_GBR },
    { "vbr", TOK_VBR },
    { "ssr", TOK_SSR },
    { "spc", TOK_SPC },
    { "sgr", TOK_SGR },
    { "dbr", TOK_DBR },
    { "pc", TOK_PC },
    { "mach", TOK_MACH },
    { "macl", TOK_MACL },
    { "pr", TOK_PR },
    { "fpul", TOK_FPUL },
    { "fpscr", TOK_FPSCR },

    { "xmtrx", TOK_XMTRX },

    { NULL }
};
#define TOK_LEN_MAX 32
static char cur_tok[TOK_LEN_MAX];
unsigned tok_len;

static struct tok_mapping const* check_tok(void) {
    struct tok_mapping const *curs = tok_map;

    while (curs->txt) {
        if (strcmp(cur_tok, curs->txt) == 0) {
            return curs;
        }
        curs++;
    }

    return NULL; // no token found
}

void lexer_input_char(char ch, emit_tok_func emit) {
    if (tok_len >= (TOK_LEN_MAX - 1))
        err(1, "Token is too long");

    if (isspace(ch) || ch == ',' || ch == '@' || ch == '(' || ch == ')' ||
        ch == '\0' || ch == '\n' || ch == '+' || ch == '-') {
        if (tok_len) {
            cur_tok[tok_len] = '\0';

            struct tok_mapping const *mapping = check_tok();
            if (mapping) {
                // 'normal' single-word token
                struct tok tk = {
                    .tp = mapping->tok
                };
                emit(&tk);
            } else if (cur_tok[0] == '#' && tok_len > 1) {
                // string literal
                errno = 0;
                long val_as_long = strtol(cur_tok + 1, NULL, 0);
                if (errno)
                    err(1, "failed to decode integer literal");
                struct tok tk = {
                    .tp = TOK_IMM,
                    .val = { .as_int = val_as_long }
                };
                emit(&tk);
            } else if (cur_tok[0] == 'r' && (tok_len == 2 || tok_len == 3)) {
                // general-purpose register
                int reg_no = atoi(cur_tok + 1);
                if (reg_no < 0 || reg_no > 15)
                    errx(1, "invalid register index %d", reg_no);
                struct tok tk = {
                    .tp = TOK_RN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if (cur_tok[0] == 'r' && (tok_len == 7 || tok_len == 8) &&
                       cur_tok[tok_len - 1] == 'k' && cur_tok[tok_len - 2] == 'n' &&
                       cur_tok[tok_len - 3] == 'a' && cur_tok[tok_len - 4] == 'b' &&
                       cur_tok[tok_len - 5] == '_') {
                // banked general-purpose register
                int reg_no = atoi(cur_tok + 1);
                if (reg_no < 0 || reg_no > 15)
                    errx(1, "invalid banked register index %d", reg_no);
                struct tok tk = {
                    .tp = TOK_RN_BANK,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'f' && cur_tok[1] == 'r') {
                // floating-point register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15)
                    errx(1, "invalid floating-point register index %d",
                         reg_no);
                struct tok tk = {
                    .tp = TOK_FRN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'd' && cur_tok[1] == 'r') {
                // double-precision floating-point register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 1))
                    errx(1, "invalid double-precision floating-point "
                         "register index %d", reg_no);
                struct tok tk = {
                    .tp = TOK_DRN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'x' && cur_tok[1] == 'd') {
                // double-precision floating-point register (banked-out)
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 1))
                    errx(1, "invalid banked double-precision floating-point "
                         "register index %d", reg_no);
                struct tok tk = {
                    .tp = TOK_XDN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'f' && cur_tok[1] == 'v') {
                // floating-point vector register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 3))
                    errx(1, "invalid floating-point vector register index "
                         "%d\n", reg_no);
                struct tok tk = {
                    .tp = TOK_FVN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else {
                /*
                 * Maybe it's an offset (which is an integer literal without a
                 * preceding '#' character).  Try to decode it as one, and
                 * error out if that assumption does not fit.
                 */
                errno = 0;
                long val_as_long = strtol(cur_tok, NULL, 0);
                if (errno)
                    errx(1, "unrecognized token \"%s\"", cur_tok);
                struct tok tk = {
                    .tp = TOK_DISP,
                    .val = { .as_int = val_as_long }
                };
                emit(&tk);
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
        } else if (ch == '\n') {
            struct tok tk = {
                .tp = TOK_NEWLINE
            };
            emit(&tk);
        } else if (ch == '+') {
            struct tok tk = {
                .tp = TOK_PLUS
            };
            emit(&tk);
        } else if (ch == '-') {
            struct tok tk = {
                .tp = TOK_MINUS
            };
            emit(&tk);
        }
    } else {
        cur_tok[tok_len++] = ch;
    }
}

char const *tok_as_str(struct tok const *tk) {
    static char buf[TOK_LEN_MAX];

    if (tk->tp == TOK_IMM) {
        snprintf(buf, TOK_LEN_MAX, "#0x%x", tk->val.as_int);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_RN) {
        snprintf(buf, TOK_LEN_MAX, "r%u", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_RN_BANK) {
        snprintf(buf, TOK_LEN_MAX, "r%u_bank", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_FRN) {
        snprintf(buf, TOK_LEN_MAX, "fr%u", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_DRN) {
        snprintf(buf, TOK_LEN_MAX, "dr%u", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_XDN) {
        snprintf(buf, TOK_LEN_MAX, "xd%u", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_FVN) {
        snprintf(buf, TOK_LEN_MAX, "fv%u", tk->val.reg_idx);
        buf[TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == TOK_DISP) {
        snprintf(buf, TOK_LEN_MAX, "0x%x", tk->val.as_int);
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
