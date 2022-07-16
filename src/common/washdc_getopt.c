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
#include <string.h>

#include "washdc_getopt.h"

char *washdc_optarg;
int washdc_optind, washdc_opterr, washdc_optopt;

static void
shift_args(int argc, char **argv, int start, int end) {

    char *arg_end = argv[end];

    while (end != start) {
        argv[end] = argv[end - 1];
        end--;
    }
    argv[end] = arg_end;
}

int washdc_getopt(int argc, char **argv, char const *optstring) {
    static char *nextch;

    if (washdc_optind == 0) {
        nextch = NULL;
        washdc_optind = 1;
    }

    if (washdc_optind < 1 || washdc_optind >= argc || !strlen(argv[washdc_optind]))
        return -1;

    if (!nextch) {
        // find the next option.
        int next_opt_idx;
        for (next_opt_idx = washdc_optind; next_opt_idx < argc; next_opt_idx++) {
            if (strcmp(argv[next_opt_idx], "--") == 0) {
                // no more options
                washdc_optind = next_opt_idx + 1;
                return -1;
            }
            if (argv[next_opt_idx][0] == '-')
                break; // found an option
        }
        if (next_opt_idx >= argc)
            return -1; // no more options

        if (next_opt_idx != washdc_optind) {
            /*
             * we may have to advance by more than one argument to find the
             * next option.
             */
            char *cptr = argv[next_opt_idx] + 1;
            bool need_opt = false;

            /*
             * iterate through the argument to see if its an option, and if so
             * whether or not the option's value is in a separate argument.
             * If both of those conditions are true then we will also need to shift
             * the option's argument to the front
             */
            while (*cptr) {
                char *optptr;
                if ((optptr = strchr(optstring, *cptr)) && optptr[1] == ':') {
                    if (!cptr[1])
                        need_opt = true;
                    break;
                }
                cptr++;
            }
            shift_args(argc, argv, washdc_optind, next_opt_idx);
            if (need_opt) {
                if (next_opt_idx + 1 < argc && argv[next_opt_idx + 1][0] != '-') {
                    shift_args(argc, argv, washdc_optind + 1, next_opt_idx + 1);
                } else {
                    fprintf(stderr, "%s - missing option for '%c'\n", __func__, (int)*cptr);
                    washdc_optopt = *cptr;
                    nextch = NULL;
                    washdc_optind++;
                    return '?';
                }
            }
        }
        nextch = argv[washdc_optind] + 1;
    }

    char optch = *nextch;
    char const *optptr;
    if (optch && (optptr = strchr(optstring, optch))) {
        if (optptr[1] == ':') {
            if (nextch[1]) {
                washdc_optarg = ++nextch;
                washdc_optind++;
                nextch=NULL;
            } else {
                if (washdc_optind >= argc - 1 ||
                    argv[washdc_optind + 1][0] == '-') {
                    // there's nothing after the arg...
                    fprintf(stderr, "%s - missing option for '%c'\n",
                            __func__, (int)optch);
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

        return (int)optch;
    } else {
        washdc_optind++;
        nextch = NULL;
        washdc_optopt = optch;
        return '?';
    }
}
