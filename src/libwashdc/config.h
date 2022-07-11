/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019, 2022 snickerbockers
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

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

#define CONFIG_DECL_BOOL(prop)                  \
    bool config_get_ ## prop(void);             \
    void config_set_ ## prop(bool new_val)

#define CONFIG_DECL_INT(prop)                   \
    int config_get_ ## prop(void);              \
    void config_set_ ## prop(int new_val)

#define CONFIG_STR_LEN 256
#define CONFIG_DECL_STRING(prop)                        \
    char const *config_get_ ## prop(void);              \
    void config_set_ ## prop(char const *new_val)

#ifdef ENABLE_DEBUGGER
// if true, enable the remote GDB debugger
CONFIG_DECL_BOOL(dbg_enable);

/*
 * if true, use washdbg.  If false, use GDB instead.
 *
 * If this is true then dbg_enable should also be true.
 */
CONFIG_DECL_BOOL(washdbg_enable);
#endif

// if true, enable the TCP/IP serial server
CONFIG_DECL_BOOL(ser_srv_enable);

// if true, enable the TCP/IP remote cli frontend
CONFIG_DECL_BOOL(enable_cmd_tcp);

// path to the dreamcast bios file
CONFIG_DECL_STRING(dc_bios_path);

// path to the dreamcast flash image
CONFIG_DECL_STRING(dc_flash_path);

// path to the rtc image
CONFIG_DECL_STRING(dc_path_rtc);

// path to the syscalls.bin system call image
CONFIG_DECL_STRING(syscall_path);

// if true, then direct-boot mode has been enabled
CONFIG_DECL_INT(boot_mode);

// path to the disc.gdi file
CONFIG_DECL_STRING(gdi_image);

// path to the 1st_read.bin file
CONFIG_DECL_STRING(exec_bin_path);

// enable the dynamic recompiler, or disable it to use the interpreter
CONFIG_DECL_BOOL(jit);

#ifdef ENABLE_JIT_X86_64
/*
 * enable the x86_64 backend to the dynamic recompiler.
 * if this is enabled and the jit option is not enabled, then this option will
 * override the jit option and the jit will still be enabled.
 *
 * If the jit option is enabled and this is not enabled, then the jit's
 * platform-independent interpreter backend will be used.
 */
CONFIG_DECL_BOOL(native_jit);
#endif

/*
 * if this is set (default is true) then the jit's x86_64 backend will
 * inline memory accesses.
 */
CONFIG_DECL_BOOL(inline_mem);

CONFIG_DECL_BOOL(log_stdout);
CONFIG_DECL_BOOL(log_verbose);

CONFIG_DECL_BOOL(dump_mem_on_error);

#endif
