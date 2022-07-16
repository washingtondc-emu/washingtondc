/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020, 2022 snickerbockers
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
 * This is missing the -- and the non-option argument shifting that GNU does.
 *
 * to reset the state of washdc_getopt, set washdc_optind to 0.  This is also
 * how you reset GNU getopt.  posix getopt requires you to set it to 1.
 */

extern char *washdc_optarg;
extern int washdc_optind, washdc_opterr, washdc_optopt;

int washdc_getopt(int argc, char **argv, char const *optstring);

#ifdef __cplusplus
}
#endif

#endif
