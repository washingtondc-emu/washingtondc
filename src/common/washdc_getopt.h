/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#ifndef WASHDC_GETOPT_H_
#define WASHDC_GETOPT_H_

#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * my own home-made implementation of the posix getopt(3) function, since some
 * copmilers (by which I mean Micro$oft Visual C++) don't support it or any
 * comparable alternatives.
 *
 * This is missing the --, the non-option argument shifting that GNU does, and
 * (most importantly) the ability to have multiple options share a single
 * hyphen (ie -pv as a shorthand for -p -v).
 */

/*
 * frontend programs have to define these themselves.
 * optind must be initialized to 1.
 */
extern char *washdc_optarg;
extern int washdc_optind, washdc_opterr, washdc_optopt;

static int washdc_getopt(int argc, char **argv, char const *optstring) {
    bool need_arg = false;

    if (washdc_optind < 1 || washdc_optind >= argc)
        return -1;

    char *arg = argv[washdc_optind];
    int src_idx = washdc_optind;

    if (argv[src_idx][0] != '-')
        return -1;

    char optch = argv[src_idx][1];
    char const *optptr;
    if (optch && (optptr = strchr(optstring, optch))) {
        if (optptr[1] == ':') {
            if (washdc_optind >= argc - 1) {
                // there's nothing after the arg...
                washdc_optopt = optch;
                washdc_optind++;
                return '?';
            }
            washdc_optarg = argv[washdc_optind + 1];
            washdc_optind += 2;
        } else {
            washdc_optind++;
        }

        return  (int)optch;
    } else {
        washdc_optind++;
        washdc_optopt = optch;
        return '?';
    }
}

#ifdef __cplusplus
}
#endif

#endif
