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

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#include "dreamcast.h"
#include "fifo.h"

#include "debugger.h"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

static DEF_ERROR_INT_ATTR(dbg_state);

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
    addr32_t watchpoint_addr;

    // when this is true and at_watchpoint is true: read-watchpoint
    // when this is false and at_watchpoint is true: write-watchpoint
    bool is_read_watchpoint;

    enum debug_state cur_state;

    struct debug_frontend const *frontend;
};

static struct debugger dbg;

/*
 * this gets cleared by debug_request_break to request the debugger break
 *
 * debug_request_break is called from outside of the emu thread in response to
 * the user pressing Ctrl+C on his gdb client.
 */
static atomic_flag not_request_break = ATOMIC_FLAG_INIT;

/*
 * this gets cleared by debug_request_single_step to request the debugger do a
 * single-step.
 *
 * debug_request_single_step is called from outside of the emu thread
 */
static atomic_flag not_single_step = ATOMIC_FLAG_INIT;

/*
 * this gets cleared by debug_request_continue to request the debugger continue
 * execution from a breakpoint or watchpoin
 *
 * debug_request_continue is called from outside of the emu thread
 */
static atomic_flag not_continue = ATOMIC_FLAG_INIT;

/*
 * this gets cleared by debug_request_detach to request the debugger detach
 *
 * debug_request_detach is called from outside of the emu thread
 */
static atomic_flag not_detach = ATOMIC_FLAG_INIT;

// uncomment this line to make the debugger print what it's doing
// #define DEBUGGER_LOG_VERBOSE

#ifdef DEBUGGER_LOG_VERBOSE
#define DBG_TRACE(msg, ...) dbg_do_trace(msg, ##__VA_ARGS__)
#else
#define DBG_TRACE(msg, ...)
#endif

static void frontend_attach(void);
static void frontend_on_break(void);
static void frontend_on_read_watchpoint(addr32_t addr);
static void frontend_on_write_watchpoint(addr32_t addr);
static void frontend_on_softbreak(inst_t inst, addr32_t addr);
static void frontend_on_cleanup(void);
static void frontend_run_once(void);

#ifdef DEBUGGER_LOG_VERBOSE
// don't call this directly, use the DBG_TRACE macro instead
static void dbg_do_trace(char const *msg, ...);
#endif

static void dbg_state_transition(enum debug_state new_state);

void debug_init(void) {
    memset(&dbg, 0, sizeof(dbg));

    dbg.cur_state = DEBUG_STATE_BREAK;

    memset(dbg.breakpoint_enable, 0, sizeof(dbg.breakpoint_enable));
    memset(dbg.w_watchpoint_enable, 0, sizeof(dbg.w_watchpoint_enable));
    memset(dbg.r_watchpoint_enable, 0, sizeof(dbg.r_watchpoint_enable));

    atomic_flag_test_and_set(&not_request_break);
    atomic_flag_test_and_set(&not_single_step);
    atomic_flag_test_and_set(&not_continue);
    atomic_flag_test_and_set(&not_detach);
}

void debug_cleanup(void) {
    frontend_on_cleanup();
}

void debug_check_break(Sh4 *sh4) {
    /*
     * clear the flag now, but don't actually check it until the end of the
     * function.  We want this to be the lowest-priority break-reason and we
     * also don't want it to linger around if we find some higher-priority
     * reason to stop
     */
    bool user_break = !atomic_flag_test_and_set(&not_request_break);

    // hold at a breakpoint for user interaction
    if ((dbg.cur_state == DEBUG_STATE_BREAK) ||
        (dbg.cur_state == DEBUG_STATE_WATCH))
        return;

    if (dbg.cur_state == DEBUG_STATE_STEP) {
        dbg_state_transition(DEBUG_STATE_BREAK);
        frontend_on_break();
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
        return;
    }

    /*
     * transition out of post-watch state.
     *
     * After a transition out of DC_STATE_DEBUG, the main loop in dreamcast_run
     * will execute an instruction and it will not call this function until the
     * next instruciton after that is about to be called; therefore it is always
     * correct for this function to to transition to DEBUG_STATE_NORM when the
     * cur_state is DEBUG_STATE_POST_WATCH.
     *
     */
    if (dbg.cur_state == DEBUG_STATE_POST_WATCH) {
        /*
         * we intentionally do not return here because we still want to check
         * the breakpoints below.
         */
        dbg_state_transition(DEBUG_STATE_NORM);
    }

    if (dbg.cur_state == DEBUG_STATE_PRE_WATCH) {
        if (dbg.is_read_watchpoint)
            frontend_on_read_watchpoint(dbg.watchpoint_addr);
        else
            frontend_on_write_watchpoint(dbg.watchpoint_addr);
        dbg_state_transition(DEBUG_STATE_WATCH);
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
        return;
    }

    for (unsigned bp_idx = 0; bp_idx < DEBUG_N_BREAKPOINTS; bp_idx++) {
        reg32_t pc = sh4->reg[SH4_REG_PC];
        if (dbg.breakpoint_enable[bp_idx] && pc == dbg.breakpoints[bp_idx]) {
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
            return;
        }
    }

    if (user_break) {
        frontend_on_break();
        dbg_state_transition(DEBUG_STATE_BREAK);
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
    }
}

