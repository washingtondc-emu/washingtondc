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

#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "washdc/log.h"

static FILE *logfile;
static bool also_stdout;
static bool verbose_mode;

static void log_do_write_vararg(enum log_severity lvl,
                                char const *fmt, va_list args);

void log_init(bool to_stdout, bool verbose) {
    logfile = fopen("wash.log", "w");
    also_stdout = to_stdout;
    verbose_mode = verbose;
}

void log_cleanup(void) {
    fclose(logfile);
    logfile = NULL;
}

void log_flush(void) {
    fflush(logfile);
}

void log_do_write(enum log_severity lvl, char const *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_do_write_vararg(lvl, fmt, args);
    va_end(args);
}

static void log_do_write_vararg(enum log_severity lvl,
                                char const *fmt, va_list args) {
    if (verbose_mode || lvl >= log_severity_info) {
        va_list args2;
        va_copy(args2, args);
        vfprintf(logfile, fmt, args);

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
