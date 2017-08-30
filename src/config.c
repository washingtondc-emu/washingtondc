/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"

#define CONFIG_DEF_BOOL(prop)                                           \
    static atomic_bool config_ ## prop = ATOMIC_VAR_INIT(false);        \
    bool config_get_ ## prop(void) {                                    \
        return atomic_load_explicit(&config_ ## prop,                   \
                                    memory_order_relaxed);              \
    }                                                                   \
    void config_set_ ## prop(bool new_val) {                            \
        atomic_store_explicit(&config_ ## prop, new_val,                \
                              memory_order_relaxed);                    \
    }

#define CONFIG_DEF_INT(prop)                                    \
    static atomic_int config_ ## prop = ATOMIC_VAR_INIT(0);     \
    int config_get_ ## prop(void) {                             \
        return atomic_load_explicit(&config_ ## prop,           \
                                    memory_order_relaxed);      \
    }                                                           \
    void config_set_ ## prop(int new_val) {                     \
        atomic_store_explicit(&config_ ## prop, new_val,        \
                              memory_order_relaxed);            \
    }

#define CONFIG_DEF_STRING(prop)                                         \
    static char config_ ##prop[CONFIG_STR_LEN];                         \
    char const *config_get_ ## prop(void) {                             \
        atomic_thread_fence(memory_order_acquire);                      \
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
        atomic_thread_fence(memory_order_release);                      \
    }

#ifdef ENABLE_DEBUGGER
CONFIG_DEF_BOOL(dbg_enable)
#endif

CONFIG_DEF_BOOL(ser_srv_enable)

CONFIG_DEF_STRING(dc_bios_path);

CONFIG_DEF_STRING(dc_flash_path);

CONFIG_DEF_STRING(syscall_path);

CONFIG_DEF_INT(boot_mode);

CONFIG_DEF_STRING(gdi_image);

CONFIG_DEF_STRING(ip_bin_path);

CONFIG_DEF_STRING(exec_bin_path);

CONFIG_DEF_BOOL(enable_cmd_tcp);

CONFIG_DEF_BOOL(hack_power_stone_no_aica);
