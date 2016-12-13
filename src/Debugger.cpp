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

#include "Debugger.hpp"
#include "GdbStub.hpp"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

Debugger::Debugger(Dreamcast *dc) {
    this->cur_state = STATE_NORM;
    this->dc = dc;

    for (int i = 0; i < N_BREAKPOINTS; i++)
        this->breakpoints[i] = -1;
}

bool Debugger::should_break(inst_t pc) {

    // hold at a breakpoint for user interaction
    if (cur_state == STATE_STEP)
        return true;

    for (int i = 0; i < N_BREAKPOINTS; i++)
        if (pc == breakpoints[i])
            return true;

    return false;
}

void Debugger::step(inst_t pc) {
    if (!should_break(pc))
        return;
}
