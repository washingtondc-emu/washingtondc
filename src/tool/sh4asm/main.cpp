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

#include <unistd.h>

#include <iostream>
#include <sstream>

#include "sh4asm.hpp"

int main(int argc, char **argv) {
    Sh4Prog prog;
    inst_t inst;
    int opt;
    bool disas = false;
    char const *cmd = argv[0];

    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd':
            disas = true;
            break;
        }
    }

    argv += optind;
    argc -= optind;

    if (argc != 1) {
        std::cerr << "Usage: " << cmd << " instruction" << std::endl;
        return 1;
    }

    if (disas) {
        std::stringstream ss;
        ss << std::hex << argv[0];
        ss >> inst;

        std::cout << prog.disassemble_line(inst) << std::endl;
    } else {
        inst = prog.assemble_line((argv[0] + std::string("\n")).c_str());
        std::cout << std::hex << inst << std::endl;
    }

    // TODO: this goes in the unit_tests
    // inst = prog.assemble_line("MOV.W R4, @R5\n");
    // std::cout << std::hex << inst << std::endl;

    // inst = prog.assemble_line("MOV.W @R5, R4");
    // std::cout << std::hex << inst << std::endl;

    return 0;
}