void debug_notify_inst(Sh4 *sh4) {
    debug_check_break(sh4);
}

void debug_request_detach(void) {
    atomic_flag_clear(&not_detach);
}

int debug_add_break(addr32_t addr) {
    DBG_TRACE("request to add hardware breakpoint at 0x%08x\n",
        (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (!dbg.breakpoint_enable[idx]) {
            dbg.breakpoints[idx] = addr;
            dbg.breakpoint_enable[idx] = true;
            return 0;
        }

    DBG_TRACE("unable to add hardware breakpoint at 0x%08x (there are already "
              "%u breakpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_BREAKPOINTS);
    return ENOBUFS;
}

int debug_remove_break(addr32_t addr) {
    DBG_TRACE("request to remove hardware breakpoint at 0x%08x\n",
              (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (dbg.breakpoint_enable[idx] && dbg.breakpoints[idx] == addr) {
            dbg.breakpoint_enable[idx] = false;
            return 0;
        }

    DBG_TRACE("unable to remove hardware breakpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

// these functions return 0 on success, nonzero on failure
int debug_add_r_watch(addr32_t addr, unsigned len) {
    DBG_TRACE("request to add read-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++)
        if (!dbg.r_watchpoint_enable[idx]) {
            dbg.r_watchpoints[idx] = addr;
            dbg.r_watchpoint_len[idx] = len;
            dbg.r_watchpoint_enable[idx] = true;
            return 0;
        }

    DBG_TRACE("unable to add read-watchpoint at 0x%08x (there are already %u "
              "read-watchpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_R_WATCHPOINTS);
    return ENOBUFS;
}

int debug_remove_r_watch(addr32_t addr, unsigned len) {
    DBG_TRACE("request to remove read-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++)
        if (dbg.r_watchpoint_enable[idx] && dbg.r_watchpoints[idx] == addr &&
            dbg.r_watchpoint_len[idx] == len) {
            dbg.r_watchpoint_enable[idx] = false;
            return 0;
        }

    DBG_TRACE("unable to remove read-watchpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(addr32_t addr, unsigned len) {
    DBG_TRACE("request to add write-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++)
        if (!dbg.w_watchpoint_enable[idx]) {
            dbg.w_watchpoints[idx] = addr;
            dbg.w_watchpoint_len[idx] = len;
            dbg.w_watchpoint_enable[idx] = true;
            return 0;
        }

    DBG_TRACE("unable to add write-watchpoint at 0x%08x (there are already %u "
              "read-watchpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_W_WATCHPOINTS);
    return ENOBUFS;
}

int debug_remove_w_watch(addr32_t addr, unsigned len) {
    DBG_TRACE("request to remove write-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++)
        if (dbg.w_watchpoint_enable[idx] && dbg.w_watchpoints[idx] == addr &&
            dbg.w_watchpoint_len[idx] == len) {
            dbg.w_watchpoint_enable[idx] = false;
            return 0;
        }

    DBG_TRACE("unable to remove write-watchpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

bool debug_is_w_watch(addr32_t addr, unsigned len) {
    if (dbg.cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++) {
        if (dbg.w_watchpoint_enable[idx]) {
            addr32_t watch_first = dbg.w_watchpoints[idx];
            addr32_t watch_last = watch_first +
                (dbg.w_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg_state_transition(DEBUG_STATE_PRE_WATCH);
                dbg.watchpoint_addr = addr;
                dbg.is_read_watchpoint = false;
                DBG_TRACE("write-watchpoint at 0x%08x triggered!\n",
                          (unsigned)addr);
                return true;
            }
        }
    }
    return false;
}

bool debug_is_r_watch(addr32_t addr, unsigned len) {
    if (dbg.cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++) {
        if (dbg.r_watchpoint_enable[idx]) {
            addr32_t watch_first = dbg.r_watchpoints[idx];
            addr32_t watch_last = watch_first +
                (dbg.r_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg_state_transition(DEBUG_STATE_PRE_WATCH);
                dbg.watchpoint_addr = addr;
                dbg.is_read_watchpoint = true;
                DBG_TRACE("read-watchpoint at 0x%08x triggered!\n",
                          (unsigned)addr);
                return true;
            }
        }
    }
    return false;
}

void debug_on_softbreak(inst_t inst, addr32_t pc) {
    DBG_TRACE("softbreak at 0x%08x\n", (unsigned)pc);
    dbg_state_transition(DEBUG_STATE_BREAK);
    dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
    frontend_on_softbreak(inst, pc);
}

void debug_attach(struct debug_frontend const *frontend) {
    DBG_TRACE("debugger attached\n");
    dbg.frontend = frontend;
    frontend_attach();
    dbg_state_transition(DEBUG_STATE_BREAK);
    dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);

    atomic_flag_test_and_set(&not_request_break);
    atomic_flag_test_and_set(&not_single_step);
    atomic_flag_test_and_set(&not_continue);
    atomic_flag_test_and_set(&not_detach);
}

static void frontend_attach(void) {
    if (dbg.frontend && dbg.frontend->attach)
        dbg.frontend->attach(dbg.frontend->arg);
}

static void frontend_run_once(void) {
    if (dbg.frontend && dbg.frontend->run_once)
        dbg.frontend->run_once(dbg.frontend->arg);
}

static void frontend_on_break(void) {
    if (dbg.frontend && dbg.frontend->on_break)
        dbg.frontend->on_break(dbg.frontend->arg);
}

static void frontend_on_read_watchpoint(addr32_t addr) {
    if (dbg.frontend && dbg.frontend->on_read_watchpoint)
        dbg.frontend->on_read_watchpoint(addr, dbg.frontend->arg);
}

static void frontend_on_write_watchpoint(addr32_t addr) {
    if (dbg.frontend->on_write_watchpoint)
        dbg.frontend->on_write_watchpoint(addr, dbg.frontend->arg);
}

static void frontend_on_softbreak(inst_t inst, addr32_t addr) {
    if (dbg.frontend->on_softbreak)
        dbg.frontend->on_softbreak(inst, addr, dbg.frontend->arg);
}

static void frontend_on_cleanup(void) {
    if (dbg.frontend && dbg.frontend->on_cleanup)
        dbg.frontend->on_cleanup(dbg.frontend->arg);
}

unsigned debug_gen_reg_idx(unsigned idx) {
    return SH4_REG_R0 + idx;
}

unsigned debug_bank0_reg_idx(unsigned reg_sr, unsigned idx) {
    if (reg_sr & SH4_SR_RB_MASK)
        return SH4_REG_R0_BANK + idx;
    return SH4_REG_R0 + idx;
}

unsigned debug_bank1_reg_idx(unsigned reg_sr, unsigned idx) {
    if (reg_sr & SH4_SR_RB_MASK)
        return SH4_REG_R0 + idx;
    return SH4_REG_R0_BANK + idx;
}

void debug_request_continue(void) {
    atomic_flag_clear(&not_continue);
}

void debug_request_single_step(void) {
    atomic_flag_clear(&not_single_step);
}

void debug_request_break() {
    atomic_flag_clear(&not_request_break);
}

#ifdef DEBUGGER_LOG_VERBOSE
void dbg_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    printf("DEBUGGER: ");

    vprintf(msg, var_args);

    va_end(var_args);
}
#endif

#ifdef DEBUGGER_LOG_VERBOSE
static char const*
dbg_state_names[DEBUG_STATE_COUNT] = {
    "DEBUG_STATE_NORM",
    "DEBUG_STATE_STEP",
    "DEBUG_STATE_BREAK",
    "DEBUG_STATE_PRE_WATCH",
    "DEBUG_STATE_WATCH",
    "DEBUG_STATE_POST_WATCH"
};
#endif

static void dbg_state_transition(enum debug_state new_state) {
    DBG_TRACE("state transition from %s to %s\n",
              dbg_state_names[dbg.cur_state], dbg_state_names[new_state]);
    dbg.cur_state = new_state;
}

static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t debug_cond = PTHREAD_COND_INITIALIZER;

void debug_lock(void) {
    if (pthread_mutex_lock(&debug_mutex) < 0)
        abort(); // TODO error handling
}

void debug_unlock(void) {
    if (pthread_mutex_unlock(&debug_mutex) < 0)
        abort(); // TODO error handling
}

void debug_signal(void) {
    if (pthread_cond_signal(&debug_cond) < 0)
        abort();
}

void debug_run_once(void) {
    frontend_run_once();

    if ((dbg.cur_state != DEBUG_STATE_BREAK) &&
        (dbg.cur_state != DEBUG_STATE_WATCH)) {
        error_set_dbg_state(dbg.cur_state);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (!atomic_flag_test_and_set(&not_single_step)) {
        // gdb frontend requested a single-step via debug_request_single_step
        dbg_state_transition(DEBUG_STATE_STEP);
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }

    if (!atomic_flag_test_and_set(&not_continue)) {
        if (dbg.cur_state == DEBUG_STATE_WATCH)
            dbg_state_transition(DEBUG_STATE_POST_WATCH);
        else
            dbg_state_transition(DEBUG_STATE_NORM);
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }

    if (!atomic_flag_test_and_set(&not_detach)) {
        DBG_TRACE("detach request\n");
        memset(dbg.breakpoint_enable, 0, sizeof(dbg.breakpoint_enable));
        memset(dbg.w_watchpoint_enable, 0, sizeof(dbg.w_watchpoint_enable));
        memset(dbg.r_watchpoint_enable, 0, sizeof(dbg.r_watchpoint_enable));

        dbg_state_transition(DEBUG_STATE_NORM);
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }
}

void debug_get_all_regs(reg32_t reg_file[SH4_REGISTER_COUNT]) {
    sh4_get_regs(dreamcast_get_cpu(), reg_file);
}

void debug_set_all_regs(reg32_t const reg_file[SH4_REGISTER_COUNT]) {
    DBG_TRACE("writing to all registers\n");
    sh4_set_regs(dreamcast_get_cpu(), reg_file);
}

// this one's just a layer on top of get_all_regs
reg32_t debug_get_reg(unsigned reg_no) {
    reg32_t reg_file[SH4_REGISTER_COUNT];
    debug_get_all_regs(reg_file);
    return reg_file[reg_no];
}

void debug_set_reg(unsigned reg_no, reg32_t val) {
    DBG_TRACE("setting register index %u to 0x%08x\n",
              (unsigned)reg_no, (unsigned)val);
    sh4_set_individual_reg(dreamcast_get_cpu(),
                           (unsigned)reg_no, (unsigned)val);
}

int debug_read_mem(void *out, addr32_t addr, unsigned len) {
    unsigned unit_len, n_units;

    DBG_TRACE("request to read %u bytes from %08x\n",
              (unsigned)len, (unsigned)addr);

    if (len % 4 == 0) {
        unit_len = 4;
        n_units = len / 4;
    } else if (len % 2 == 0) {
        unit_len = 2;
        n_units = len / 2;
    } else {
        unit_len = 1;
        n_units = len;
    }

    uint8_t *out_byte_ptr = out;

    while (n_units) {
        int err = sh4_do_read_mem(dreamcast_get_cpu(), out_byte_ptr,
                                  addr, unit_len);

        if (err != MEM_ACCESS_SUCCESS) {
            error_clear();
            return -1;
        }

        out_byte_ptr += unit_len;
        addr += unit_len;
        n_units--;
    }

    return 0;
}

int debug_write_mem(void const *input, addr32_t addr, unsigned len) {
    unsigned unit_len, n_units;

    DBG_TRACE("request to write %u bytes to 0x%08x\n",
              (unsigned)len, (unsigned)addr);

    if (len % 4 == 0) {
        unit_len = 4;
        n_units = len / 4;
    } else if (len % 2 == 0) {
        unit_len = 2;
        n_units = len / 2;
    } else {
        unit_len = 1;
        n_units = len;
    }

    uint8_t const *input_byte_ptr = input;

    while (n_units) {
        int err = sh4_do_write_mem(dreamcast_get_cpu(), input_byte_ptr,
                                   addr, unit_len);

        if (err != MEM_ACCESS_SUCCESS) {
            /*
             * Ideally none of the writes would go through if there's a
             * failure at any point down the line, but that's not the way I've
             * implemented this.
             */
            DBG_TRACE("WARNING: failed %u-byte write to address "
                      "0x%08x\n.  Past writes may not have failed!\n",
                      len, addr);
            error_clear();
            return -1;
        }

        input_byte_ptr += unit_len;
        addr += unit_len;
        n_units--;
    }

    return 0;
}
