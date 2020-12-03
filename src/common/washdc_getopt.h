/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
    if (washdc_optind < 1 || washdc_optind >= argc)
        return -1;

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
