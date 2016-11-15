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

#include "BaseException.hpp"
#include "hw/sh4/Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "tool/sh4asm/sh4asm.hpp"

int basic_test(Sh4 *cpu, Memory *mem) {
    Sh4Prog test_prog;
    inst_t inst = test_prog.assemble_line("NOP");
    mem->write(&inst, 0, sizeof(inst));

    cpu->exec_inst();

    return 0;
}

int main(int argc, char **argv) {
    int ret_val;
    Memory mem(16 * 1024 * 1024);
    Sh4 cpu(&mem);

    std::cout << "Trying basic_test..." << std::endl;

    try {
        ret_val = basic_test(&cpu, &mem);
    } catch (UnimplementedError excp) {
        std::cerr << "ERROR: " << excp.what() << std::endl;
        return 1;
    }

    std::cout << "Success!" << std::endl;

    return ret_val;
}
