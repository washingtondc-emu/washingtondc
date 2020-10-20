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

// path to the IP.BIN file
CONFIG_DECL_STRING(ip_bin_path);

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
