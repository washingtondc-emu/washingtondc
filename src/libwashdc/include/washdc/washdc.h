/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020, 2022 snickerbockers
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
#include <stdint.h>

#include "sound_intf.h"
#include "gameconsole.h"
#include "hostfile.h"
#include "washdc/gfx/gfx_all.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WASHDC_PATH_LEN 4096

struct washdc_launch_settings;

/*
 * gdi_path is a path to the GDI image to mount, or NULL to boot with nothing
 * in the disc drive.
 * win_width and win_height are window dimensions
 * cmd_session should be true if the remote command prompt is enabled.
 */
struct washdc_gameconsole const*
washdc_init(struct washdc_launch_settings const *settings);

void washdc_cleanup();

void washdc_run();

void washdc_kill(void);

bool washdc_is_running(void);

enum washdc_boot_mode {
    // standard boot into firmware
    WASHDC_BOOT_FIRMWARE,

    // skip the firmware and IP.BIN and boot directly into 1st_read.bin
    WASHDC_BOOT_DIRECT
};

struct win_intf;

#define WASHDC_CONTROLLER_PORTS 4
#define WASHDC_CONTROLLER_UNITS 3

enum washdc_controller_tp {
    WASHDC_CONTROLLER_TP_INVALID,
    WASHDC_CONTROLLER_TP_CONTROLLER,
    WASHDC_CONTROLLER_TP_KEYBOARD_US,
    WASHDC_CONTROLLER_TP_PURUPURU,
    WASHDC_CONTROLLER_TP_VMU
};

struct washdc_controller_dev {
    enum washdc_controller_tp tp;

    // path to the VMU image; only valid when tp == WASHDC_CONTROLLER_TP_VMU
    char image_path[WASHDC_PATH_LEN];
};

struct washdc_launch_settings {
    char const *path_1st_read_bin;
    char const *path_dc_bios;
    char const *path_dc_flash;
    char const *path_gdi;
    char const *path_rtc;

    struct washdc_hostfile_api const *hostfile_api;

    /*
     * if this is not WASHDC_HOSTFILE_INVALID, then proxies will be inserted
     * between the CPU and the PowerVR2 to monitor all memory and register
     * writes and log them to this file.
     */
    washdc_hostfile pvr2_trace_file, aica_trace_file;

    struct win_intf const *win_intf;

    // only valid if dbg_enable is true
    struct debug_frontend const *dbg_intf;

    struct serial_server_intf const *sersrv;

    struct washdc_sound_intf const *sndsrv;

    struct gfx_rend_if const *gfx_rend_if;

    enum washdc_boot_mode boot_mode;

    bool log_to_stdout;
    bool log_verbose;
    /* #ifdef ENABLE_DEBUGGER */
    bool dbg_enable;
    bool washdbg_enable;
    /* #endif */
    bool inline_mem;
    bool enable_jit;
    /* #ifdef ENABLE_JIT_X86_64 */
    bool enable_native_jit;
    /* #endif */
    bool cmd_session;
    bool enable_serial;

    // if true, the flash image will be written out at the end
    bool write_to_flash;

    bool dump_mem_on_error;

    struct washdc_controller_dev controllers[WASHDC_CONTROLLER_PORTS][WASHDC_CONTROLLER_UNITS];
};

int washdc_save_screenshot(char const *path);
int washdc_save_screenshot_dir(void);

char const *washdc_win_get_title(void);

void washdc_gfx_toggle_wireframe(void);

#define WASHDC_CONT_BTN_C_SHIFT 0
#define WASHDC_CONT_BTN_C_MASK (1 << WASHDC_CONT_BTN_C_SHIFT)

#define WASHDC_CONT_BTN_B_SHIFT 1
#define WASHDC_CONT_BTN_B_MASK (1 << WASHDC_CONT_BTN_B_SHIFT)

#define WASHDC_CONT_BTN_A_SHIFT 2
#define WASHDC_CONT_BTN_A_MASK (1 << WASHDC_CONT_BTN_A_SHIFT)

#define WASHDC_CONT_BTN_START_SHIFT 3
#define WASHDC_CONT_BTN_START_MASK (1 << WASHDC_CONT_BTN_START_SHIFT)

#define WASHDC_CONT_BTN_DPAD_UP_SHIFT 4
#define WASHDC_CONT_BTN_DPAD_UP_MASK (1 << WASHDC_CONT_BTN_DPAD_UP_SHIFT)

#define WASHDC_CONT_BTN_DPAD_DOWN_SHIFT 5
#define WASHDC_CONT_BTN_DPAD_DOWN_MASK (1 << WASHDC_CONT_BTN_DPAD_DOWN_SHIFT)

#define WASHDC_CONT_BTN_DPAD_LEFT_SHIFT 6
#define WASHDC_CONT_BTN_DPAD_LEFT_MASK (1 << WASHDC_CONT_BTN_DPAD_LEFT_SHIFT)

#define WASHDC_CONT_BTN_DPAD_RIGHT_SHIFT 7
#define WASHDC_CONT_BTN_DPAD_RIGHT_MASK (1 << WASHDC_CONT_BTN_DPAD_RIGHT_SHIFT)

#define WASHDC_CONT_BTN_Z_SHIFT 8
#define WASHDC_CONT_BTN_Z_MASK (1 << WASHDC_CONT_BTN_Z_SHIFT)

#define WASHDC_CONT_BTN_Y_SHIFT 9
#define WASHDC_CONT_BTN_Y_MASK (1 << WASHDC_CONT_BTN_Y_SHIFT)

