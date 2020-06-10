/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#include <stdio.h>
#include <stdarg.h>

#include "washdc/washdc.h"

#include "config.h"
#include "dreamcast.h"
#include "screenshot.h"
#include "hw/maple/maple_controller.h"
#include "hw/maple/maple_keyboard.h"
#include "gfx/gfx.h"
#include "gfx/gfx_config.h"
#include "title.h"
#include "washdc/win.h"
#include "hw/pvr2/pvr2.h"
#include "log.h"
#include "washdc/config_file.h"

static struct washdc_hostfile_api const *hostfile_api;

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

struct washdc_gameconsole const*
washdc_init(struct washdc_launch_settings const *settings) {
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
    config_set_ser_srv_enable(settings->enable_serial);
    config_set_dc_path_rtc(settings->path_rtc);

    win_set_intf(settings->win_intf);
    gfx_set_overlay_intf(settings->overlay_intf);

    hostfile_api = settings->hostfile_api;

    return dreamcast_init(settings->path_gdi,
                          settings->gfx_rend_if,
                          settings->overlay_intf, settings->dbg_intf,
                          settings->sersrv, settings->sndsrv,
                          settings->write_to_flash);
}

void washdc_cleanup() {
    dreamcast_cleanup();
}

void washdc_run() {
    dreamcast_run();
}

void washdc_kill(void) {
    dreamcast_kill();
}

bool washdc_is_running(void) {
    return dc_is_running();
}

int washdc_save_screenshot(char const *path) {
    return save_screenshot(path);
}

int washdc_save_screenshot_dir(void) {
    return save_screenshot_dir();
}

// mark all buttons in btns as being pressed
void washdc_controller_press_btns(unsigned port_no, uint32_t btns) {
    dc_controller_press_buttons(port_no, btns);
}

// mark all buttons in btns as being released
void washdc_controller_release_btns(unsigned port_no, uint32_t btns) {
    dc_controller_release_buttons(port_no, btns);
}

void
washdc_keyboard_set_btn(unsigned port_no, unsigned btn_no, bool is_pressed) {
    dc_keyboard_set_key(port_no, btn_no, is_pressed);
}

void
washdc_keyboard_press_special(unsigned port_no,
                              enum washdc_keyboard_special_keys which) {
    dc_keyboard_press_special(port_no, which);
}

void
washdc_keyboard_release_special(unsigned port_no,
                                enum washdc_keyboard_special_keys which) {
    dc_keyboard_release_special(port_no, which);
}

// 0 = min, 255 = max, 128 = half
void washdc_controller_set_axis(unsigned port_no, unsigned axis, unsigned val) {
    dc_controller_set_axis(port_no, axis, val);
}

void washdc_on_expose(void) {
    gfx_expose();
}

void washdc_on_resize(int xres, int yres) {
    gfx_resize(xres, yres);
}

char const *washdc_win_get_title(void) {
    return title_get();
}

void washdc_gfx_toggle_wireframe(void) {
    gfx_config_toggle_wireframe();
}

void washdc_get_pvr2_stat(struct washdc_pvr2_stat *stat) {
    struct pvr2_stat src;
    dc_get_pvr2_stats(&src);

    stat->vert_count[WASHDC_PVR2_POLY_GROUP_OPAQUE] =
        src.per_frame_counters.vert_count[PVR2_POLY_TYPE_OPAQUE];
    stat->vert_count[WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD] =
        src.per_frame_counters.vert_count[PVR2_POLY_TYPE_OPAQUE_MOD];
    stat->vert_count[WASHDC_PVR2_POLY_GROUP_TRANS] =
        src.per_frame_counters.vert_count[PVR2_POLY_TYPE_TRANS];
    stat->vert_count[WASHDC_PVR2_POLY_GROUP_TRANS_MOD] =
        src.per_frame_counters.vert_count[PVR2_POLY_TYPE_TRANS_MOD];
    stat->vert_count[WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH] =
        src.per_frame_counters.vert_count[PVR2_POLY_TYPE_PUNCH_THROUGH];

    stat->tex_xmit_count = src.persistent_counters.tex_xmit_count;
    stat->tex_invalidate_count = src.persistent_counters.tex_invalidate_count;
    stat->pal_tex_invalidate_count =
        src.persistent_counters.pal_tex_invalidate_count;
    stat->texture_overwrite_count =
        src.persistent_counters.texture_overwrite_count;
    stat->fresh_texture_upload_count =
        src.persistent_counters.fresh_texture_upload_count;
    stat->tex_eviction_count =
        src.persistent_counters.tex_eviction_count;
}

