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
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include "lexer.h"
#include "parser.h"

static FILE *output;
static FILE *input;

static struct options {
    char const *filename_in, *filename_out;
    bool bin_mode;
    bool print_addrs;
    bool disas;
    bool hex_comments;
} options;

static void print_usage(char const *cmd) {
    fprintf(stderr, "Usage: %s -[bdlc] [-i input] [-o output] instruction\n",
            cmd);
}

static void do_emit_bin(uint16_t inst) {
    if (options.bin_mode) {
        fwrite(&inst, sizeof(inst), 1, output);
    } else {
        fprintf(output, "%02x\n%02x\n", (unsigned)(inst & 255), (unsigned)(inst >> 8));
    }
}

static void do_asm(void) {
    int ch;

    parser_set_emitter(do_emit_bin);

    while ((ch = fgetc(input)) != EOF)
        lexer_input_char(ch, parser_input_token);
}

int main(int argc, char **argv) {
    int opt;
    char const *cmd = argv[0];

    FILE *file_out = NULL;
    FILE *file_in = NULL;
    output = stdout;
    input = stdin;

    while ((opt = getopt(argc, argv, "bcdli:o:")) != -1) {
        switch (opt) {
        case 'b':
            options.bin_mode = true;
            break;
        case 'c':
            options.hex_comments = true;
            break;
        case 'd':
            options.disas = true;
            break;
        case 'l':
            options.print_addrs = true;
            break;
        case 'i':
            options.filename_in = optarg;
            break;
        case 'o':
            options.filename_out = optarg;
            break;
        default:
            print_usage(cmd);
            return 1;
        }
    }

    argv += optind;
    argc -= optind;

    if (argc != 0) {
        print_usage(cmd);
        return 1;
    }

    if (options.filename_in)
        input = file_in = fopen(options.filename_in, "rb");

    if (options.filename_out)
        output = file_out = fopen(options.filename_out, "wb");

    if (options.disas)
        errx(1, "disassembly is not yet implemented");
    else
        do_asm();

    if (file_in)
        fclose(file_in);
    if (file_out)
        fclose(file_out);

    return 0;
}

static void emit(struct tok const *tk) {
    printf("tk->tp is %s\n", tok_as_str(tk));
}
