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

#include <iostream>

#include "common/BaseException.hpp"

#ifdef ENABLE_DEBUGGER
#include "GdbStub.hpp"
#endif

#include "Dreamcast.hpp"

Dreamcast::Dreamcast(char const *bios_path) {
    is_running = true;

#ifdef ENABLE_DEBUGGER
    debugger = NULL;
#endif

    memory_init(&mem, MEM_SZ);
    bios = new BiosFile(bios_path);
    g1 = new G1Bus();

    mem_map = new MemoryMap(bios, &mem, g1);

    cpu = new Sh4(mem_map);
}

Dreamcast::~Dreamcast() {
#ifdef ENABLE_DEBUGGER
    if (debugger)
        delete debugger;
#endif

    delete cpu;
    delete mem_map;
    delete g1;
    delete bios;
    memory_cleanup(&mem);
}

void Dreamcast::run() {
    /*
     * TODO: later when I'm emulating more than just the CPU,
     * I'll need to remember to call this every time I re-enter
     * the CPU's context.
     */
    cpu->sh4_enter();

    try {
        while (is_running) {
#ifdef ENABLE_DEBUGGER
            if (debugger && debugger->step(cpu->get_pc()))
                continue;
#endif

            cpu->exec_inst();
        }
    } catch(const BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
    }

    if (!is_running)
        std::cout << "program execution ended normally" << std::endl;
}

void Dreamcast::kill() {
    is_running = false;
}

Sh4 *Dreamcast::get_cpu() {
    return cpu;
}

Memory *Dreamcast::gem_mem() {
    return &mem;
}

#ifdef ENABLE_DEBUGGER
void Dreamcast::enable_debugger(void) {
    debugger = new GdbStub(this);
    debugger->attach();
}
#endif
