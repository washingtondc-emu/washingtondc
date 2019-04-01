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

#ifndef LIBWASHDC_H_
#define LIBWASHDC_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct washdc_launch_settings;

/*
 * gdi_path is a path to the GDI image to mount, or NULL to boot with nothing
 * in the disc drive.
 * win_width and win_height are window dimensions
 * cmd_session should be true if the remote command prompt is enabled.
 */
void washdc_init(struct washdc_launch_settings const *settings);

void washdc_cleanup();

void washdc_run();

enum washdc_boot_mode {
    // standard boot into firmware
    WASHDC_BOOT_FIRMWARE,

    // boot directly to IP.BIN and then continue into 1st_read.bin
    WASHDC_BOOT_IP_BIN,

    // skip the firmware and IP.BIN and boot directly into 1st_read.bin
    WASHDC_BOOT_DIRECT
};

struct washdc_launch_settings {
    char const *path_ip_bin;
    char const *path_1st_read_bin;
    char const *path_syscalls_bin;
    char const *path_dc_bios;
    char const *path_dc_flash;
    char const *path_gdi;

    enum washdc_boot_mode boot_mode;

    bool log_to_stdout;
    bool log_verbose;
#ifdef ENABLE_DEBUGGER
    bool dbg_enable;
    bool washdbg_enable;
#endif
    bool inline_mem;
    bool enable_jit;
#ifdef ENABLE_JIT_X86_64
    bool enable_native_jit;
#endif
    bool enable_cmd_tcp;
    bool cmd_session;
    bool enable_serial;
};

#ifdef __cplusplus
}
#endif

#endif
