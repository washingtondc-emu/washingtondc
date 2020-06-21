/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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