#define WASHDC_CONT_BTN_X_SHIFT 10
#define WASHDC_CONT_BTN_X_MASK (1 << WASHDC_CONT_BTN_X_SHIFT)

#define WASHDC_CONT_BTN_D_SHIFT 11
#define WASHDC_CONT_BTN_D_MASK (1 << WASHDC_CONT_BTN_D_SHIFT)

#define WASHDC_CONT_BTN_DPAD2_UP_SHIFT 12
#define WASHDC_CONT_BTN_DPAD2_UP_MASK (1 << WASHDC_CONT_BTN_DPAD2_UP_SHIFT)

#define WASHDC_CONT_BTN_DPAD2_DOWN_SHIFT 13
#define WASHDC_CONT_BTN_DPAD2_DOWN_MASK (1 << WASHDC_CONT_BTN_DPAD2_DOWN_SHIFT)

#define WASHDC_CONT_BTN_DPAD2_LEFT_SHIFT 14
#define WASHDC_CONT_BTN_DPAD2_LEFT_MASK (1 << WASHDC_CONT_BTN_DPAD2_LEFT_SHIFT)

#define WASHDC_CONT_BTN_DPAD2_RIGHT_SHIFT 15
#define WASHDC_CONT_BTN_DPAD2_RIGHT_MASK (1 << WASHDC_CONT_BTN_DPAD2_RIGHT_SHIFT)

enum {
    WASHDC_CONTROLLER_AXIS_R_TRIG,
    WASHDC_CONTROLLER_AXIS_L_TRIG,
    WASHDC_CONTROLLER_AXIS_JOY1_X,
    WASHDC_CONTROLLER_AXIS_JOY1_Y,
    WASHDC_CONTROLLER_AXIS_JOY2_X,
    WASHDC_CONTROLLER_AXIS_JOY2_Y,

    WASHDC_CONTROLLER_N_AXES
};

enum washdc_keyboard_special_keys {
    WASHDC_KEYBOARD_NONE = 0,
    WASHDC_KEYBOARD_LEFT_CTRL = 1,
    WASHDC_KEYBOARD_LEFT_SHIFT = 2,
    WASHDC_KEYBOARD_LEFT_ALT = 4,
    WASHDC_KEYBOARD_S1 = 8,
    WASHDC_KEYBOARD_RIGHT_CTRL = 16,
    WASHDC_KEYBOARD_RIGHT_SHIFT = 32,
    WASHDC_KEYBOARD_RIGHT_ALT = 64,
    WASHDC_KEYBOARD_S2 = 128
};

// mark all buttons in btns as being pressed
void washdc_controller_press_btns(unsigned port_no, uint32_t btns);

// mark all buttons in btns as being released
void washdc_controller_release_btns(unsigned port_no, uint32_t btns);

void
washdc_keyboard_set_btn(unsigned port_no, unsigned btn_no, bool is_pressed);

void
washdc_keyboard_press_special(unsigned port_no,
                              enum washdc_keyboard_special_keys which);
void
washdc_keyboard_release_special(unsigned port_no,
                                enum washdc_keyboard_special_keys which);

// 0 = min, 255 = max, 128 = half
void washdc_controller_set_axis(unsigned port_no, unsigned axis, unsigned val);

enum washdc_pvr2_poly_group {
    WASHDC_PVR2_POLY_GROUP_OPAQUE,
    WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD,
    WASHDC_PVR2_POLY_GROUP_TRANS,
    WASHDC_PVR2_POLY_GROUP_TRANS_MOD,
    WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH,

    WASHDC_PVR2_POLY_GROUP_COUNT
};

struct washdc_pvr2_stat {
    unsigned vert_count[WASHDC_PVR2_POLY_GROUP_COUNT];

    /*
     * number of times textures get transmitted to the gfx infra.
     * this includes both overwritten textures and new textures that aren't
     * overwriting anything that already exists.
     */
    unsigned tex_xmit_count;

    // number of times (non-paletted) textures get invalidated
    unsigned tex_invalidate_count;

    /*
     * number of times paletted textures get invalidated
     *
     * the reason why this is separate from tex_invalidate_count is that this
     * type of overwrite is done through a different code-path so it makes
     * sense to track them separately.  Otherwise they are redundant.
     */
    unsigned pal_tex_invalidate_count;

    /*
     * number of times a texture gets kicked out of the cache to make room
     * for another one
     */
    unsigned texture_overwrite_count;

    /*
     * number of times a new texture gets uploaded into an empty slot in
     * the texture cache.
     */
    unsigned fresh_texture_upload_count;

    /*
     * number of times a texture got kicked out of the cache because it got
     * invalidated but it wasn't immediately needed so we just ignored it.
     *
     * For the sake of simplicity, this counter is included in the
     * tex_xmit_count even though it probably shouldn't since the texture
     * doesn't get transmitted.  It also overlaps with tex_invalidate_count
     * since that's generally how textures end up in this situation.
     */
    unsigned tex_eviction_count;
};

void washdc_get_pvr2_stat(struct washdc_pvr2_stat *stat);

void washdc_pause(void);
void washdc_resume(void);
bool washdc_is_paused(void);
void washdc_run_one_frame(void);

unsigned washdc_get_frame_count(void);

double washdc_get_fps(void);
double washdc_get_virt_fps(void);

#ifdef __cplusplus
}
#endif

#endif
