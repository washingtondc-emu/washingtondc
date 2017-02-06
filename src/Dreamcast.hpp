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

#ifndef DREAMCAST_HPP_
#define DREAMCAST_HPP_

#include "BiosFile.hpp"
#include "Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "hw/g1/g1.hpp"

#ifdef ENABLE_DEBUGGER
#include "Debugger.hpp"
#endif

class Dreamcast {
public:
    Dreamcast(char const *bios_path);
    ~Dreamcast();

#ifdef ENABLE_DEBUGGER
    // this must be called before run or not at all
    void enable_debugger(void);
#endif

    void run();
    void kill();

    Sh4 *get_cpu();
    Memory *gem_mem();
private:
    static const size_t MEM_SZ = 16 * 1024 * 1024;

    Sh4 cpu;
    BiosFile *bios;
    struct Memory mem;
    G1Bus *g1;

    bool is_running;

#ifdef ENABLE_DEBUGGER
    Debugger *debugger;
#endif
};

#endif
