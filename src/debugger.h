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
    DEBUG_STATE_BREAK,

    /*
     * during a memory access, the debugger detected that the CPU was triggering
     * a watchpoint or rwatchpoint.  The debugger has not yet notified the gdb
     * stub that this happened.
     */
    DEBUG_STATE_PRE_WATCH,

    /*
     * the debugger is holding at a watchpoint or rwatchpoint pending
     * permission to continue from the user.  This is really the same thing as
     * DEBUG_STATE_BREAK, except DEBUG_STATE_WATCH will transition to
     * DEBUG_STATE_POST_WATCH when the user is ready to continue.
     */
    DEBUG_STATE_WATCH,

    /*
     * the debugger just left a watchpoint and needs to be smart enough not to
     * trigger that same watchpoint.  This state only lasts for one instruction.
     */
    DEBUG_STATE_POST_WATCH,

    DEBUG_STATE_COUNT
};

struct debug_frontend {
    bool(*step)(addr32_t, void*);

    /*
     * this method gets called from the emulation thread.  It signals that the
     * debugger should configure its interface and block until that interface
     * is ready.
     *
     * In GDB's case, that means it should listen for incoming connections and
     * block until somebody connects.
     */
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

    /*
     * this function gets called periodically when the debugger is at a
     * breakpoint or watchpoint.  The purpose of it is to give the gdb_stub a
     * place to do work from within the emulation thread.  This function should
     * not block and it should not have any hard timing requirements (although
     * it will get called often).
     */
    void(*run_once)(void*);

    void *arg;
};

#define DEBUG_N_BREAKPOINTS 16
#define DEBUG_N_W_WATCHPOINTS 16
#define DEBUG_N_R_WATCHPOINTS 16

/*
 * it is safe to call debug_init before the frontend is initialized as long as
 * it gets initialized before you call any other debug_* functions.
 */
void debug_init(void);
void debug_cleanup(void);

void debug_attach(struct debug_frontend const *frontend);

/*
 * this function is called from the emulation thread when it encounters a TRAPA
 * instruction.  It cannot safely be called from any other thread.
 */
void debug_on_softbreak(inst_t inst, addr32_t pc);

// these functions return 0 on success, nonzer on failure
int debug_add_break(addr32_t addr);
int debug_remove_break(addr32_t addr);

// these functions return 0 on success, nonzer on failure
int debug_add_r_watch(addr32_t addr, unsigned len);
int debug_remove_r_watch(addr32_t addr, unsigned len);

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(addr32_t addr, unsigned len);
int debug_remove_w_watch(addr32_t addr, unsigned len);

// return true if the given addr and len trigger a watchpoint
bool debug_is_w_watch(addr32_t addr, unsigned len);
bool debug_is_r_watch(addr32_t addr, unsigned len);

/*
 * called by the dreamcast code to notify the debugger that a new instruction
 * is about to execute.  This should check for hardware breakpoints and set the
 * emulator's state to DC_STATE_DEBUG if a breakpoint has been hit.
 */
void debug_notify_inst(Sh4 *sh4);

/*
 * called by the gdb_stub to tell the debugger to continue executing if
 * execution is suspended.
 */
void debug_request_continue(void);

/*
 * called by the gdb_stub to tell the debugger to single-step.
 */
void debug_request_single_step(void);

/*
 * called by the gdb_stub to tell the debugger that the remote gdb frontend is
 * detaching.  This clears out break points and such.
 */
void debug_request_detach(void);

/*
 * This function can be called from any thread to request the debugger cause a
 * breakpoint.  The breakpoint may not come immediately.  The intended use-case
 * is for when the user presses Ctrl+C on his gdb client.
 */
void debug_request_break(void);

/*
 * These functions can be called from any thread.  debug_signal will wake up
 * the emulation thread when it is blocking on a debugging-related event.
 * debug_lock must be held when you call debug_signal.
 */
void debug_lock(void);
void debug_unlock(void);
void debug_signal(void);

/*
 * this is called from the emu thread's main loop whenever the dreamcast state
 * is DC_STATE_DEBUG.
 *
 * it is called repeatedly until the dreamcast's state is no longer DC_STATE_DEBUG
 */
void debug_run_once(void);

/*******************************************************************************
 *
 * The following functions all retrieve or modify the state of the emulator.
 * They are intended to be called from outside of the emulation thread.
 *
 ******************************************************************************/
// this function is meant to be called from the io thread
void debug_get_all_regs(reg32_t reg_file[SH4_REGISTER_COUNT]);

void debug_set_all_regs(reg32_t const reg_file[SH4_REGISTER_COUNT]);

void debug_set_reg(unsigned reg_no, reg32_t val);

reg32_t debug_get_reg(unsigned reg_no);

unsigned debug_gen_reg_idx(unsigned idx);
unsigned debug_bank0_reg_idx(unsigned reg_sr, unsigned idx);
unsigned debug_bank1_reg_idx(unsigned reg_sr, unsigned idx);

int debug_read_mem(void *out, addr32_t addr, unsigned len);

int debug_write_mem(void const *input, addr32_t addr, unsigned len);

#endif
