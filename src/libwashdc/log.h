/*******************************************************************************
 *
 * Copyright 2017, 2019 snickerbockers
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

#ifndef LOG_H_
#define LOG_H_

#include <stdbool.h>

#define ENABLE_LOG_ERROR
#define ENABLE_LOG_WARN
#define ENABLE_LOG_INFO

enum log_severity {
    log_severity_debug,
    log_severity_info,
    log_severity_warn,
    log_severity_error
};

// imminent problem that will impact WashingtonDC's operation
#ifdef ENABLE_LOG_ERROR
#define LOG_ERROR(msg, ...) log_do_write(log_severity_error, msg, ##__VA_ARGS__)
#else
#define LOG_ERROR(msg, ...)
#endif

// something that should be noted but probably isn't too important
#ifdef ENABLE_LOG_WARN
#define LOG_WARN(msg, ...) log_do_write(log_severity_warn, msg, ##__VA_ARGS__)
#else
#define LOG_WARN(msg, ...)
#endif

// used to communicate information to the user
#ifdef ENABLE_LOG_INFO
#define LOG_INFO(msg, ...) log_do_write(log_severity_info, msg, ##__VA_ARGS__)
#else
#define LOG_INFO(msg, ...)
#endif

// catch-all for most of the useless crap WashingtonDC dumps to stdout
#ifdef ENABLE_LOG_DEBUG
#define LOG_DBG(msg, ...) log_do_write(log_severity_debug, msg, ##__VA_ARGS__)
#else
#define LOG_DBG(msg, ...)
#endif

void log_do_write(enum log_severity lvl, char const *fmt, ...);

void log_init(bool to_stdout, bool verbose);
void log_flush(void);
void log_cleanup(void);

#endif
