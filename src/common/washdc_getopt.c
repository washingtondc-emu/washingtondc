/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#include <stdio.h>

#include "washdc_getopt.h"

char *washdc_optarg;
int washdc_optind, washdc_opterr, washdc_optopt;

int washdc_getopt(int argc, char **argv, char const *optstring) {
    static char *nextch;

    if (washdc_optind == 0) {
        nextch = NULL;
        washdc_optind = 1;
    }

    if (washdc_optind < 1 || washdc_optind >= argc || !argv[washdc_optind][0])
        return -1;

    if (argv[washdc_optind][0] != '-')
        return -1;

    if (!nextch)
        nextch = argv[washdc_optind] + 1;
    char optch = *nextch;
    char const *optptr;
    if (optch && (optptr = strchr(optstring, optch))) {
        if (optptr[1] == ':') {
            if (nextch[1]) {
                washdc_optarg = ++nextch;
                washdc_optind++;
                nextch=NULL;
            } else {
                if (washdc_optind >= argc - 1) {
                    // there's nothing after the arg...
                    washdc_optopt = optch;
                    nextch = NULL;
                    washdc_optind++;
                    return '?';
                }
                washdc_optarg = argv[washdc_optind + 1];
                washdc_optind += 2;
                nextch = NULL;
            }
        } else {
            if (nextch[1]) {
                nextch++;
            } else {
                washdc_optind++;
                nextch = NULL;
            }
        }

        return  (int)optch;
    } else {
        washdc_optind++;
        nextch = NULL;
        washdc_optopt = optch;
        return '?';
    }
}
