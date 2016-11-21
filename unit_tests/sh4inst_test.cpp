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
#include <sstream>

#include "BaseException.hpp"
#include "hw/sh4/Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "tool/sh4asm/sh4asm.hpp"
#include "RandGenerator.hpp"

typedef int(*inst_test_func_t)(Sh4 *cpu, Memory *mem);

class Sh4InstTests {
public:
    /*
     * Put the cpu in a "clean" default state.
     */
    static void reset_cpu(Sh4 *cpu) {
	cpu->reg.pc = 0;
    }

    // very basic test that does a whole lot of nothing
    static int nop_test(Sh4 *cpu, Memory *mem) {
	Sh4Prog test_prog;
	test_prog.assemble("NOP\n");
	const Sh4Prog::InstList& inst = test_prog.get_prog();
	mem->load_program(0, inst.begin(), inst.end());

	reset_cpu(cpu);

	cpu->exec_inst();

	return 0;
    }

    // ADD #imm, Rn
    // 0111nnnniiiiiiii
    static int add_immed_test(Sh4 *cpu, Memory *mem) {
	RandGenerator<boost::uint32_t> randgen32;
	randgen32.reset();

	int initial_val = randgen32.pick_val(0);
	/*
	 * I don't bother toggling the bank switching flag because if there's a
	 * problem with that, the root-cause will be in Sh4::gen_reg and if the
	 * root-cause is in Sh4::gen_reg then both this function and the opcode
	 * will have the exact same bug, an it will be hidden.
	 */
	for (int reg_no = 0; reg_no <= 15; reg_no++) {
	    for (int imm_val = 0; imm_val <= 0xff; imm_val++) {
		Sh4Prog test_prog;
		std::stringstream ss;
		ss << "ADD #" << imm_val << ", R" << reg_no << "\n";
		test_prog.assemble(ss.str());
		const Sh4Prog::InstList& inst = test_prog.get_prog();
		mem->load_program(0, inst.begin(), inst.end());

		reset_cpu(cpu);

		*cpu->gen_reg(reg_no) = initial_val;
		cpu->exec_inst();

		int expected_val = (initial_val + imm_val);
		int actual_val = *cpu->gen_reg(reg_no);

		if (actual_val != expected_val) {
		    std::cout << "ERROR running: " << std::endl << "\t" << ss.str();
		    std::cout << "Expected " << (initial_val + imm_val) <<
			" but got " << actual_val;
		    return 1;
		}
	    }
	}
	return 0;
    }
};

struct inst_test {
    char const *name;
    inst_test_func_t func;
} inst_tests[] = {
    { "nop_test", &Sh4InstTests::nop_test },
    { "add_immed_test", &Sh4InstTests::add_immed_test },
    { NULL }
};

int main(int argc, char **argv) {
    Memory mem(16 * 1024 * 1024);
    Sh4 cpu(&mem);
    struct inst_test *test = inst_tests;
    int n_success = 0, n_tests = 0;

    try {
	while (test->name) {
	    std::cout << "Trying " << test->name << "..." << std::endl;

	    int test_ret = test->func(&cpu, &mem);

	    if (test_ret != 0)
		std::cout << test->name << " FAIL" << std::endl;
	    else {
		std::cout << test->name << " SUCCESS" << std::endl;
		n_success++;
	    }

	    test++;
	    n_tests++;
	}
    } catch (UnimplementedError excp) {
        std::cerr << "ERROR: " << excp.what() << std::endl;
        return 1;
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << std::dec << n_tests << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;

    if (n_success == n_tests)
        return 0;
    return 1;
}
