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
#include "dc_sched.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

#ifdef ENABLE_SERIAL_SERVER
#include "serial_server.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void dreamcast_init(char const *bios_path, char const *flash_path);

#ifdef ENABLE_DIRECT_BOOT
#define ADDR_IP_BIN        0x8c008000
#define ADDR_1ST_READ_BIN  0x8c010000
#define ADDR_BOOTSTRAP     0x8c008300
#define ADDR_SYSCALLS      0x8c000000
#define LEN_SYSCALLS           0x8000

/*
 * version of dreamcast_init for direct boots (ie boots that skip BIOS and go
 * straight to IP.BIN or 1ST_READ.BIN
 */
void dreamcast_init_direct(char const *path_ip_bin,
                           char const *path_1st_read_bin,
                           char const *bios_path,
                           char const *flash_path,
                           char const *syscalls_path,
                           bool skip_ip_bin);
#endif

void dreamcast_cleanup();

#ifdef ENABLE_DEBUGGER
// this must be called before run or not at all
void dreamcast_enable_debugger(void);
#endif

#ifdef ENABLE_SERIAL_SERVER
// this must be called before run or not at all
void dreamcast_enable_serial_server(void);
#endif

void dreamcast_run();

/*
 * Kill the emulator.  This function can be safely called
 * from any thread.
 */
void dreamcast_kill();

Sh4 *dreamcast_get_cpu();

#ifdef ENABLE_DEBUGGER
struct debugger *dreamcast_get_debugger();
#endif

#if defined(ENABLE_DEBUGGER) || defined(ENABLE_SERIAL_SERVER)
extern struct event_base *dc_event_base;
#endif

/*
 * This is being made extern so that dc_cycle_stamp() can be an inline function.
 * This variable should not be read from or written to from outside of
 * Dreamcast.cpp.
 */
extern dc_cycle_stamp_t dc_cycle_stamp_priv_;

static inline dc_cycle_stamp_t dc_cycle_stamp() {
    return dc_cycle_stamp_priv_;
}

// advance the cycle stmap by n_cycles.  This will not run any scheduled events.
static inline void dc_cycle_advance(dc_cycle_stamp_t n_cycles) {
    dc_cycle_stamp_priv_ += n_cycles;
}

void dc_print_perf_stats(void);

bool dc_is_running(void);

void dreamcast_kill(void);

enum dc_state {
    // the emulation thread has not been started yet
    DC_STATE_NOT_RUNNING,

    // the emulation thread is currently executing
    DC_STATE_RUNNING,

    // the emulation thread has been suspended by the gdb stub
    DC_STATE_DEBUG
};

enum dc_state dc_get_state(void);
void dc_state_transition(enum dc_state state_new);

#ifdef __cplusplus
}
#endif

#endif