void washdc_pause(void) {
    dc_request_frame_stop();
}

void washdc_resume(void) {
    dc_state_transition(DC_STATE_RUNNING, DC_STATE_SUSPEND);
}

bool washdc_is_paused(void) {
    return dc_get_state() == DC_STATE_SUSPEND;
}

void washdc_run_one_frame(void) {
    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_SUSPEND) {
        dc_request_frame_stop();
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_SUSPEND);
    } else {
        LOG_ERROR("%s - cannot run one frame becase emulator state is not "
                  "suspended\n", __func__);
    }
}

unsigned washdc_get_frame_count(void) {
    return dc_get_frame_count();
}

washdc_hostfile washdc_hostfile_open(char const *path,
                                     enum washdc_hostfile_mode mode) {
    return hostfile_api->open(path, mode);
}

void washdc_hostfile_close(washdc_hostfile file) {
    hostfile_api->close(file);
}

int washdc_hostfile_seek(washdc_hostfile file, long disp,
                         enum washdc_hostfile_seek_origin origin) {
    return hostfile_api->seek(file, disp, origin);
}

long washdc_hostfile_tell(washdc_hostfile file) {
    return hostfile_api->tell(file);
}

size_t washdc_hostfile_read(washdc_hostfile file, void *outp, size_t len) {
    return hostfile_api->read(file, outp, len);
}

size_t washdc_hostfile_write(washdc_hostfile file, void const *inp, size_t len) {
    return hostfile_api->write(file, inp, len);
}

int washdc_hostfile_flush(washdc_hostfile file) {
    return hostfile_api->flush(file);
}

int washdc_hostfile_putc(washdc_hostfile file, char ch) {
    if (hostfile_api->write(file, &ch, sizeof(ch)) == sizeof(ch))
        return ch;
    return WASHDC_HOSTFILE_EOF;
}

int washdc_hostfile_puts(washdc_hostfile file, char const *str) {
    int n_chars = 0;
    while (*str) {
        if (washdc_hostfile_putc(file, *str++) == WASHDC_HOSTFILE_EOF)
            return WASHDC_HOSTFILE_EOF;
        n_chars++;
    }
    return n_chars;
}

int washdc_hostfile_getc(washdc_hostfile file) {
    char ch;

    if (washdc_hostfile_read(file, &ch, sizeof(ch)) != sizeof(ch))
        return WASHDC_HOSTFILE_EOF;
    return ch;
}

void washdc_hostfile_printf(washdc_hostfile file, char const *fmt, ...) {
    static char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';

    washdc_hostfile_puts(file, buf);
}

washdc_hostfile washdc_hostfile_open_cfg_file(enum washdc_hostfile_mode mode) {
    if (hostfile_api->open_cfg_file)
        return hostfile_api->open_cfg_file(mode);
    return WASHDC_HOSTFILE_INVALID;
}

washdc_hostfile washdc_hostfile_open_screenshot(char const *name,
                                                enum washdc_hostfile_mode mode) {
    if (hostfile_api->open_screenshot)
        return hostfile_api->open_screenshot(name, mode);
    return WASHDC_HOSTFILE_INVALID;
}

char washdc_hostfile_pathsep(void) {
    return hostfile_api->pathsep;
}

enum washdc_controller_tp washdc_controller_type(unsigned port_no) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "wash.dc.port.%u.0", port_no);
    tmp[sizeof(tmp) - 1] = '\0';
    char const *tpstr = cfg_get_node(tmp);
    if (tpstr) {
        if (strcmp(tpstr, "dreamcast_controller") == 0)
            return WASHDC_CONTROLLER_TP_DREAMCAST_CONTROLLER;
        else if (strcmp(tpstr, "dreamcast_keyboard_us") == 0)
            return WASHDC_CONTROLLER_TP_DREAMCAST_KEYBOARD;
    }
    return WASHDC_CONTROLLER_TP_NONE;
}
