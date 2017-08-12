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

#include "dreamcast.h"

#include "debugger.h"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

static void frontend_attach(struct debug_frontend *frontend);
static void frontend_on_break(struct debug_frontend *frontend);
static void frontend_on_read_watchpoint(struct debug_frontend *frontend,
                                        addr32_t addr);
static void frontend_on_write_watchpoint(struct debug_frontend *frontend,
                                         addr32_t addr);
static void frontend_on_softbreak(struct debug_frontend *frontend, inst_t inst,
                                  addr32_t addr);
static void frontend_on_cleanup(struct debug_frontend *frontend);

void debug_init(struct debugger *dbg) {
    dbg->cur_state = DEBUG_STATE_BREAK;

    memset(&dbg->frontend, 0, sizeof(dbg->frontend));

    memset(dbg->breakpoint_enable, 0, sizeof(dbg->breakpoint_enable));
    memset(dbg->w_watchpoint_enable, 0, sizeof(dbg->w_watchpoint_enable));
    memset(dbg->r_watchpoint_enable, 0, sizeof(dbg->r_watchpoint_enable));
}

void debug_cleanup(struct debugger *dbg) {
    frontend_on_cleanup(&dbg->frontend);
}

void debug_check_break(struct debugger *dbg, Sh4 *sh4) {
    // hold at a breakpoint for user interaction
    if ((dbg->cur_state == DEBUG_STATE_BREAK) ||
        (dbg->cur_state == DEBUG_STATE_WATCH))
        return;

    if (dbg->cur_state == DEBUG_STATE_STEP) {
        dbg->cur_state = DEBUG_STATE_BREAK;
        frontend_on_break(&dbg->frontend);
        dc_state_transition(DC_STATE_DEBUG);
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
    if (dbg->cur_state == DEBUG_STATE_POST_WATCH) {
        /*
         * we intentionally do not return here because we still want to check
         * the breakpoints below.
         */
        dbg->cur_state = DEBUG_STATE_NORM;
    }

    if (dbg->cur_state == DEBUG_STATE_PRE_WATCH) {
        if (dbg->is_read_watchpoint)
            frontend_on_read_watchpoint(&dbg->frontend, dbg->watchpoint_addr);
        else
            frontend_on_write_watchpoint(&dbg->frontend, dbg->watchpoint_addr);
        dbg->cur_state = DEBUG_STATE_WATCH;
        dc_state_transition(DC_STATE_DEBUG);
        return;
    }

    for (unsigned bp_idx = 0; bp_idx < DEBUG_N_BREAKPOINTS; bp_idx++) {
        reg32_t pc = sh4->reg[SH4_REG_PC];
        if (dbg->breakpoint_enable[bp_idx] && pc == dbg->breakpoints[bp_idx]) {
            frontend_on_break(&dbg->frontend);
            dbg->cur_state = DEBUG_STATE_BREAK;
            dc_state_transition(DC_STATE_DEBUG);
            return;
        }
    }

    return;
}

void debug_notify_inst(struct debugger *dbg, Sh4 *sh4) {
    debug_check_break(dbg, sh4);
}

void debug_request_detach(struct debugger *dbg) {
    memset(dbg->breakpoint_enable, 0, sizeof(dbg->breakpoint_enable));
    memset(dbg->w_watchpoint_enable, 0, sizeof(dbg->w_watchpoint_enable));
    memset(dbg->r_watchpoint_enable, 0, sizeof(dbg->r_watchpoint_enable));

    dbg->cur_state = DEBUG_STATE_NORM;
    dc_state_transition(DC_STATE_RUNNING);
}

int debug_add_break(struct debugger *dbg, addr32_t addr) {
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (!dbg->breakpoint_enable[idx]) {
            dbg->breakpoints[idx] = addr;
            dbg->breakpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int debug_remove_break(struct debugger *dbg, addr32_t addr) {
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (dbg->breakpoint_enable[idx] && dbg->breakpoints[idx] == addr) {
            dbg->breakpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

// these functions return 0 on success, nonzero on failure
int debug_add_r_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++)
        if (!dbg->r_watchpoint_enable[idx]) {
            dbg->r_watchpoints[idx] = addr;
            dbg->r_watchpoint_len[idx] = len;
            dbg->r_watchpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int debug_remove_r_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++)
        if (dbg->r_watchpoint_enable[idx] && dbg->r_watchpoints[idx] == addr &&
            dbg->r_watchpoint_len[idx] == len) {
            dbg->r_watchpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++)
        if (!dbg->w_watchpoint_enable[idx]) {
            dbg->w_watchpoints[idx] = addr;
            dbg->w_watchpoint_len[idx] = len;
            dbg->w_watchpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int debug_remove_w_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++)
        if (dbg->w_watchpoint_enable[idx] && dbg->w_watchpoints[idx] == addr &&
            dbg->w_watchpoint_len[idx] == len) {
            dbg->w_watchpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

bool debug_is_w_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    if (dbg->cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++) {
        if (dbg->w_watchpoint_enable[idx]) {
            addr32_t watch_first = dbg->w_watchpoints[idx];
            addr32_t watch_last = watch_first +
                (dbg->w_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg->cur_state = DEBUG_STATE_PRE_WATCH;
                dbg->watchpoint_addr = addr;
                dbg->is_read_watchpoint = false;
                return true;
            }
        }
    }
    return false;
}

bool debug_is_r_watch(struct debugger *dbg, addr32_t addr, unsigned len) {
    if (dbg->cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++) {
        if (dbg->r_watchpoint_enable[idx]) {
            addr32_t watch_first = dbg->r_watchpoints[idx];
            addr32_t watch_last = watch_first +
                (dbg->r_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg->cur_state = DEBUG_STATE_PRE_WATCH;
                dbg->watchpoint_addr = addr;
                dbg->is_read_watchpoint = true;
                return true;
            }
        }
    }
    return false;
}

void debug_on_softbreak(struct debugger *dbg, inst_t inst, addr32_t pc) {
    dbg->cur_state = DEBUG_STATE_BREAK;
    dc_state_transition(DC_STATE_DEBUG);
    frontend_on_softbreak(&dbg->frontend, inst, pc);
}

void debug_attach(struct debugger *dbg) {
    frontend_attach(&dbg->frontend);
    dbg->cur_state = DEBUG_STATE_BREAK;
    dc_state_transition(DC_STATE_DEBUG);
}

static void frontend_attach(struct debug_frontend *frontend) {
    if (frontend->attach)
        frontend->attach(frontend->arg);
}

static void frontend_on_break(struct debug_frontend *frontend) {
    if (frontend->on_break)
        frontend->on_break(frontend->arg);
}

static void frontend_on_read_watchpoint(struct debug_frontend *frontend,
                                        addr32_t addr) {
    if (frontend->on_read_watchpoint)
        frontend->on_read_watchpoint(addr, frontend->arg);
}

static void frontend_on_write_watchpoint(struct debug_frontend *frontend,
                                         addr32_t addr) {
    if (frontend->on_write_watchpoint)
        frontend->on_write_watchpoint(addr, frontend->arg);
}

static void frontend_on_softbreak(struct debug_frontend *frontend, inst_t inst,
                                  addr32_t addr) {
    if (frontend->on_softbreak)
        frontend->on_softbreak(inst, addr, frontend->arg);
}

static void frontend_on_cleanup(struct debug_frontend *frontend) {
    if (frontend->on_cleanup)
        frontend->on_cleanup(frontend->arg);
}

void debug_get_all_regs(reg32_t reg_file[SH4_REGISTER_COUNT]) {
    sh4_get_regs(dreamcast_get_cpu(), reg_file);
}

void debug_set_all_regs(reg32_t const reg_file[SH4_REGISTER_COUNT]) {
    sh4_set_regs(dreamcast_get_cpu(), reg_file);
}

void debug_set_reg(unsigned reg_no, reg32_t val) {
    sh4_set_individual_reg(dreamcast_get_cpu(), reg_no, val);
}

reg32_t debug_get_reg(unsigned reg_no) {
    reg32_t reg_file[SH4_REGISTER_COUNT];
    debug_get_all_regs(reg_file);
    return reg_file[reg_no];
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

int debug_read_mem(void *out, addr32_t addr, unsigned len) {
    unsigned unit_len, n_units;

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
            fprintf(stderr, "WARNING: failed %u-byte write to address "
                    "0x%08x\n.  Past writes may not have failed!\n", len, addr);
            error_clear();
            return -1;
        }

        input_byte_ptr += unit_len;
        addr += unit_len;
        n_units--;
    }

    return 0;
}

void debug_request_continue(struct debugger *dbg) {
    if (dbg->cur_state == DEBUG_STATE_WATCH)
        dbg->cur_state = DEBUG_STATE_POST_WATCH;
    else
        dbg->cur_state = DEBUG_STATE_NORM;
    dc_state_transition(DC_STATE_RUNNING);
}

void debug_request_single_step(struct debugger *dbg) {
    dbg->cur_state = DEBUG_STATE_STEP;
    dc_state_transition(DC_STATE_RUNNING);
}

void debug_request_break(struct debugger *dbg) {
    if (dbg->cur_state == DEBUG_STATE_NORM) {
        dbg->cur_state = DEBUG_STATE_BREAK;
        dc_state_transition(DC_STATE_DEBUG);
        frontend_on_break(&dbg->frontend);
    }
}
