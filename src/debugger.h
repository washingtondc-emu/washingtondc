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

#ifndef DEBUGGER_H_
#define DEBUGGER_H_

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

#include <stdbool.h>

#include "types.h"
#include "hw/sh4/sh4_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

enum debug_state {
    // the debugger is not suspending the dreamcast
    DEBUG_STATE_NORM,

    /*
     * the debugger has allowed the dreamcast to run for one instruction,
     * but it should break immediately after
     */
    DEBUG_STATE_STEP,

    /*
     * the debugger is holding at a breakpoint pending permission to continue
     * from the user.
     */
    DEBUG_STATE_BREAK
};

struct debug_frontend {
    bool(*step)(addr32_t, void*);
    void(*attach)(void*);
    void(*on_break)(void*);
    void(*on_read_watchpoint)(addr32_t addr, void*);
    void(*on_write_watchpoint)(addr32_t, void*);

    /*
     * called by the sh4 instruction decoder when it doesn't recognize an
     * opcode or it hits a TRAPA.  This generally means that we stumbled
     * across a softbreak.
     */
    void(*on_softbreak)(inst_t, addr32_t, void*);

    void(*on_cleanup)(void*);

    void *arg;
};

#define DEBUG_N_BREAKPOINTS 16
#define DEBUG_N_W_WATCHPOINTS 16
#define DEBUG_N_R_WATCHPOINTS 16

struct debugger {
    addr32_t breakpoints[DEBUG_N_BREAKPOINTS];
    bool breakpoint_enable[DEBUG_N_BREAKPOINTS];

    addr32_t w_watchpoints[DEBUG_N_W_WATCHPOINTS];
    unsigned w_watchpoint_len[DEBUG_N_W_WATCHPOINTS];
    bool w_watchpoint_enable[DEBUG_N_W_WATCHPOINTS];

    addr32_t r_watchpoints[DEBUG_N_R_WATCHPOINTS];
    unsigned r_watchpoint_len[DEBUG_N_R_WATCHPOINTS];
    bool r_watchpoint_enable[DEBUG_N_R_WATCHPOINTS];

    // when a watchpoint gets triggered, at_watchpoint is set to true
    // and the memory address is placed in watchpoint_addr
    bool at_watchpoint;
    addr32_t watchpoint_addr;

    // when this is true and at_watchpoint is true: read-watchpoint
    // when this is false and at_watchpoint is true: write-watchpoint
    bool is_read_watchpoint;

    enum debug_state cur_state;

    struct debug_frontend frontend;
};

/*
 * it is safe to call debug_init before the frontend is initialized as long as
 * it gets initialized before you call any other debug_* functions.
 */
void debug_init(struct debugger *dbg);
void debug_cleanup(struct debugger *dbg);

void debug_attach(struct debugger *dbg);

void debug_on_softbreak(struct debugger *dbg, inst_t inst, addr32_t pc);

// these functions return 0 on success, nonzer on failure
int debug_add_break(struct debugger *dbg, addr32_t addr);
int debug_remove_break(struct debugger *dbg, addr32_t addr);

// these functions return 0 on success, nonzer on failure
int debug_add_r_watch(struct debugger *dbg, addr32_t addr, unsigned len);
int debug_remove_r_watch(struct debugger *dbg, addr32_t addr, unsigned len);

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(struct debugger *dbg, addr32_t addr, unsigned len);
int debug_remove_w_watch(struct debugger *dbg, addr32_t addr, unsigned len);

// return true if the given addr and len trigger a watchpoint
bool debug_is_w_watch(struct debugger *dbg, addr32_t addr, unsigned len);
bool debug_is_r_watch(struct debugger *dbg, addr32_t addr, unsigned len);

void debug_get_all_regs(reg32_t reg_file[SH4_REGISTER_COUNT]);

void debug_set_all_regs(reg32_t const reg_file[SH4_REGISTER_COUNT]);

void debug_set_reg(unsigned reg_no, reg32_t val);

reg32_t debug_get_reg(unsigned reg_no);

unsigned debug_gen_reg_idx(unsigned idx);
unsigned debug_bank0_reg_idx(unsigned reg_sr, unsigned idx);
unsigned debug_bank1_reg_idx(unsigned reg_sr, unsigned idx);

int debug_read_mem(void *out, addr32_t addr, unsigned len);

int debug_write_mem(void const *input, addr32_t addr, unsigned len);

/*
 * called by the dreamcast code to notify the debugger that a new instruction
 * is about to execute.  This should check for hardware breakpoints and set the
 * emulator's state to DC_STATE_DEBUG if a breakpoint has been hit.
 */
void debug_notify_inst(struct debugger *dbg, Sh4 *sh4);

/*
 * called by the gdb_stub to tell the debugger to continue executing if
 * execution is suspended.
 */
void debug_request_continue(struct debugger *dbg);

/*
 * called by the gdb_stub to tell the debugger to single-step.
 */
void debug_request_single_step(struct debugger *dbg);

/*
 * called by the gdb_stub to tell the debugger that the remote gdb frontend is
 * detaching.  This clears out break points and such.
 */
void debug_request_detach(struct debugger *dbg);

void debug_request_break(struct debugger *dbg);

#ifdef __cplusplus
}
#endif

#endif
