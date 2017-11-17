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
#include <stdbool.h>
#include <stddef.h>

#include "parser.h"

#define MAX_TOKENS 32
struct tok tokens[MAX_TOKENS];
unsigned n_tokens;

static emit_bin_handler_func emit;

typedef void(*parser_emit_func)(void);

struct pattern {
    parser_emit_func emit;
    enum tok_tp toks[MAX_TOKENS];
} const tok_ptrns[] = {
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
