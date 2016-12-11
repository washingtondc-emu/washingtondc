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

#include <iostream>

#include "common/BaseException.hpp"
#include "BiosFile.hpp"

#include "Dreamcast.hpp"

Dreamcast::Dreamcast(char const *bios_path) {
    mem = new Memory(MEM_SZ);
    cpu = new Sh4(mem);

    BiosFile bios(bios_path);
    mem->load_program<inst_t*>(0, (inst_t*)bios.begin(), (inst_t*)bios.end());
}

Dreamcast::~Dreamcast() {
    delete mem;
    delete cpu;
}

void Dreamcast::run() {
    try {
        while (true) {
            cpu->exec_inst();
        }
    } catch(const std::exception& exc) {
        std::cerr << "Exception caught - " << exc.what() << std::endl;
    }
}
