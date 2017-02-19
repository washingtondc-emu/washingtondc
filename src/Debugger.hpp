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

#ifndef DEBUGGER_HPP_
#define DEBUGGER_HPP_

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "types.hpp"

class Debugger {
public:
    Debugger();
    virtual ~Debugger();

    bool should_break(addr32_t pc);

    virtual bool step(addr32_t pc);

    virtual void attach() = 0;

    virtual void on_break() = 0;

    // these functions return 0 on success, nonzer on failure
    int add_break(addr32_t addr);
    int remove_break(addr32_t addr);
private:
    static const unsigned N_BREAKPOINTS = 10;
    addr32_t breakpoints[N_BREAKPOINTS];
    bool breakpoint_enable[N_BREAKPOINTS];

protected:
    enum State {
        STATE_NORM,
        STATE_PRE_STEP,
        STATE_POST_STEP,
        STATE_BREAK
    };
    State cur_state;

    /*
     * call this when gdb sends a detach packet.
     * This clears out break points and such.
     */
    void on_detach();
};

#endif
