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
