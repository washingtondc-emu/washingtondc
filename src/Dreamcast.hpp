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

#ifndef DREAMCAST_HPP_
#define DREAMCAST_HPP_

#include <boost/asio.hpp>

#include "BiosFile.h"
#include "memory.h"
#include "hw/sh4/sh4.hpp"
#include "dc_sched.hpp"

#ifdef ENABLE_DEBUGGER
#include "Debugger.hpp"
#endif

#ifdef ENABLE_SERIAL_SERVER
#include "SerialServer.hpp"
#endif

void dreamcast_init(char const *bios_path, char const *flash_path = NULL);

#ifdef ENABLE_DIRECT_BOOT
static const size_t ADDR_IP_BIN = 0x8c008000;
static const size_t ADDR_1ST_READ_BIN = 0x8c010000;
static const size_t ADDR_BOOTSTRAP = 0x8c008300;
static const size_t ADDR_SYSCALLS = 0x8c000000;
static const size_t LEN_SYSCALLS = 0x8000;

/*
 * version of dreamcast_init for direct boots (ie boots that skip BIOS and go
 * straight to IP.BIN or 1ST_READ.BIN
 */
void dreamcast_init_direct(char const *path_ip_bin,
                           char const *path_1st_read_bin,
                           char const *bios_path = NULL,
                           char const *flash_path = NULL,
                           char const *syscalls_path = NULL,
                           bool skip_ip_bin = false);
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
Debugger *dreamcast_get_debugger();
#endif

#if defined(ENABLE_DEBUGGER) || defined(ENABLE_SERIAL_SERVER)
extern boost::asio::io_service dc_io_service;
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

#endif
