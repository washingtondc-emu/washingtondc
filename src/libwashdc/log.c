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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"
#include "washdc/log.h"
#include "washdc/hostfile.h"

static washdc_hostfile logfile;
static bool also_stdout;
static bool verbose_mode;

static void log_do_write_vararg(enum log_severity lvl,
                                char const *fmt, va_list args);

void log_init(bool to_stdout, bool verbose) {
    logfile =
        washdc_hostfile_open("wash.log",
                             WASHDC_HOSTFILE_WRITE | WASHDC_HOSTFILE_TEXT);
    also_stdout = to_stdout;
    verbose_mode = verbose;
}

void log_cleanup(void) {
    washdc_hostfile_close(logfile);
    logfile = NULL;
}

void log_flush(void) {
    washdc_hostfile_flush(logfile);
}

void log_do_write(enum log_severity lvl, char const *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_do_write_vararg(lvl, fmt, args);
    va_end(args);
}

static void log_do_write_vararg(enum log_severity lvl,
                                char const *fmt, va_list args) {
    static char buf[1024];
    if (verbose_mode || lvl >= log_severity_info) {
        va_list args2;
        va_copy(args2, args);
        vsnprintf(buf, sizeof(buf), fmt, args);
        buf[sizeof(buf) - 1] = '\0';
        washdc_hostfile_write(logfile, buf, strlen(buf));

        if (also_stdout || lvl >= log_severity_error)
            vprintf(fmt, args2);
        va_end(args2);
    }
}

void washdc_log(enum washdc_log_severity severity,
                char const *fmt, va_list args) {
    enum log_severity lvl;
    switch (severity) {
    case washdc_log_severity_debug:
        lvl = log_severity_debug;
        break;
    case washdc_log_severity_info:
        lvl = log_severity_info;
        break;
    case washdc_log_severity_warn:
        lvl = log_severity_warn;
        break;
    default:
    case washdc_log_severity_error:
        lvl = log_severity_error;
    }

    log_do_write_vararg(lvl, fmt, args);
}
