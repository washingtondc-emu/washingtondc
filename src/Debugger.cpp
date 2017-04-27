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

#include <algorithm>
#include <cstring>

#include <errno.h>

#include "Debugger.hpp"

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

void debug_init(struct debugger *dbg) {
    dbg->cur_state = DEBUG_STATE_BREAK;
    dbg->at_watchpoint = false;

    memset(&dbg->frontend, 0, sizeof(dbg->frontend));

    std::fill(dbg->breakpoint_enable,
              dbg->breakpoint_enable + DEBUG_N_BREAKPOINTS, false);
    std::fill(dbg->w_watchpoint_enable,
              dbg->w_watchpoint_enable + DEBUG_N_W_WATCHPOINTS, false);
    std::fill(dbg->r_watchpoint_enable,
              dbg->r_watchpoint_enable + DEBUG_N_R_WATCHPOINTS, false);
}

void debug_cleanup(struct debugger *dbg) {
}

bool debug_should_break(struct debugger *dbg, addr32_t pc) {
    // hold at a breakpoint for user interaction
    if (dbg->cur_state == DEBUG_STATE_BREAK)
        return true;

    if (dbg->cur_state == DEBUG_STATE_POST_STEP) {
        frontend_on_break(&dbg->frontend);
        dbg->cur_state = DEBUG_STATE_BREAK;
        return true;
    }

    // allow it to step once then break
    if (dbg->cur_state == DEBUG_STATE_PRE_STEP) {
        dbg->cur_state = DEBUG_STATE_POST_STEP;
        return false;
    }

    if (dbg->at_watchpoint) {
        if (dbg->is_read_watchpoint)
            frontend_on_read_watchpoint(&dbg->frontend, dbg->watchpoint_addr);
        else
            frontend_on_write_watchpoint(&dbg->frontend, dbg->watchpoint_addr);
        dbg->cur_state = DEBUG_STATE_BREAK;
        dbg->at_watchpoint = false;
        return true;
    }

    for (unsigned bp_idx = 0; bp_idx < DEBUG_N_BREAKPOINTS; bp_idx++) {
        if (dbg->breakpoint_enable[bp_idx] && pc == dbg->breakpoints[bp_idx]) {
            frontend_on_break(&dbg->frontend);
            dbg->cur_state = DEBUG_STATE_BREAK;
            return true;
        }
    }

    return false;
}

bool debug_step(struct debugger *dbg, addr32_t pc) {
    return debug_should_break(dbg, pc);
}

void debug_on_detach(struct debugger *dbg) {
    std::fill(dbg->breakpoint_enable,
              dbg->breakpoint_enable + DEBUG_N_BREAKPOINTS, false);
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
                dbg->at_watchpoint = true;
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
                dbg->at_watchpoint = true;
                dbg->watchpoint_addr = addr;
                dbg->is_read_watchpoint = true;
                return true;
            }
        }
    }
    return false;
}

void debug_on_softbreak(struct debugger *dbg, inst_t inst, addr32_t pc) {
    frontend_on_softbreak(&dbg->frontend, inst, pc);
}

void debug_attach(struct debugger *dbg) {
    frontend_attach(&dbg->frontend);
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
