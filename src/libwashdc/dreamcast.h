/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2020, 2022 snickerbockers
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

#ifndef DREAMCAST_H_
#define DREAMCAST_H_

#include <stdbool.h>

#include "memory.h"
#include "hw/sh4/sh4.h"
#include "dc_sched.h"
#include "washdc/serial_server.h"
#include "washdc/gameconsole.h"
#include "washdc/gfx/gfx_all.h"
#include "washdc/washdc.h"

#ifdef ENABLE_DEBUGGER
#include "washdc/debugger.h"
#endif

extern struct dc_clock sh4_clock;

#define ADDR_1ST_READ_BIN  0x8c010000
#define ADDR_BOOTSTRAP     0x8c008300
#define ADDR_SYSCALLS      0x8c000000
#define LEN_SYSCALLS           0x8000

/*
 * gdi_path is a path to the GDI image to mount, or NULL to boot with nothing
 * in the disc drive.
 * win_width and win_height are window dimensions
 * cmd_session should be true if the remote command prompt is enabled.
 */
struct washdc_sound_intf;

struct washdc_gameconsole const*
dreamcast_init(char const *gdi_path,
               struct gfx_rend_if const *gfx_if,
               struct debug_frontend const *dbg_frontend,
               struct serial_server_intf const *ser_intf,
               struct washdc_sound_intf const *snd_intf,
               bool flash_mem_writeable,
               washdc_hostfile pvr2_trace_file,
               washdc_hostfile aica_trace_file,
               struct washdc_controller_dev const controllers[4][3]);

void dreamcast_cleanup();

void dreamcast_run();

/*
 * Kill the emulator.  This function can be safely called
 * from any thread.
 */
void dreamcast_kill(void);

Sh4 *dreamcast_get_cpu();

void dc_print_perf_stats(void);

bool dc_is_running(void);

/*
 * this function should only ever be called from the emulation thread.
 * all other threads should call dc_is_running() instead.
 */
bool dc_emu_thread_is_running(void);

bool dc_debugger_enabled(void);

/*
 * called by the SPG code on every v-blank to notify the rest of the emulator
 * that a frame has just ended.  This does not necessarily mean the frame has
 * been rendered yet (because the gfx_thread does rendering in parallel), it
 * just means that the emulation code is done simulating that frame.
 */
void dc_end_frame(void);

#ifdef ENABLE_DEBUGGER
void dc_single_step(Sh4 *sh4);
#endif

enum dc_state {
    // the emulation thread has not been started yet
    DC_STATE_NOT_RUNNING,

    // the emulation thread is currently executing
    DC_STATE_RUNNING,

    // the emulation thread has been suspended by the gdb stub
    DC_STATE_DEBUG,

    // the emulation thread has been suspended by the command-line interface
    DC_STATE_SUSPEND
};

enum dc_state dc_get_state(void);
void dc_state_transition(enum dc_state state_new, enum dc_state state_old);

enum dc_boot_mode {
    // standard boot into firmware
    DC_BOOT_FIRMWARE,

    // skip the firmware and IP.BIN and boot directly into 1st_read.bin
    DC_BOOT_DIRECT
};

void dc_request_frame_stop(void);

dc_cycle_stamp_t
dc_ch2_dma_xfer(addr32_t xfer_src, addr32_t xfer_dst, unsigned n_words);

/*
 * thin layers over corresponding pvr2 functions.  These are implemented for the
 * sake of cmd, so that dreamcast.c's pvr2 instance can be a static variable but
 * cmd can still get the info it needs.
 */
struct pvr2_tex_meta;
void dc_tex_cache_read(void **tex_dat_out, size_t *n_bytes_out,
                       struct pvr2_tex_meta const *meta);

struct pvr2_stat;
void dc_get_pvr2_stats(struct pvr2_stat *stats);

unsigned dc_get_frame_count(void);

/*
 * attempt to read a 4-byte value from the sh4's memory map.
 * returns 0 on success, -1 on failure.
 *
 * This function is intended for debugging/logging code.  It's usage should be
 * kept to a minimum.
 *
 * If ENABLE_MMU is defined and address-translation is enabled, this function
 * will attempt to perform address translation on the given address.
 */
int dc_try_read32(uint32_t addr, uint32_t *valp);

/*
 * control whether ADDR_AREA4_TEX_REGION_0 and ADDR_AREA4_TEX_REGION_1 map to
 * 64-bit or 32-bit texture bus.
 */
void dc_set_lmmode0(unsigned val);
void dc_set_lmmode1(unsigned val);
unsigned dc_get_lmmode0(void);
unsigned dc_get_lmmode1(void);

void dc_controller_press_buttons(unsigned port_no, uint32_t btns);
void dc_controller_release_buttons(unsigned port_no, uint32_t btns);
void dc_controller_set_axis(unsigned port_no, unsigned axis, unsigned val);

void dc_keyboard_set_key(unsigned port_no, unsigned btn_no, bool is_pressed);
void dc_keyboard_press_special(unsigned port_no,
                               enum washdc_keyboard_special_keys which);
void dc_keyboard_release_special(unsigned port_no,
                                 enum washdc_keyboard_special_keys which);

double dc_get_fps(void);
double dc_get_virt_fps(void);

#endif
