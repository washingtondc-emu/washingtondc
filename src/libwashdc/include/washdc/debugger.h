/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include <stdbool.h>

#include "washdc/cpu.h"
#include "washdc/types.h"
#include "washdc/hw/sh4/sh4_reg_idx.h"
#include "washdc/MemoryMap.h"

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

enum dbg_context_id {
    DEBUG_CONTEXT_SH4,
    DEBUG_CONTEXT_ARM7,

    NUM_DEBUG_CONTEXTS
};

#ifdef ENABLE_DBG_COND

enum dbg_cond_tp {
    DEBUG_CONDITION_NONE,

    // break when a register is set to a given value
    DEBUG_CONDITION_REG_VAL,

    /*
     * break when a memory address is set to a given value.
     * TODO: re-implement watchpoints on top of this.
     */
    DEBUG_CONDITION_MEM_VAL
};

struct dbg_cond_reg_val {
    unsigned reg_no;
    uint32_t reg_val;

    uint32_t prev_reg_val;
};

union dbg_val {
    uint32_t val32;
    uint16_t val16;
    uint8_t val8;
};

struct dbg_cond_mem_val {
    uint32_t addr;
    union dbg_val val;

    // size can only be 1, 2 or 4
    unsigned size;

    union dbg_val prev_val;
};

union dbg_cond_status {
    struct dbg_cond_reg_val cond_reg_val;
    struct dbg_cond_mem_val cond_mem_val;
};

struct dbg_condition {
    enum dbg_cond_tp cond_tp;

    union dbg_cond_status status;

    enum dbg_context_id ctx;
};

void debug_check_conditions(enum dbg_context_id ctx);

// returns false if it failed to add the condition
bool debug_reg_cond(enum dbg_context_id ctx, unsigned reg_no,
                    uint32_t reg_val);

bool debug_mem_cond(enum dbg_context_id ctx, uint32_t addr,
                    uint32_t val, unsigned size);

#define N_DEBUG_CONDITIONS 16

#endif

void debug_init_context(enum dbg_context_id id, void *cpu,
                        struct memory_map *map);

void debug_set_context(enum dbg_context_id id);
enum dbg_context_id debug_current_context(void);

struct debug_frontend {
    /*
     * this method gets called from the emulation thread.  It signals that the
     * debugger should configure its interface and block until that interface
     * is ready.
     *
     * In GDB's case, that means it should listen for incoming connections and
     * block until somebody connects.
     */
    void(*attach)(void*);

    void(*on_break)(enum dbg_context_id, void*);
    void(*on_read_watchpoint)(enum dbg_context_id, addr32_t, void*);
    void(*on_write_watchpoint)(enum dbg_context_id, addr32_t, void*);

    /*
     * called by the sh4 instruction decoder when it doesn't recognize an
     * opcode or it hits a TRAPA.  This generally means that we stumbled
     * across a softbreak.
     */
    void(*on_softbreak)(enum dbg_context_id, cpu_inst_param, addr32_t, void*);

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
void debug_on_softbreak(cpu_inst_param inst, addr32_t pc);

// these functions return 0 on success, nonzer on failure
int debug_add_break(enum dbg_context_id id, addr32_t addr);
int debug_remove_break(enum dbg_context_id id, addr32_t addr);

// these functions return 0 on success, nonzer on failure
int debug_add_r_watch(enum dbg_context_id id, addr32_t addr, unsigned len);
int debug_remove_r_watch(enum dbg_context_id id, addr32_t addr, unsigned len);

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(enum dbg_context_id id, addr32_t addr, unsigned len);
int debug_remove_w_watch(enum dbg_context_id id, addr32_t addr, unsigned len);

// return true if the given addr and len trigger a watchpoint
bool
debug_is_w_watch(addr32_t addr, unsigned len);
bool
debug_is_r_watch(addr32_t addr, unsigned len);

/*
 * called by the dreamcast code to notify the debugger that a new instruction
 * is about to execute.  This should check for hardware breakpoints and set the
 * emulator's state to DC_STATE_DEBUG if a breakpoint has been hit.
 */
void debug_notify_inst(void);

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
void debug_get_all_regs(enum dbg_context_id id, void *reg_file_out,
                        size_t n_bytes);

void debug_set_all_regs(enum dbg_context_id id, void const *reg_file_in,
                        size_t n_bytes);

void debug_set_reg(enum dbg_context_id id, unsigned reg_no, reg32_t val);

reg32_t debug_get_reg(enum dbg_context_id id, unsigned reg_no);

unsigned debug_gen_reg_idx(enum dbg_context_id id, unsigned idx);
unsigned debug_bank0_reg_idx(enum dbg_context_id id,
                             unsigned reg_sr, unsigned idx);
unsigned debug_bank1_reg_idx(enum dbg_context_id id,
                             unsigned reg_sr, unsigned idx);

int debug_read_mem(enum dbg_context_id id, void *out,
                   addr32_t addr, unsigned len);

int debug_write_mem(enum dbg_context_id id, void const *input,
                    addr32_t addr, unsigned len);

uint32_t debug_pc_next(enum dbg_context_id id);

#ifdef __cplusplus
}␘
#endif

#endif
