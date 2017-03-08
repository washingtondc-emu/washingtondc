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
    virtual void on_read_watchpoint(addr32_t addr) = 0;
    virtual void on_write_watchpoint(addr32_t addr) = 0;

    /*
     * called by the sh4 instruction decoder when it doesn't recognize an
     * opcode or it hits a TRAPA.  This generally means that we stumbled
     * across a softbreak.
     */
    virtual void on_softbreak(inst_t inst, addr32_t addr) = 0;

    // these functions return 0 on success, nonzer on failure
    int add_break(addr32_t addr);
    int remove_break(addr32_t addr);

    // these functions return 0 on success, nonzer on failure
    int add_r_watch(addr32_t addr, unsigned len);
    int remove_r_watch(addr32_t addr, unsigned len);

    // these functions return 0 on success, nonzer on failure
    int add_w_watch(addr32_t addr, unsigned len);
    int remove_w_watch(addr32_t addr, unsigned len);

    // return true if the given addr and len trigger a watchpoint
    bool is_w_watch(addr32_t addr, unsigned len);
    bool is_r_watch(addr32_t addr, unsigned len);

private:
    static const unsigned N_BREAKPOINTS = 16;
    addr32_t breakpoints[N_BREAKPOINTS];
    bool breakpoint_enable[N_BREAKPOINTS];

    static const unsigned N_W_WATCHPOINTS = 16;
    addr32_t w_watchpoints[N_W_WATCHPOINTS];
    unsigned w_watchpoint_len[N_W_WATCHPOINTS];
    bool w_watchpoint_enable[N_W_WATCHPOINTS];

    static const unsigned N_R_WATCHPOINTS = 16;
    addr32_t r_watchpoints[N_R_WATCHPOINTS];
    unsigned r_watchpoint_len[N_R_WATCHPOINTS];
    bool r_watchpoint_enable[N_R_WATCHPOINTS];

    // when a watchpoint gets triggered, at_watchpoint is set to true
    // and the memory address is placed in watchpoint_addr
    bool at_watchpoint;
    addr32_t watchpoint_addr;

    // when this is true and at_watchpoint is true: read-watchpoint
    // when this is false and at_watchpoint is true: write-watchpoint
    bool is_read_watchpoint;

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
