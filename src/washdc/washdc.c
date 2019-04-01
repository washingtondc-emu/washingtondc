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

#include "washdc/washdc.h"

#include "config.h"
#include "dreamcast.h"

static enum dc_boot_mode translate_boot_mode(enum washdc_boot_mode mode) {
    switch (mode) {
    case WASHDC_BOOT_FIRMWARE:
        return DC_BOOT_FIRMWARE;
    case WASHDC_BOOT_DIRECT:
        return DC_BOOT_DIRECT;
    default:
    case WASHDC_BOOT_IP_BIN:
        return DC_BOOT_IP_BIN;
    }
}

void washdc_init(struct washdc_launch_settings const *settings) {
    config_set_log_stdout(settings->log_to_stdout);
    config_set_log_verbose(settings->log_verbose);
#ifdef ENABLE_DEBUGGER
    config_set_dbg_enable(settings->dbg_enable);
    config_set_washdbg_enable(settings->washdbg_enable);
#endif
    config_set_inline_mem(settings->inline_mem);
    config_set_jit(settings->enable_jit);
#ifdef ENABLE_JIT_X86_64
    config_set_native_jit(settings->enable_native_jit);
#endif
    config_set_boot_mode(translate_boot_mode(settings->boot_mode));
    config_set_ip_bin_path(settings->path_ip_bin);
    config_set_exec_bin_path(settings->path_1st_read_bin);
    config_set_syscall_path(settings->path_syscalls_bin);
    config_set_dc_bios_path(settings->path_dc_bios);
    config_set_dc_flash_path(settings->path_dc_flash);
    config_set_enable_cmd_tcp(settings->enable_cmd_tcp);
    config_set_ser_srv_enable(settings->enable_serial);

    dreamcast_init(settings->path_gdi, settings->enable_cmd_tcp);
}

void washdc_cleanup() {
    dreamcast_cleanup();
}

void washdc_run() {
    dreamcast_run();
}
