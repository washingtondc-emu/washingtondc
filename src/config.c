/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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
