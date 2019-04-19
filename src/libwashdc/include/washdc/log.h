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

#ifndef WASHDC_LOG_H_
#define WASHDC_LOG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

enum washdc_log_severity {
    washdc_log_severity_debug,
    washdc_log_severity_info,
    washdc_log_severity_warn,
    washdc_log_severity_error
};

void washdc_log(enum washdc_log_severity severity,
                char const *fmt, va_list args);

static inline void washdc_log_debug(char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    washdc_log(washdc_log_severity_debug, fmt, args);
    va_end(args);
}

static inline void washdc_log_info(char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    washdc_log(washdc_log_severity_info, fmt, args);
    va_end(args);
}

static inline void washdc_log_warn(char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    washdc_log(washdc_log_severity_warn, fmt, args);
    va_end(args);
}

static inline void washdc_log_error(char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    washdc_log(washdc_log_severity_error, fmt, args);
    va_end(args);
}

#ifdef __cplusplus
}
#endif

#endif
