/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#ifndef REAL_TICKS_H_
#define REAL_TICKS_H_

/*
 * The functions and structures defined in this file refer to the passage of
 * time in the host environment, NOT the guest environment.  Do not use it for
 * emulation purposes.
 */

#ifdef _WIN32

#include "i_hate_windows.h"

typedef ULONGLONG washdc_real_time;

static void washdc_get_real_time(washdc_real_time *time) {
    *time = GetTickCount64();
}

static void
washdc_real_time_diff(washdc_real_time *delta, washdc_real_time const *end,
                      washdc_real_time const *start) {
    *delta = *end - *start;
}

static double washdc_real_time_to_seconds(washdc_real_time const *in) {
    return *in / 1000.0;
}

static void washdc_real_time_from_seconds(washdc_real_time *outp,
                                          double seconds) {
    *outp = seconds * 1000.0;
}

#else

#include <time.h>
#include <math.h>

typedef struct timespec washdc_real_time;

static void washdc_get_real_time(washdc_real_time *time) {
    clock_gettime(CLOCK_MONOTONIC, time);
}

static void
washdc_real_time_diff(washdc_real_time *delta, washdc_real_time const *end,
                      washdc_real_time const *start) {
    /* subtract delta_time = end_time - start_time */
    if (end->tv_nsec < start->tv_nsec) {
        delta->tv_nsec = 1000000000 - start->tv_nsec + end->tv_nsec;
        delta->tv_sec = end->tv_sec - 1 - start->tv_sec;
    } else {
        delta->tv_nsec = end->tv_nsec - start->tv_nsec;
        delta->tv_sec = end->tv_sec - start->tv_sec;
    }
}

static double washdc_real_time_to_seconds(washdc_real_time const *in) {
    return in->tv_sec + ((double)in->tv_nsec) / 1000000000.0;
}

static void washdc_real_time_from_seconds(washdc_real_time *outp,
                                          double seconds) {
    double int_part;
    double frac_part = modf(seconds, &int_part);

    outp->tv_sec = int_part;
    outp->tv_nsec = frac_part * 1000000000.0;
}

#endif

#endif
