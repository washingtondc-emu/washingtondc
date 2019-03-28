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

static FILE *logfile;
static bool also_stdout;

void log_init(bool to_stdout) {
    logfile = fopen("wash.log", "w");
    also_stdout = to_stdout;
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
    vfprintf(logfile, fmt, args);
    va_end(args);

    if (also_stdout || lvl >= log_severity_error) {
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}
