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

#include "tool/sh4asm/sh4asm.hpp"

/*
 * This function tests assembler and disassembler functionality on the given
 * string by first assembling it, then disassessembling it then reassembling it
 * and checking the results of the two assembly operations to see if they are
 * equal (they should be).  The instructions are only compared in binary form
 * because there is not a 1:1 mapping between text-based assembly and binary
 * instructions (whitespace, hex/decimal, double-precision floating point
 * instructions that share opcodes with single-precision floating point
 * instructions, etc.).
 *
 * Of course this doesn't technically test that the assembler is correct, but
 * if it's idempotent then it probably is correct.
 *
 * This function returns true on test-pass, and false on test-fail
 */
bool test_inst(std::string const& inst) {
    Sh4Prog prog;
    inst_t inst1, inst2;
    std::string inst1_as_txt;

    if (inst.at(inst.size() - 1) != '\n') {
        // maybe an exception would be more appropriate here, idgaf
        std::cout << "ERROR: instructions need to end with newlines (this " <<
            "is a problem with the test!)" << std::endl;
        return false;
    }

    std::cout << "Testing \"" << inst.substr(0, inst.size() - 1) << "\"..." << std::endl;

    inst1 = prog.assemble_line(inst);
    inst1_as_txt = prog.disassemble_line(inst1);
    inst2 = prog.assemble_line(inst1_as_txt);

    if (inst1 == inst2) {
        std::cout << "success!" << std::endl;
        return true;
    }

    std::cout << "Failure: expected " << std::hex << inst1 << " but got " <<
        std::hex << inst2 << std::endl;
    return false;
}

bool test_inst(char const *inst) {
    return test_inst(std::string(inst));
}

char const *insts_to_test[] = {
    "NOP\n",
    NULL
};

int test_all_insts() {
    unsigned n_tests = 0;
    unsigned n_success = 0;
    char const **inst = insts_to_test;

    while (*inst) {
        if (test_inst(*inst))
            n_success++;
        n_tests++;

        inst++;
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << n_tests << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;

    if (n_success == n_tests)
        return 0;
    return 1;
}

int main(int argc, char **argv) {
    return test_all_insts();
}
