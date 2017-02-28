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

#include "Dreamcast.hpp"
#include "Debugger.hpp"
#include "GdbStub.hpp"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

Debugger::Debugger() {
    this->cur_state = STATE_BREAK;
    at_watchpoint = false;

    std::fill(breakpoint_enable, breakpoint_enable + N_BREAKPOINTS, false);
    std::fill(w_watchpoint_enable, w_watchpoint_enable + N_W_WATCHPOINTS,
              false);
    std::fill(r_watchpoint_enable, r_watchpoint_enable + N_R_WATCHPOINTS,
              false);
}

Debugger::~Debugger() {
    // empty placeholder for virtual destructor, nothing to do here...
}

bool Debugger::should_break(addr32_t pc) {
    // hold at a breakpoint for user interaction
    if (cur_state == STATE_BREAK) {
        return true;
    }

    if (cur_state == STATE_POST_STEP) {
        on_break();
        cur_state = STATE_BREAK;
        return true;
    }

    // allow it to step once then break
    if (cur_state == STATE_PRE_STEP) {
        cur_state = STATE_POST_STEP;
        return false;
    }

    if (at_watchpoint) {
        if (is_read_watchpoint)
            on_read_watchpoint(watchpoint_addr);
        else
            on_write_watchpoint(watchpoint_addr);
        cur_state = STATE_BREAK;
        at_watchpoint = false;
        return true;
    }

    for (unsigned i = 0; i < N_BREAKPOINTS; i++) {
        if (breakpoint_enable[i] && pc == breakpoints[i]) {
            on_break();
            cur_state = STATE_BREAK;
            return true;
        }
    }

    return false;
}

bool Debugger::step(addr32_t pc) {
    return should_break(pc);
}

void Debugger::on_detach() {
    std::fill(breakpoint_enable, breakpoint_enable + N_BREAKPOINTS, false);
}

int Debugger::add_break(addr32_t addr) {
    for (unsigned idx = 0; idx < N_BREAKPOINTS; idx++)
        if (!breakpoint_enable[idx]) {
            breakpoints[idx] = addr;
            breakpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int Debugger::remove_break(addr32_t addr) {
    for (unsigned idx = 0; idx < N_BREAKPOINTS; idx++)
        if (breakpoint_enable[idx] && breakpoints[idx] == addr) {
            breakpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

// these functions return 0 on success, nonzero on failure
int Debugger::add_r_watch(addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < N_R_WATCHPOINTS; idx++)
        if (!r_watchpoint_enable[idx]) {
            r_watchpoints[idx] = addr;
            r_watchpoint_len[idx] = len;
            r_watchpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int Debugger::remove_r_watch(addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < N_R_WATCHPOINTS; idx++)
        if (r_watchpoint_enable[idx] && r_watchpoints[idx] == addr &&
            r_watchpoint_len[idx] == len) {
            r_watchpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

// these functions return 0 on success, nonzer on failure
int Debugger::add_w_watch(addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < N_W_WATCHPOINTS; idx++)
        if (!w_watchpoint_enable[idx]) {
            w_watchpoints[idx] = addr;
            w_watchpoint_len[idx] = len;
            w_watchpoint_enable[idx] = true;
            return 0;
        }

    return ENOBUFS;
}

int Debugger::remove_w_watch(addr32_t addr, unsigned len) {
    for (unsigned idx = 0; idx < N_W_WATCHPOINTS; idx++)
        if (w_watchpoint_enable[idx] && w_watchpoints[idx] == addr &&
            w_watchpoint_len[idx] == len) {
            w_watchpoint_enable[idx] = false;
            return 0;
        }

    return EINVAL;
}

bool Debugger::is_w_watch(addr32_t addr, unsigned len) {
    if (cur_state != STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < N_W_WATCHPOINTS; idx++) {
        if (w_watchpoint_enable[idx]) {
            addr32_t watch_first = w_watchpoints[idx];
            addr32_t watch_last = watch_first + (w_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                at_watchpoint = true;
                watchpoint_addr = addr;
                is_read_watchpoint = false;
                return true;
            }
        }
    }
    return false;
}

bool Debugger::is_r_watch(addr32_t addr, unsigned len) {
    if (cur_state != STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < N_R_WATCHPOINTS; idx++) {
        if (r_watchpoint_enable[idx]) {
            addr32_t watch_first = r_watchpoints[idx];
            addr32_t watch_last = watch_first + (r_watchpoint_len[idx] - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                at_watchpoint = true;
                watchpoint_addr = addr;
                is_read_watchpoint = true;
                return true;
            }
        }
    }
    return false;
}
