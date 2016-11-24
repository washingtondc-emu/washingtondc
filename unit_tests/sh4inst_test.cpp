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
    static int do_subc_gen_gen_test(Sh4 *cpu, Memory *mem,
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
                if (initial_val1 <= initial_val2) {
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

    static int subc_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_subc_gen_gen_test(cpu, mem,
                                                randgen32.pick_val(0),
                                                randgen32.pick_val(0));

        // make sure we get at least one value in that should not cause a carry
        failed = failed || do_subc_gen_gen_test(cpu, mem, 0, 0);

        // make sure we get at least one value in that should cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, mem,
                                 std::numeric_limits<reg32_t>::max(), 0);

        // test a value that should *almost* cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max());

        // test a value pair that should barely cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, mem, 1, 0);

        return failed;
    }

    static int do_subv_gen_gen_test(Sh4 *cpu, Memory *mem,
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

    static int subv_gen_gen_test(Sh4 *cpu, Memory *mem) {
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();
        int failed = 0;

        // do one at random...
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 randgen32.pick_val(0), randgen32.pick_val(0));

        // now do one that's trivial
        failed = failed || do_subv_gen_gen_test(cpu, mem, 0, 0);

        // now do one that *almost* causes a negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1), 0);

        // now do one that barely causes a negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1), -1);

        // now do a massive negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1),
                                 std::numeric_limits<int32_t>::min());

        // now do one that *almost* causes a positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -std::numeric_limits<int32_t>::max(), 0);

        // now do one that barely causes a positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -std::numeric_limits<int32_t>::max(), 1);

        // now do a massive positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 -std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::max());

        return failed;
    }

    // MOVT Rn
    // 0000nnnn00101001
    static int movt_unary_gen_test(Sh4 *cpu, Memory *mem) {
        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int t_val = 0; t_val < 2; t_val++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                cpu->reg.sr &= ~Sh4::SR_FLAG_T_MASK;
                if (t_val)
                    cpu->reg.sr |= Sh4::SR_FLAG_T_MASK;

                ss << "MOVT R" << reg_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                cpu->exec_inst();

                if (*cpu->gen_reg(reg_no) != t_val)
                    return 1;
            }
        }

        return 0;
    }

    // MOV #imm, Rn
    // 1110nnnniiiiiiii
    static int mov_binary_imm_gen_test(Sh4 *cpu, Memory *mem) {
        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (uint8_t imm_val = 0;
                 imm_val < std::numeric_limits<uint8_t>::max();
                 imm_val++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                // the reason for the cast to unsigned below is that the
                // operator<< overload can't tell the difference between a char
                // and an 8-bit integer
                ss << "MOV #" << (unsigned)imm_val << ", R" << reg_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                cpu->exec_inst();

                if (*cpu->gen_reg(reg_no) != (int32_t)imm_val)
                    return 1;
            }
        }

        return 0;
    }

    // MOV.W @(disp, PC), Rn
    // 1001nnnndddddddd
    static int do_movw_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem,
                                                 unsigned disp, unsigned pc,
                                                 unsigned reg_no,
                                                 int16_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV.W @(" << disp << ", PC), R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(pc, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.pc = pc;
        mem->write(&mem_val, disp * 2 + pc + 4, sizeof(mem_val));

        cpu->exec_inst();

        if (int32_t(*cpu->gen_reg(reg_no)) != int32_t(mem_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "pc is " << std::hex << pc << std::endl;
            std::cout << "expected mem_val is " << std::hex << mem_val
                      << std::endl;
            std::cout << "actual mem_val is " << std::hex <<
                *cpu->gen_reg(reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int i = 0; i < 1024; i++) {
            failed = failed ||
                do_movw_binary_binind_disp_pc_gen(cpu, mem, randgen32.pick_val(0) % 0xff,
                                                  (randgen32.pick_val(0) %
                                                   (16*1024*1024)) & ~1,
                                                  randgen32.pick_val(0) % 15,
                                                  randgen32.pick_val(0) & 0xffff);
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        failed = failed ||
            do_movw_binary_binind_disp_pc_gen(cpu, mem, 48,
                                              (randgen32.pick_val(0) %
                                               (16*1024*1024)) & ~1, 2,
                                              randgen32.pick_val(0) & 0xffff);
        return failed;
    }

    // MOV.L @(disp, PC), Rn
    // 1001nnnndddddddd
    static int do_movl_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem,
                                                 unsigned disp, unsigned pc,
                                                 unsigned reg_no,
                                                 int32_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV.L @(" << disp << ", PC), R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(pc, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.pc = pc;
        mem->write(&mem_val, disp * 4 + (pc & ~3) + 4, sizeof(mem_val));

        cpu->exec_inst();

        if (int32_t(*cpu->gen_reg(reg_no)) != int32_t(mem_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "pc is " << std::hex << pc << std::endl;
            std::cout << "expected mem_val is " << std::hex << mem_val
                      << std::endl;
            std::cout << "actual mem_val is " << std::hex <<
                *cpu->gen_reg(reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int i = 0; i < 1024; i++) {
            failed = failed ||
                do_movl_binary_binind_disp_pc_gen(cpu, mem, randgen32.pick_val(0) % 0xff,
                                                  (randgen32.pick_val(0) %
                                                   (16*1024*1024)) & ~1,
                                                  randgen32.pick_val(0) % 15,
                                                  randgen32.pick_val(0));
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        failed = failed ||
            do_movl_binary_binind_disp_pc_gen(cpu, mem, 48,
                                              (randgen32.pick_val(0) %
                                               (16*1024*1024)) & ~1, 2,
                                              randgen32.pick_val(0));
        return failed;
    }

    // MOV Rm, Rn
    // 0110nnnnmmmm0011
    static int do_mov_binary_gen_gen(Sh4 *cpu, Memory *mem, reg32_t src_val,
                                     unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = src_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != src_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int mov_binary_gen_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_mov_binary_gen_gen(cpu, mem, randgen32.pick_val(0),
                                          reg_src, reg_dst);
            }
        }
        return failed;
    }

    // MOV.B Rm, @Rn
    // 0010nnnnmmmm0000
    static int do_movb_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, uint8_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint8_t mem_val;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.B R" << reg_src << ", @R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_binary_gen_indgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W Rm, @Rn
    // 0010nnnnmmmm0001
    static int do_movw_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, uint16_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint16_t mem_val;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.W R" << reg_src << ", @R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_gen_indgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0) % 0xffff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L Rm, @Rn
    // 0010nnnnmmmm0010
    static int do_movl_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, uint32_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint8_t mem_val;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L R" << reg_src << ", @R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_gen_indgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0),
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B @Rm, Rn
    // 0110nnnnmmmm0000
    static int do_movb_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, int8_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.B @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_binary_indgen_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movb_binary_indgen_gen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W @Rm, Rn
    // 0110nnnnmmmm0001
    static int do_movw_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, int16_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.W @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_indgen_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movw_binary_indgen_gen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L @Rm, Rn
    // 0110nnnnmmmm0010
    static int do_movl_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                         unsigned addr, int32_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_indgen_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_movw_binary_indgen_gen(cpu, mem, randgen32.pick_val(0) %
                                              (16*1024*1024),
                                              randgen32.pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B Rm, @-Rn
    // 0010nnnnmmmm0100
    static int do_movb_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint8_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint8_t mem_val;

        // increment addr 'cause the opcode is going to decrement it
        addr++;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.B R" << reg_src << ", @-R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr-1, sizeof(mem_val));

        if (reg_src == reg_dst) {
            // special case - val will be decremented because the source and
            // destination are the same register
            val -= 1;
        }

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_dst) != addr - 1) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "Expected the destination to be decremented "
                "(it was not)" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_binary_gen_inddecgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movb_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W Rm, @-Rn
    // 0010nnnnmmmm0101
    static int do_movw_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint16_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint16_t mem_val;

        // increment addr 'cause the opcode is going to decrement it
        addr += 2;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.W R" << reg_src << ", @-R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr-2, sizeof(mem_val));

        if (reg_src == reg_dst) {
            // special case - val will be decremented because the source and
            // destination are the same register
            val -= 2;
        }

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_dst) != addr - 2) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "Expected the destination to be decremented "
                "(it was not)" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_gen_inddecgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movb_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L Rm, @-Rn
    // 0010nnnnmmmm0110
    static int do_movl_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint32_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint32_t mem_val;

        // increment addr 'cause the opcode is going to decrement it
        addr += 4;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L R" << reg_src << ", @-R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_dst) = addr;
        cpu->exec_inst();

        mem->read(&mem_val, addr-4, sizeof(mem_val));

        if (reg_src == reg_dst) {
            // special case - val will be decremented because the source and
            // destination are the same register
            val -= 4;
        }

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_dst) != addr - 4) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "Expected the destination to be decremented "
                "(it was not)" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_gen_inddecgen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movb_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B @Rm+, Rn
    // 0110nnnnmmmm0100
    static int do_movb_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint8_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.B @R" << reg_src << "+, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_src) != 1 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
        }

        return 0;
    }

    static int movb_binary_indgeninc_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movb_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W @Rm+, Rn
    // 0110nnnnmmmm0101
    static int do_movw_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint16_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.W @R" << reg_src << "+, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_src) != 2 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
        }

        return 0;
    }

    static int movw_binary_indgeninc_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movw_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L @Rm+, Rn
    // 0110nnnnmmmm0110
    static int do_movl_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                            unsigned addr, uint32_t val,
                                            unsigned reg_src,
                                            unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L @R" << reg_src << "+, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = addr;
        mem->write(&val, addr, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        if (*cpu->gen_reg(reg_src) != 4 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
        }

        return 0;
    }

    static int movl_binary_indgeninc_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
            failed = failed ||
                do_movl_binary_gen_inddecgen(cpu, mem,
                                             randgen32.pick_val(0) % (16 * 1024 * 1024),
                                             randgen32.pick_val(0),
                                             reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B R0, @(disp, Rn)
    // 10000000nnnndddd
    static int do_movb_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 uint8_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.B R0, @(" << (int)disp << ", R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(0) = val;
        *cpu->gen_reg(reg_base) = base;
        cpu->exec_inst();

        uint8_t mem_val;
        mem->read(&mem_val, disp + base, sizeof(mem_val));
        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << (unsigned)mem_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                failed = failed ||
                    do_movb_binary_r0_binind_disp_gen(cpu, mem, disp,
                                                      randgen32.pick_val(0) %
                                                      (16 * 1024 * 1024),
                                                      randgen32.pick_val(0),
                                                      reg_no);
            }
        }

        return failed;
    }

    static int do_movw_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 uint16_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.W R0, @(" << (int)disp << ", R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(0) = val;
        *cpu->gen_reg(reg_base) = base;
        cpu->exec_inst();

        uint16_t mem_val;
        mem->read(&mem_val, disp * 2 + base, sizeof(mem_val));
        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << (unsigned)mem_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                failed = failed ||
                    do_movw_binary_r0_binind_disp_gen(cpu, mem, disp,
                                                      randgen32.pick_val(0) %
                                                      (16 * 1024 * 1024),
                                                      randgen32.pick_val(0),
                                                      reg_no);
            }
        }

        return failed;
    }

    static int do_movl_binary_gen_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                                  uint8_t disp, reg32_t base,
                                                  uint32_t val, int reg_base,
                                                  int reg_src) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == reg_src) {
            val = base;
        }

        ss << "MOV.L R" << reg_src << ", @(" << (int)disp << ", R" <<
            reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = val;
        *cpu->gen_reg(reg_base) = base;
        cpu->exec_inst();

        uint32_t mem_val;
        mem->read(&mem_val, disp * 4 + base, sizeof(mem_val));
        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << (unsigned)mem_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_gen_binind_disp_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_base = 0; reg_base < 16; reg_base++) {
                for (int disp = 0; disp < 4; disp++) {
                    addr32_t base = randgen32.pick_val(0) % (16 * 1024 * 1024);
                    reg32_t val = randgen32.pick_val(0);
                    failed = failed ||
                        do_movl_binary_gen_binind_disp_gen(cpu, mem, disp,
                                                           base, val,
                                                           reg_base, reg_src);
                }
            }
        }

        return failed;
    }

    // MOV.B @(disp, Rm), R0
    // 10000100mmmmdddd
    static int do_movb_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 int8_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.B @(" << (int)disp << ", R" << reg_base << "), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base;
        mem->write(&val, disp + base, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(0) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            return 1;
        }
        return 0;
    }

    static int movb_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                failed = failed ||
                    do_movb_binary_binind_disp_gen_r0(cpu, mem, disp,
                                                      randgen32.pick_val(0) %
                                                      (16 * 1024 * 1024),
                                                      randgen32.pick_val(0),
                                                      reg_no);
            }
        }
        return failed;
    }

    // MOV.W @(disp, Rm), R0
    // 10000101mmmmdddd
    static int do_movw_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 int16_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.W @(" << (int)disp << ", R" << reg_base << "), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base;
        mem->write(&val, disp * 2 + base, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(0) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            return 1;
        }
        return 0;
    }

    static int movw_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                failed = failed ||
                    do_movw_binary_binind_disp_gen_r0(cpu, mem, disp,
                                                      randgen32.pick_val(0) %
                                                      (16 * 1024 * 1024),
                                                      randgen32.pick_val(0),
                                                      reg_no);
            }
        }
        return failed;
    }

    // MOV.L @(disp, Rm), Rn
    // 0101nnnnmmmmdddd
    static int do_movl_binary_binind_disp_gen_gen(Sh4 *cpu, Memory *mem,
                                                  uint8_t disp, reg32_t base,
                                                  int32_t val, int reg_base,
                                                  int reg_dst) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == reg_dst) {
            val = base;
        }

        ss << "MOV.L @(" << (int)disp << ", R" << reg_base << "), R" <<
            reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base;
        mem->write(&val, disp * 4 + base, sizeof(val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }
        return 0;
    }

    static int movl_binary_binind_disp_gen_gen(Sh4 *cpu, Memory *mem) {
        int failed = 0;
        RandGenerator<boost::uint32_t> randgen32;
        randgen32.reset();

        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                    for (int disp = 0; disp < 4; disp++) {
                        addr32_t base = randgen32.pick_val(0) %
                            (16 * 1024 * 1024);
                        uint32_t val = randgen32.pick_val(0);
                        failed = failed ||
                            do_movl_binary_binind_disp_gen_gen(cpu, mem, disp,
                                                               base, val,
                                                               reg_base,
                                                               reg_dst);
                    }
                }
        }
        return failed;
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
    { "movt_unary_gen_test", &Sh4InstTests::movt_unary_gen_test },
    { "mov_binary_imm_gen_test", &Sh4InstTests::mov_binary_imm_gen_test },
    { "movw_binary_binind_disp_pc_gen",
      &Sh4InstTests::movw_binary_binind_disp_pc_gen },
    { "movl_binary_binind_disp_pc_gen",
      &Sh4InstTests::movl_binary_binind_disp_pc_gen },
    { "mov_binary_gen_gen",
      &Sh4InstTests::mov_binary_gen_gen },
    { "movb_binary_gen_indgen", &Sh4InstTests::movb_binary_gen_indgen },
    { "movw_binary_gen_indgen", &Sh4InstTests::movw_binary_gen_indgen },
    { "movl_binary_gen_indgen", &Sh4InstTests::movl_binary_gen_indgen },
    { "movb_binary_indgen_gen", &Sh4InstTests::movb_binary_indgen_gen },
    { "movw_binary_indgen_gen", &Sh4InstTests::movw_binary_indgen_gen },
    { "movl_binary_indgen_gen", &Sh4InstTests::movl_binary_indgen_gen },
    { "movb_binary_gen_inddecgen", &Sh4InstTests::movb_binary_gen_inddecgen },
    { "movw_binary_gen_inddecgen", &Sh4InstTests::movw_binary_gen_inddecgen },
    { "movl_binary_gen_inddecgen", &Sh4InstTests::movl_binary_gen_inddecgen },
    { "movb_binary_indgeninc_gen", &Sh4InstTests::movb_binary_indgeninc_gen },
    { "movw_binary_indgeninc_gen", &Sh4InstTests::movw_binary_indgeninc_gen },
    { "movl_binary_indgeninc_gen", &Sh4InstTests::movl_binary_indgeninc_gen },
    { "movb_binary_r0_binind_disp_gen",
      &Sh4InstTests::movb_binary_r0_binind_disp_gen },
    { "movw_binary_r0_binind_disp_gen",
      &Sh4InstTests::movw_binary_r0_binind_disp_gen },
    { "movl_binary_gen_binind_disp_gen",
      &Sh4InstTests::movl_binary_gen_binind_disp_gen },
    { "movb_binary_binind_disp_gen_r0",
      &Sh4InstTests::movb_binary_binind_disp_gen_r0 },
    { "movw_binary_binind_disp_gen_r0",
      &Sh4InstTests::movw_binary_binind_disp_gen_r0 },
    { "movl_binary_binind_disp_gen_gen",
      &Sh4InstTests::movl_binary_binind_disp_gen_gen },
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
