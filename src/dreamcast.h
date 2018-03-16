/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#include "BiosFile.h"
#include "memory.h"
#include "hw/sh4/sh4.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

#include "io/serial_server.h"

#define ADDR_IP_BIN        0x8c008000
#define ADDR_1ST_READ_BIN  0x8c010000
#define ADDR_BOOTSTRAP     0x8c008300
#define ADDR_SYSCALLS      0x8c000000
#define LEN_SYSCALLS           0x8000

extern struct Memory dc_mem;

void dreamcast_init(bool cmd_session);

/* void dreamcast_cleanup(); */

void dreamcast_run();

/*
 * Kill the emulator.  This function can be safely called
 * from any thread.
 */
void dreamcast_kill();

Sh4 *dreamcast_get_cpu();

/*
 * This is being made extern so that dc_cycle_stamp() can be an inline function.
 * This variable should not be read from or written to from outside of
 * Dreamcast.cpp.
 */
/* extern dc_cycle_stamp_t dc_cycle_stamp_priv_; */

dc_cycle_stamp_t dc_cycle_stamp();
void dc_cycle_stamp_set(dc_cycle_stamp_t new_val);

/*
 * Point to where the dc_cycle_stamp should be stored.  The old pointer must
 * still be valid when this function is called.  If the new pointer is NULL,
 * then this function will choose its own pointer.
 *
 * The purpose of this is to be used as an optimization so that the x86_64 jit
 * backend can reference this variable from executable memory, which will ensure
 * fast cache accesses and also allow it to be referenced relative to the
 * x86_64's instruction pointer.
 */
void dc_set_cycle_stamp_pointer(dc_cycle_stamp_t *ptr);

void dc_print_perf_stats(void);

bool dc_is_running(void);

/*
 * this function should only ever be called from the emulation thread.
 * all other threads should call dc_is_running() instead.
 */
bool dc_emu_thread_is_running(void);

void dreamcast_kill(void);

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

    // boot directly to IP.BIN and then continue into 1st_read.bin
    DC_BOOT_IP_BIN,

    // skip the firmware and IP.BIN and boot directly into 1st_read.bin
    DC_BOOT_DIRECT
};

void dc_toggle_overlay(void);

#endif
