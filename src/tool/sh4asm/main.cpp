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

#include "sh4asm.hpp"

int main(int argc, char **argv) {
    Sh4Prog prog;
    inst_t inst;

    // TODO: this goes in the unit_tests
    inst = prog.assemble_line("MOV.W R4, @R5\n");
    std::cout << std::hex << inst << std::endl;

    inst = prog.assemble_line("MOV.W @R5, R4");
    std::cout << std::hex << inst << std::endl;

    return 0;
}
