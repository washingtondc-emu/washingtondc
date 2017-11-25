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
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include "lexer.h"
#include "parser.h"
#include "disas.h"

static FILE *output;
static FILE *input;

static struct options {
    char const *filename_in, *filename_out;
    bool bin_mode;
    bool print_addrs;
    bool disas;
    bool hex_comments;
    bool case_insensitive;
} options;

static unsigned to_hex(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;

    errx(1, "character \"%c\" is not hex\n", ch);
}

static void print_usage(char const *cmd) {
    fprintf(stderr, "Usage: %s -[bdlcu] [-i input] [-o output] instruction\n",
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

    while ((ch = fgetc(input)) != EOF) {
        if (options.case_insensitive)
            ch = tolower(ch);
        lexer_input_char(ch, parser_input_token);
    }
}

static void do_emit_asm(char ch) {
    fputc(ch, output);
}

static void do_disasm(void) {
    unsigned pc = 0;

    if (options.bin_mode) {
        uint16_t dat;
        while (fread(&dat, sizeof(dat), 1, input) == 1) {
            if (options.print_addrs)
                fprintf(output, "%08x:    ", pc);
            disas_inst(dat, do_emit_asm);
            pc += 2;
            if (options.hex_comments)
                fprintf(output, " ! 0x%04x", (unsigned)dat);
            fputc('\n', output);
        }
    } else {
        bool even = true;
        int ch;
#define DAT_BUF_LEN 2
        int dat_buf[DAT_BUF_LEN];
        int dat;
        int n_bytes = 0;
        while ((ch = fgetc(input)) != EOF) {

            if (!isxdigit(ch)) {
                if (!even)
                    dat_buf[n_bytes++] = dat;
                even = true;
                goto drain;
            }

            if (even) {
                dat = to_hex(ch);
            } else {
                dat = (dat << 4) | to_hex(ch);
                dat_buf[n_bytes++] = dat;
            }

            even = !even;

        drain:
            if (n_bytes == DAT_BUF_LEN) {
                size_t idx = 0;
                uint16_t dat16 = (dat_buf[0] & 0xff) |
                    ((dat_buf[1] & 0xff) << 8);

                if (options.print_addrs)
                    fprintf(output, "%08x:    ", pc);
                disas_inst(dat16, do_emit_asm);
                pc += 2;
                if (options.hex_comments)
                    fprintf(output, " ! 0x%04x", (unsigned)dat16);
                fputc('\n', output);

                n_bytes = 0;
            }
        }
    }
}

int main(int argc, char **argv) {
    int opt;
    char const *cmd = argv[0];

    FILE *file_out = NULL;
    FILE *file_in = NULL;
    output = stdout;
    input = stdin;

    while ((opt = getopt(argc, argv, "bcdli:o:u")) != -1) {
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
        case 'u':
            options.case_insensitive = true;
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
        do_disasm();
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
