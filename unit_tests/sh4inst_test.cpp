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
#include <limits>

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

        reg32_t initial_val = randgen32.pick_val(0);
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

                reg32_t expected_val = (initial_val + imm_val);
                reg32_t actual_val = *cpu->gen_reg(reg_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl <<
                        "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val + imm_val) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }
            }
        }
        return 0;
    }

    // ADD Rm, Rn
    // 0111nnnnmmmm1100
    static int add_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;
                reg32_t initial_val1 = randgen32.pick_val(0);
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32.pick_val(0);

                ss << "ADD R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = (initial_val1 + initial_val2);
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str();
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }
            }
        }
        return 0;
    }

    // ADDC Rm, Rn
    // 0011nnnnmmmm1110
    static int do_addc_gen_gen_test(Sh4 *cpu, Memory *mem,
                                    reg32_t src1, reg32_t src2) {
        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;
                reg32_t initial_val1 = src1;
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = src2;

                ss << "ADDC R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = (initial_val1 + initial_val2);
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }

                // now check the carry-bit
                uint64_t expected_val64 = uint64_t(initial_val1) +
                    uint64_t(initial_val2);
                if (expected_val64 == uint64_t(actual_val)) {
                    // there should not be a carry
                    if (cpu->reg.sr & Sh4::SR_FLAG_T_MASK) {
                        std::cout << "ERROR running: " << std::endl
                                  << "\t" << ss.str() << std::endl;
                        std::cout << "Expected no carry bit "
                            "(there was a carry)" << std::endl;
                        return 1;
                    }
                } else {
                    // there should be a carry
                    if (!(cpu->reg.sr & Sh4::SR_FLAG_T_MASK)) {
                        std::cout << "ERROR running: " << std::endl
                                  << "\t" << ss.str() << std::endl;
                        std::cout << "Expected a carry bit "
                            "(there was no carry)" << std::endl;
                        return 1;
                    }
                }
            }
        }
        return 0;
    }

    // ADDC Rm, Rn
    // 0011nnnnmmmm1110
    static int addc_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_addc_gen_gen_test(cpu, mem,
                                                randgen32.pick_val(0),
                                                randgen32.pick_val(0));

        // make sure we get at least one value in that should not cause a carry
        failed = failed || do_addc_gen_gen_test(cpu, mem, 0, 0);

        // make sure we get at least one value in that should cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max());

        // test a value that should *almost* cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, mem, 1,
                                 std::numeric_limits<reg32_t>::max() - 1);

        // test a value pair that should barely cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, mem,
                                 std::numeric_limits<reg32_t>::max() - 1, 2);

        return failed;
    }

    // ADDV Rm, Rn
    // 0011nnnnmmmm1111
    static int do_addv_gen_gen_test(Sh4 *cpu, Memory *mem,
                                    reg32_t src1, reg32_t src2) {
        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                // it is not a mistake that I'm using int32_t
                // here instead of reg32_t
                int32_t initial_val1 = src1;
                int32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = src2;

                ss << "ADDV R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = (initial_val1 + initial_val2);
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }

                // now check the overflow-bit
                bool overflow_flag = cpu->reg.sr & Sh4::SR_FLAG_T_MASK;
                if (initial_val1 >= 0 && initial_val2 >= 0) {
                    if (std::numeric_limits<int32_t>::max() - initial_val1 <
                        initial_val2) {
                        // there should be an overflow
                        if (!overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected an overflow bit "
                                "(there was no overflow bit set)" << std::endl;
                            return 1;
                        }
                    } else {
                        //there should not be an overflow
                        if (overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected no overflow bit "
                                "(there was an overflow bit set)" << std::endl;
                            return 1;
                        }
                    }
                } else if (initial_val1 < 0 && initial_val2 < 0) {
                    if (std::numeric_limits<int32_t>::min() - initial_val2 >
                        initial_val1) {
                        // there should be an overflow
                        if (!overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected an overflow bit "
                                "(there was no overflow bit set)" << std::endl;
                            return 1;
                        }
                    } else {
                        // there should not be an overflow
                        if (overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected no overflow bit "
                                "(there was an overflow bit set)" << std::endl;
                            return 1;
                        }
                    }
                }
            }
        }
        return 0;
    }

    // ADDV Rm, Rn
    // 0011nnnnmmmm1111
    static int addv_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        int failed = 0;
        randgen32.reset();

        // this should not overflow
        failed = failed || do_addv_gen_gen_test(cpu, mem, 0, 0);

        // random values for good measure
        failed = failed || do_addv_gen_gen_test(cpu, mem,
                                                randgen32.pick_val(0),
                                                randgen32.pick_val(0));

        // *almost* overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem, 1,
                                 std::numeric_limits<int32_t>::max() - 1);

        // slight overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem, 2,
                                 std::numeric_limits<int32_t>::max() - 1);

        // massive overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem,
                                 std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::max());

        // *almost* overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem,
                                 std::numeric_limits<int32_t>::min() + 1, 1);

        // slight overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem,
                                 std::numeric_limits<int32_t>::min() + 1, 2);

        // massive overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, mem,
                                 std::numeric_limits<int32_t>::min(),
                                 std::numeric_limits<int32_t>::min());

        return failed;
    }

    // SUB Rm, Rn
    // 0011nnnnmmmm1000
    static int sub_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;
                reg32_t initial_val1 = randgen32.pick_val(0);
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32.pick_val(0);

                ss << "SUB R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = initial_val2 - initial_val1;
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str();
                    std::cout << "Expected " << std::hex <<
                        (initial_val2 - initial_val1) << " but got " <<
                        actual_val << std::endl;
                    std::cout << "initial value of R" << std::dec <<
                        reg2_no << ": " << std::hex << initial_val2 <<
                        std::endl;
                    std::cout << "initial value of R" << std::dec <<
                        reg1_no << ": " << std::hex << initial_val1 <<
                        std::endl;
                    return 1;
                }
            }
        }
        return 0;
    }

    // SUBC Rm, Rn
    // 0011nnnnmmmm1010
    static int subc_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;
                reg32_t initial_val1 = randgen32.pick_val(0);
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32.pick_val(0);

                ss << "SUBC R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = initial_val2 - initial_val1;
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val2 - initial_val1) << " but got " <<
                        actual_val << std::endl;
                    std::cout << "initial value of R" << std::dec <<
                        reg2_no << ": " << std::hex << initial_val2;
                    std::cout << "initial value of R" << std::dec <<
                        reg1_no << ": " << std::hex << initial_val1;
                    return 1;
                }

                // now check the carry-bit
                uint64_t expected_val64 = uint64_t(initial_val2) -
                    uint64_t(initial_val1);
                if (expected_val64 <= std::numeric_limits<uint64_t>::max()) {
                    // there should not be a carry
                    if (cpu->reg.sr & Sh4::SR_FLAG_T_MASK) {
                        std::cout << "ERROR running: " << std::endl
                                  << "\t" << ss.str() << std::endl;
                        std::cout << "Expected no carry bit "
                            "(there was a carry)" << std::endl;
                        std::cout << "initial value of R" << std::dec <<
                            reg2_no << ": " << std::hex << initial_val2;
                        std::cout << "initial value of R" << std::dec <<
                            reg1_no << ": " << std::hex << initial_val1;
                        std::cout << "output val: " << std::hex <<
                            actual_val << std::endl;
                        return 1;
                    }
                } else {
                    // there should be a carry
                    if (!(cpu->reg.sr & Sh4::SR_FLAG_T_MASK)) {
                        std::cout << "ERROR running: " << std::endl
                                  << "\t" << ss.str() << std::endl;
                        std::cout << "Expected a carry bit "
                            "(there was no carry)" << std::endl;
                        std::cout << "initial value of R" << std::dec <<
                            reg2_no << ": " << std::hex << initial_val2 <<
                            std::endl;
                        std::cout << "initial value of R" << std::dec <<
                            reg1_no << ": " << std::hex << initial_val1 <<
                            std::endl;
                        std::cout << "output val: " << std::hex <<
                            actual_val << std::endl;
                        return 1;
                    }
                }
            }
        }
        return 0;
    }

    static int subv_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        /*
         * I don't bother toggling the bank switching flag because if there's a
         * problem with that, the root-cause will be in Sh4::gen_reg and if the
         * root-cause is in Sh4::gen_reg then both this function and the opcode
         * will have the exact same bug, an it will be hidden.
         */
        for (int reg1_no = 0; reg1_no <= 15; reg1_no++) {
            for (int reg2_no = 0; reg2_no <= 15; reg2_no++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                // it is not a mistake that I'm using int32_t
                // here instead of reg32_t
                int32_t initial_val1 = randgen32.pick_val(0);
                int32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32.pick_val(0);

                ss << "SUBV R" << reg1_no << ", R" << reg2_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *cpu->gen_reg(reg1_no) = initial_val1;
                *cpu->gen_reg(reg2_no) = initial_val2;
                cpu->exec_inst();

                reg32_t expected_val = initial_val2 - initial_val1;
                reg32_t actual_val = *cpu->gen_reg(reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }

                // now check the overflow-bit
                bool overflow_flag = cpu->reg.sr & Sh4::SR_FLAG_T_MASK;
                if (int32_t(initial_val2) >= 0 &&
                    int32_t(initial_val1) < 0) {
                    if (int32_t(actual_val) < 0) {
                        // there should be an overflow
                        if (!overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected an overflow bit "
                                "(there was no overflow bit set)" << std::endl;
                            return 1;
                        }
                    } else {
                        //there should not be an overflow
                        if (overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected no overflow bit "
                                "(there was an overflow bit set)" << std::endl;
                            return 1;
                        }
                    }
                } else if (int32_t(initial_val2) < 0 &&
                           int32_t(initial_val1) >= 0) {
                    if (int32_t(actual_val) > 0) {
                        // there should be an overflow
                        if (!overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected an overflow bit "
                                "(there was no overflow bit set)" << std::endl;
                            return 1;
                        }
                    } else {
                        // there should not be an overflow
                        if (overflow_flag) {
                            std::cout << "ERROR running: " << std::endl
                                      << "\t" << ss.str() << std::endl;
                            std::cout << "Expected no overflow bit "
                                "(there was an overflow bit set)" << std::endl;
                            return 1;
                        }
                    }
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
    { "add_gen_gen_test", &Sh4InstTests::add_gen_gen_test },
    { "addc_gen_gen_test", &Sh4InstTests::addc_gen_gen_test },
    { "addv_gen_gen_test", &Sh4InstTests::addv_gen_gen_test },
    { "sub_gen_gen_test", &Sh4InstTests::sub_gen_gen_test },
    { "subc_gen_gen_test", &Sh4InstTests::subc_gen_gen_test },
    { "subv_gen_gen_test", &Sh4InstTests::subv_gen_gen_test },
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
