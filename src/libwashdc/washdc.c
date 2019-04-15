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
#include "screenshot.h"
#include "hw/maple/maple_controller.h"
#include "gfx/gfx.h"
#include "gfx/gfx_config.h"
#include "title.h"
#include "washdc/win.h"
#include "hw/pvr2/pvr2.h"

static uint32_t trans_bind_washdc_to_maple(uint32_t wash);
static int trans_axis_washdc_to_maple(int axis);

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

    win_set_intf(settings->win_intf);
    gfx_set_overlay_intf(settings->overlay_intf);

    dreamcast_init(settings->path_gdi, settings->enable_cmd_tcp,
                   settings->overlay_intf);
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

int washdc_save_screenshot(char const *path) {
    return save_screenshot(path);
}

int washdc_save_screenshot_dir(void) {
    return save_screenshot_dir();
}

// mark all buttons in btns as being pressed
void washdc_controller_press_btns(unsigned port_no, uint32_t btns) {
    maple_controller_press_btns(port_no, trans_bind_washdc_to_maple(btns));
}

// mark all buttons in btns as being released
void washdc_controller_release_btns(unsigned port_no, uint32_t btns) {
    maple_controller_release_btns(port_no, trans_bind_washdc_to_maple(btns));
}

// 0 = min, 255 = max, 128 = half
void washdc_controller_set_axis(unsigned port_no, unsigned axis, unsigned val) {
    maple_controller_set_axis(port_no, trans_axis_washdc_to_maple(axis), val);
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

void washdc_gfx_toggle_filter(void) {
    gfx_toggle_output_filter();
}

static uint32_t trans_bind_washdc_to_maple(uint32_t wash) {
    uint32_t ret = 0;

    if (wash & WASHDC_CONT_BTN_C_MASK)
        ret |= MAPLE_CONT_BTN_C_MASK;
    if (wash & WASHDC_CONT_BTN_B_MASK)
        ret |= MAPLE_CONT_BTN_B_MASK;
    if (wash & WASHDC_CONT_BTN_A_MASK)
        ret |= MAPLE_CONT_BTN_A_MASK;
    if (wash & WASHDC_CONT_BTN_START_MASK)
        ret |= MAPLE_CONT_BTN_START_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_UP_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_UP_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_DOWN_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_DOWN_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_LEFT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_LEFT_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_RIGHT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_RIGHT_MASK;
    if (wash & WASHDC_CONT_BTN_Z_MASK)
        ret |= MAPLE_CONT_BTN_Z_MASK;
    if (wash & WASHDC_CONT_BTN_Y_MASK)
        ret |= MAPLE_CONT_BTN_Y_MASK;
    if (wash & WASHDC_CONT_BTN_X_MASK)
        ret |= MAPLE_CONT_BTN_X_MASK;
    if (wash & WASHDC_CONT_BTN_D_MASK)
        ret |= MAPLE_CONT_BTN_D_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_UP_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_UP_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_DOWN_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_DOWN_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_LEFT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_LEFT_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_RIGHT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_RIGHT_MASK;

    return ret;
}

static int trans_axis_washdc_to_maple(int axis) {
    switch (axis) {
    case WASHDC_CONTROLLER_AXIS_R_TRIG:
        return MAPLE_CONTROLLER_AXIS_R_TRIG;
    case WASHDC_CONTROLLER_AXIS_L_TRIG:
        return MAPLE_CONTROLLER_AXIS_L_TRIG;
    case WASHDC_CONTROLLER_AXIS_JOY1_Y:
        return MAPLE_CONTROLLER_AXIS_JOY1_Y;
    case WASHDC_CONTROLLER_AXIS_JOY2_X:
        return MAPLE_CONTROLLER_AXIS_JOY2_X;
    case WASHDC_CONTROLLER_AXIS_JOY2_Y:
        return MAPLE_CONTROLLER_AXIS_JOY2_Y;
    default:
        LOG_ERROR("unknown axis %d\n", axis);
    case WASHDC_CONTROLLER_AXIS_JOY1_X:
        return MAPLE_CONTROLLER_AXIS_JOY1_X;
    }
}

void washdc_get_pvr2_stat(struct washdc_pvr2_stat *stat) {
    struct pvr2_stat src;
    dc_get_pvr2_stats(&src);

    stat->poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE] =
        src.poly_count[DISPLAY_LIST_OPAQUE];
    stat->poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD] =
        src.poly_count[DISPLAY_LIST_OPAQUE_MOD];
    stat->poly_count[WASHDC_PVR2_POLY_GROUP_TRANS] =
        src.poly_count[DISPLAY_LIST_TRANS];
    stat->poly_count[WASHDC_PVR2_POLY_GROUP_TRANS_MOD] =
        src.poly_count[DISPLAY_LIST_TRANS_MOD];
    stat->poly_count[WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH] =
        src.poly_count[DISPLAY_LIST_PUNCH_THROUGH];
}
