/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include "Debugger.hpp"
#include "GdbStub.hpp"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

Debugger::Debugger() {
    this->cur_state = STATE_BREAK;

    std::fill(breakpoint_enable, breakpoint_enable + N_BREAKPOINTS, false);
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
