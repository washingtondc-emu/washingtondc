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
#endif

// if true, enable the TCP/IP serial server
CONFIG_DECL_BOOL(ser_srv_enable);

// if true, enable the TCP/IP remote cli frontend
CONFIG_DECL_BOOL(enable_cmd_tcp);

// path to the dreamcast bios file
CONFIG_DECL_STRING(dc_bios_path);

// path to the dreamcast flash image
CONFIG_DECL_STRING(dc_flash_path);

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

/*
 * if true, send a fake response from the AICA's CPU to fool the game into
 * thinking the AICA CPU is working.  This only applies to Power Stone,
 * obviously.
 */
CONFIG_DECL_BOOL(hack_power_stone_no_aica);

#endif
