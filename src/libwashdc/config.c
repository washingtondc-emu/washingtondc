/*******************************************************************************
 *
 * Copyright 2017-2019 snickerbockers
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

#include <stdbool.h>
#include <string.h>

#include "config.h"

#define CONFIG_DEF_BOOL(prop, defval)                                   \
    static bool config_ ## prop = defval;                               \
    bool config_get_ ## prop(void) {                                    \
        return config_##prop;                                           \
    }                                                                   \
    void config_set_ ## prop(bool new_val) {                            \
        config_##prop = new_val;                                        \
    }

#define CONFIG_DEF_INT(prop, defval)                                 \
    static int config_ ## prop = defval;                             \
    int config_get_ ## prop(void) {                                  \
        return config_ ## prop;                                      \
    }                                                                \
    void config_set_ ## prop(int new_val) {                          \
        config_ ## prop = new_val;                                   \
    }

#define CONFIG_DEF_STRING(prop)                                         \
    static char config_ ##prop[CONFIG_STR_LEN];                         \
    char const *config_get_ ## prop(void) {                             \
        return config_ ##prop;                                          \
    }                                                                   \
    void config_set_ ## prop(char const *new_val) {                     \
        if (new_val) {                                                  \
            strncpy(config_ ## prop, new_val,                           \
                    sizeof(char) * CONFIG_STR_LEN);                     \
            config_ ## prop[CONFIG_STR_LEN - 1] = '\0';                 \
        } else {                                                        \
            memset(config_ ## prop, 0, sizeof(config_ ## prop));        \
        }                                                               \
    }

#ifdef ENABLE_DEBUGGER
CONFIG_DEF_BOOL(dbg_enable, false)
CONFIG_DEF_BOOL(washdbg_enable, false)
#endif

CONFIG_DEF_BOOL(ser_srv_enable, false)

CONFIG_DEF_STRING(dc_bios_path);

CONFIG_DEF_STRING(dc_flash_path);

CONFIG_DEF_STRING(dc_path_rtc);

CONFIG_DEF_STRING(syscall_path);

CONFIG_DEF_INT(boot_mode, 0);

CONFIG_DEF_STRING(gdi_image);

CONFIG_DEF_STRING(ip_bin_path);

CONFIG_DEF_STRING(exec_bin_path);

CONFIG_DEF_BOOL(enable_cmd_tcp, false);

CONFIG_DEF_BOOL(jit, false);

#ifdef ENABLE_JIT_X86_64
CONFIG_DEF_BOOL(native_jit, false);
#endif

CONFIG_DEF_BOOL(inline_mem, true);

CONFIG_DEF_BOOL(log_verbose, false);
CONFIG_DEF_BOOL(log_stdout, false);

CONFIG_DEF_BOOL(dump_mem_on_error, false)
