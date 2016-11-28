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
#include <limits>

#include "BaseException.hpp"
#include "hw/sh4/Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "tool/sh4asm/sh4asm.hpp"
#include "RandGenerator.hpp"

typedef RandGenerator<boost::uint32_t> RandGen32;
typedef int(*inst_test_func_t)(Sh4 *cpu, Memory *mem, RandGen32 *randgen32);

class Sh4InstTests {
public:
    /*
     * Put the cpu in a "clean" default state.
     */
    static void reset_cpu(Sh4 *cpu) {
        cpu->reg.pc = 0;
        cpu->reg.sr = Sh4::SR_MD_MASK;
    }

    // very basic test that does a whole lot of nothing
    static int nop_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
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
    static int add_immed_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        reg32_t initial_val = randgen32->pick_val(0);
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
    static int add_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
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
                reg32_t initial_val1 = randgen32->pick_val(0);
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32->pick_val(0);

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
    static int addc_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_addc_gen_gen_test(cpu, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0));

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
    static int addv_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failed = 0;
        randgen32->reset();

        // this should not overflow
        failed = failed || do_addv_gen_gen_test(cpu, mem, 0, 0);

        // random values for good measure
        failed = failed || do_addv_gen_gen_test(cpu, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0));

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
    static int sub_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
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
                reg32_t initial_val1 = randgen32->pick_val(0);
                reg32_t initial_val2;

                if (reg1_no == reg2_no)
                    initial_val2 = initial_val1;
                else
                    initial_val2 = randgen32->pick_val(0);

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

    static int subc_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_subc_gen_gen_test(cpu, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0));

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

    static int subv_gen_gen_test(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failed = 0;

        // do one at random...
        failed = failed ||
            do_subv_gen_gen_test(cpu, mem,
                                 randgen32->pick_val(0), randgen32->pick_val(0));

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
    static int movt_unary_gen_test(Sh4 *cpu, Memory *mem,
                                   RandGen32 *randgen32) {
        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int t_val = 0; t_val < 2; t_val++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                ss << "MOVT R" << reg_no << "\n";
                test_prog.assemble(ss.str());
                const Sh4Prog::InstList& inst = test_prog.get_prog();
                mem->load_program(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                cpu->reg.sr &= ~Sh4::SR_FLAG_T_MASK;
                if (t_val)
                    cpu->reg.sr |= Sh4::SR_FLAG_T_MASK;

                 cpu->exec_inst();

                if (*cpu->gen_reg(reg_no) != t_val)
                    return 1;
            }
        }

        return 0;
    }

    // MOV #imm, Rn
    // 1110nnnniiiiiiii
    static int mov_binary_imm_gen_test(Sh4 *cpu, Memory *mem,
                                       RandGen32 *randgen32) {
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
        cpu->write_mem(&mem_val, disp * 2 + pc + 4, sizeof(mem_val));

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

    static int movw_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int disp = 0; disp < 256; disp++) {
            for (int reg_no = 0; reg_no < 16; reg_no++) {
                addr32_t pc_max = mem->get_size() - 1 - 4 - disp * 2;
                addr32_t pc_val =
                    randgen32->pick_range(0, pc_max) & ~1;
                failed = failed ||
                    do_movw_binary_binind_disp_pc_gen(cpu, mem, disp,
                                                      pc_val, reg_no,
                                                      randgen32->pick_val(0) &
                                                      0xffff);
            }
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        addr32_t pc_val = (randgen32->pick_val(0) % mem->get_size()) & ~1;
        failed = failed ||
            do_movw_binary_binind_disp_pc_gen(cpu, mem, 48, pc_val, 2,
                                              randgen32->pick_val(0) & 0xffff);
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
        cpu->write_mem(&mem_val, disp * 4 + (pc & ~3) + 4, sizeof(mem_val));

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

    static int movl_binary_binind_disp_pc_gen(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned disp = 0; disp < 256; disp++) {
            for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            /*
             * the reason that pc_max gets OR'd with three is that those lower
             * two bits will get cleared when the instruction calculates the
             * actual address.
             */
            addr32_t pc_max = (mem->get_size() - 1 - 4 - disp * 4) | 3;
            addr32_t pc_val =
                randgen32->pick_range(0, pc_max) & ~1;
            failed = failed ||
                do_movl_binary_binind_disp_pc_gen(cpu, mem, disp, pc_val,
                                                  reg_no,
                                                  randgen32->pick_val(0));
            }
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        addr32_t pc_val = (randgen32->pick_val(0) % mem->get_size()) & ~1;
        failed = failed ||
            do_movl_binary_binind_disp_pc_gen(cpu, mem, 48, pc_val, 2,
                                              randgen32->pick_val(0));
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

    static int mov_binary_gen_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_mov_binary_gen_gen(cpu, mem, randgen32->pick_val(0),
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

        cpu->read_mem(&mem_val, addr, sizeof(mem_val));

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

    static int movb_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % mem->get_size();
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
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

        cpu->read_mem(&mem_val, addr, sizeof(mem_val));

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

    static int movw_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % (mem->get_size() - 1);
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, addr,
                                              randgen32->pick_val(0) % 0xffff,
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

        cpu->read_mem(&mem_val, addr, sizeof(mem_val));

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

    static int movl_binary_gen_indgen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % (mem->get_size() - 3);
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, mem, addr,
                                              randgen32->pick_val(0),
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movb_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % mem->get_size();
                failed = failed ||
                    do_movb_binary_indgen_gen(cpu, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movw_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % (mem->get_size() - 1);
                failed = failed ||
                    do_movw_binary_indgen_gen(cpu, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movl_binary_indgen_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_val(0) % (mem->get_size() - 3);
                failed = failed ||
                    do_movw_binary_indgen_gen(cpu, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
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

        cpu->read_mem(&mem_val, addr-1, sizeof(mem_val));

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

    static int movb_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(1, mem->get_size() - 2);
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, mem, addr,
                                                 randgen32->pick_val(0),
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

        cpu->read_mem(&mem_val, addr-2, sizeof(mem_val));

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

    static int movw_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(2, mem->get_size() - 2);
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, mem, addr,
                                                 randgen32->pick_val(0),
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

        cpu->read_mem(&mem_val, addr-4, sizeof(mem_val));

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

    static int movl_binary_gen_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, mem, addr,
                                                 randgen32->pick_val(0),
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movb_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(0, mem->get_size() - 2);
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, mem, addr,
                                                 randgen32->pick_val(0),
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movw_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(0, mem->get_size() - 3);
                failed = failed ||
                    do_movw_binary_gen_inddecgen(cpu, mem,
                                                 randgen32->pick_val(0) %
                                                 mem->get_size(),
                                                 randgen32->pick_val(0),
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
        cpu->write_mem(&val, addr, sizeof(val));
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

    static int movl_binary_indgeninc_gen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
                failed = failed ||
                    do_movl_binary_gen_inddecgen(cpu, mem, addr,
                                                 randgen32->pick_val(0),
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
        cpu->read_mem(&mem_val, disp + base, sizeof(mem_val));
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

    static int movb_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base =
                    randgen32->pick_range(0, mem->get_size() - 1 - 0xf);
                failed = failed ||
                    do_movb_binary_r0_binind_disp_gen(cpu, mem, disp, base,
                                                      randgen32->pick_val(0),
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
        cpu->read_mem(&mem_val, disp * 2 + base, sizeof(mem_val));
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

    static int movw_binary_r0_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base =
                    randgen32->pick_range(0, mem->get_size() - 2 - 0xf * 2);
                failed = failed ||
                    do_movw_binary_r0_binind_disp_gen(cpu, mem, disp,
                                                      randgen32->pick_val(0) %
                                                      mem->get_size(),
                                                      randgen32->pick_val(0),
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
        cpu->read_mem(&mem_val, disp * 4 + base, sizeof(mem_val));
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

    static int movl_binary_gen_binind_disp_gen(Sh4 *cpu, Memory *mem,
                                               RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_base = 0; reg_base < 16; reg_base++) {
                for (int disp = 0; disp < 4; disp++) {
                    reg32_t base =
                        randgen32->pick_range(0, mem->get_size() - 4 - 0xf * 4);
                    reg32_t val = randgen32->pick_val(0);
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
        cpu->write_mem(&val, disp + base, sizeof(val));
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

    static int movb_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base =
                    randgen32->pick_range(0, mem->get_size() - 1 - 0xf);
                failed = failed ||
                    do_movb_binary_binind_disp_gen_r0(cpu, mem, disp, base,
                                                      randgen32->pick_val(0),
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
        cpu->write_mem(&val, disp * 2 + base, sizeof(val));
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

    static int movw_binary_binind_disp_gen_r0(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base =
                    randgen32->pick_range(0, mem->get_size() - 2 - 0xf * 2);
                failed = failed ||
                    do_movw_binary_binind_disp_gen_r0(cpu, mem, disp, base,
                                                      randgen32->pick_val(0),
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
        cpu->write_mem(&val, disp * 4 + base, sizeof(val));
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

    static int movl_binary_binind_disp_gen_gen(Sh4 *cpu, Memory *mem,
                                               RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                    for (int disp = 0; disp < 4; disp++) {
                        reg32_t base =
                            randgen32->pick_range(0, mem->get_size() - 4 - 0xf * 4);
                        uint32_t val = randgen32->pick_val(0);
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

    // MOV.B Rm, @(R0, Rn)
    // 0000nnnnmmmm0100
    static int do_movb_gen_binind_r0_gen(Sh4 *cpu, Memory *mem, reg32_t src_val,
                                         reg32_t r0_val, reg32_t base_val,
                                         int reg_src, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        if (reg_src == 0)
            src_val = r0_val;

        if (reg_src == reg_base)
            src_val = base_val;

        ss << "MOV.B R" << reg_src << ", @(R0, R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = src_val;
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->exec_inst();

        uint8_t mem_val;
        cpu->read_mem(&mem_val, r0_val + base_val, sizeof(mem_val));

        if (mem_val != uint8_t(src_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_gen_binind_r0_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 1) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 1) / 2;

        failure = failure ||
            do_movb_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 1) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 1) / 2;

                failure = failure ||
                    do_movb_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }


    // MOV.W R0, @(disp, Rn)
    // 10000001nnnndddd
    static int do_movw_gen_binind_r0_gen(Sh4 *cpu, Memory *mem, reg32_t src_val,
                                         reg32_t r0_val, reg32_t base_val,
                                         int reg_src, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        if (reg_src == 0)
            src_val = r0_val;

        if (reg_src == reg_base)
            src_val = base_val;

        ss << "MOV.W R" << reg_src << ", @(R0, R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = src_val;
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->exec_inst();

        uint16_t mem_val;
        cpu->read_mem(&mem_val, r0_val + base_val, sizeof(mem_val));

        if (mem_val != uint16_t(src_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val <<
                std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_gen_binind_r0_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 2) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 2) / 2;

        failure = failure ||
            do_movw_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 2) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 2) / 2;

                failure = failure ||
                    do_movw_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }

    // MOV.L Rm, @(disp, Rn)
    // 0001nnnnmmmmdddd
    static int do_movl_gen_binind_r0_gen(Sh4 *cpu, Memory *mem, reg32_t src_val,
                                         reg32_t r0_val, reg32_t base_val,
                                         int reg_src, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        if (reg_src == 0)
            src_val = r0_val;

        if (reg_src == reg_base)
            src_val = base_val;

        ss << "MOV.L R" << reg_src << ", @(R0, R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_src) = src_val;
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, r0_val + base_val, sizeof(mem_val));

        if (mem_val != uint32_t(src_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_gen_binind_r0_gen(Sh4 *cpu, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 4) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 4) / 2;

        failure = failure ||
            do_movl_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 4) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 4) / 2;

                failure = failure ||
                    do_movl_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }

    // MOV.B @(R0, Rm), Rn
    // 0000nnnnmmmm1100
    static int do_binary_movb_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                                int8_t src_val, reg32_t r0_val,
                                                reg32_t base_val, int reg_dst,
                                                int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        ss << "MOV.B @(R0, R" << reg_base << "), R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->write_mem(&src_val, r0_val + base_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(src_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movb_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 1) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 1) / 2;

        failure = failure ||
            do_movb_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 1) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 1) / 2;

                failure = failure ||
                    do_binary_movb_binind_r0_gen_gen(cpu, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.W @(R0, Rm), Rn
    // 0000nnnnmmmm1101
    static int do_binary_movw_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                                int16_t src_val, reg32_t r0_val,
                                                reg32_t base_val, int reg_dst,
                                                int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        ss << "MOV.W @(R0, R" << reg_base << "), R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->write_mem(&src_val, r0_val + base_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != int32_t(src_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movw_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 2) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 2) / 2;
        failure = failure ||
            do_movw_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 2) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 2) / 2;

                failure = failure ||
                    do_binary_movw_binind_r0_gen_gen(cpu, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.L @(R0, Rm), Rn
    // 0000nnnnmmmm1110
    static int do_binary_movl_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                                int32_t src_val, reg32_t r0_val,
                                                reg32_t base_val, int reg_dst,
                                                int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        ss << "MOV.L @(R0, R" << reg_base << "), R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_base) = base_val;
        *cpu->gen_reg(0) = r0_val;
        cpu->write_mem(&src_val, r0_val + base_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != src_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *cpu->gen_reg(reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movl_binind_r0_gen_gen(Sh4 *cpu, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (randgen32->pick_range(0, mem->get_size()) - 4) / 2;
        addr32_t r0_val = (randgen32->pick_range(0, mem->get_size()) - 4) / 2;
        failure = failure ||
            do_movl_gen_binind_r0_gen(cpu, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (randgen32->pick_range(0, mem->get_size()) - 4) / 2;
                addr32_t r0_val =
                    (randgen32->pick_range(0, mem->get_size()) - 4) / 2;

                failure = failure ||
                    do_binary_movl_binind_r0_gen_gen(cpu, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.B R0, @(disp, GBR)
    // 11000000dddddddd
    static int do_binary_movb_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.B R0, @(" << (unsigned)disp << ", GBR)\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(0) = r0_val;
        cpu->reg.gbr = gbr_val;
        cpu->exec_inst();

        int8_t mem_val;
        cpu->read_mem(&mem_val, disp + gbr_val, sizeof(mem_val));
        if (mem_val != int8_t(r0_val)) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex << r0_val <<
                std::endl;
            std::cout << "actual value was " << std::hex << (unsigned)mem_val <<
                std::endl;
            std::cout << "R0 value was " << std::hex << r0_val << std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movb_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 1 - disp);
            failure = failure ||
                do_binary_movb_r0_binind_disp_gbr(cpu, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.W R0, @(disp, GBR)
    // 11000001dddddddd
    static int do_binary_movw_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.W R0, @(" << (unsigned)disp << ", GBR)\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(0) = r0_val;
        cpu->reg.gbr = gbr_val;
        cpu->exec_inst();

        int16_t mem_val;
        cpu->read_mem(&mem_val, disp * 2 + gbr_val, sizeof(mem_val));
        if (mem_val != int16_t(r0_val)) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex << r0_val <<
                std::endl;
            std::cout << "actual value was " << std::hex << (unsigned)mem_val <<
                std::endl;
            std::cout << "R0 value was " << std::hex << r0_val << std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movw_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = randgen32->pick_val(0) % mem->get_size();
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 2 - disp * 2);
            failure = failure ||
                do_binary_movw_r0_binind_disp_gbr(cpu, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.L R0, @(disp, GBR)
    // 11000010dddddddd
    static int do_binary_movl_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.L R0, @(" << (unsigned)disp << ", GBR)\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(0) = r0_val;
        cpu->reg.gbr = gbr_val;
        cpu->exec_inst();

        int32_t mem_val;
        cpu->read_mem(&mem_val, disp * 4 + gbr_val, sizeof(mem_val));
        if (mem_val != int32_t(r0_val)) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex << r0_val <<
                std::endl;
            std::cout << "actual value was " << std::hex << (unsigned)mem_val <<
                std::endl;
            std::cout << "R0 value was " << std::hex << r0_val << std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movl_r0_binind_disp_gbr(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = randgen32->pick_val(0) % mem->get_size();
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 4 - disp * 4);
            failure = failure ||
                do_binary_movl_r0_binind_disp_gbr(cpu, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.B @(disp, GBR), R0
    // 11000100dddddddd
    static int do_binary_movb_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                                 int8_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.B @(" << (unsigned)disp << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.gbr = gbr_val;
        cpu->write_mem(&src_val, disp + gbr_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(0) != int32_t(src_val)) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                int32_t(src_val) << std::endl;
            std::cout << "actual value was " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movb_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 1 - disp);
            failure = failure ||
                do_binary_movb_binind_disp_gbr_r0(cpu, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.W @(disp, GBR), R0
    // 11000101dddddddd
    static int do_binary_movw_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                                 int16_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.W @(" << (unsigned)disp << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.gbr = gbr_val;
        cpu->write_mem(&src_val, disp * 2 + gbr_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(0) != int32_t(src_val)) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                int32_t(src_val) << std::endl;
            std::cout << "actual value was " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movw_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 2 - disp * 2);
            failure = failure ||
                do_binary_movw_binind_disp_gbr_r0(cpu, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.L @(disp, GBR), R0
    // 11000110dddddddd
    static int do_binary_movl_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                                 int32_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.L @(" << (unsigned)disp << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.gbr = gbr_val;
        cpu->write_mem(&src_val, disp * 4 + gbr_val, sizeof(src_val));
        cpu->exec_inst();

        if (*cpu->gen_reg(0) != src_val) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                src_val << std::endl;
            std::cout << "actual value was " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movl_binind_disp_gbr_r0(Sh4 *cpu, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                randgen32->pick_range(0, mem->get_size() - 4 - disp * 4);
            failure = failure ||
                do_binary_movl_binind_disp_gbr_r0(cpu, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOVA @(disp, PC), R0
    // 11000111dddddddd
    static int do_binary_mova_binind_disp_pc_r0(Sh4 *cpu, Memory *mem,
                                                uint8_t disp, reg32_t pc_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOVA @(" << (unsigned)disp << ", PC), R0\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(pc_val, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg.pc = pc_val;
        cpu->exec_inst();

        reg32_t expected_val = disp * 4 + (pc_val & ~3) + 4;

        if (*cpu->gen_reg(0) != expected_val) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                expected_val << std::endl;
            std::cout << "actual value was " << std::hex << *cpu->gen_reg(0) <<
                std::endl;
            std::cout << "PC value was " << std::hex << pc_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_mova_binind_disp_pc_r0(Sh4 *cpu, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t pc_val = randgen32->pick_range(0, (mem->get_size() - 4 - disp * 4) & ~1);
            failure = failure ||
                do_binary_mova_binind_disp_pc_r0(cpu, mem, disp, pc_val);
        }

        return failure;
    }

    // LDC Rm, SR
    // 0100mmmm00001110
    static int do_binary_ldc_gen_sr(Sh4 *cpu, Memory *mem,
                                    unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.sr != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.sr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_sr(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_sr(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, GBR
    // 0100mmmm00011110
    static int do_binary_ldc_gen_gbr(Sh4 *cpu, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", GBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.gbr != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.gbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_gbr(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_gbr(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, VBR
    // 0100mmmm00101110
    static int do_binary_ldc_gen_vbr(Sh4 *cpu, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", VBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.vbr != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.vbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_vbr(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_vbr(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, SSR
    // 0100mmmm00111110
    static int do_binary_ldc_gen_ssr(Sh4 *cpu, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SSR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.ssr != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.ssr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_ssr(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_ssr(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, SPC
    // 0100mmmm01001110
    static int do_binary_ldc_gen_spc(Sh4 *cpu, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SPC\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.spc != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.spc << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_spc(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_spc(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, DBR
    // 0100mmmm11111010
    static int do_binary_ldc_gen_dbr(Sh4 *cpu, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", DBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        if (cpu->reg.dbr != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg.dbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_dbr(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_dbr(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, Rn_BANK
    // 0100mmmm1nnn1110
    static int do_binary_ldc_gen_bank(Sh4 *cpu, Memory *mem,
                                      unsigned reg_no, unsigned bank_reg_no,
                                      reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", R" << bank_reg_no << "_BANK\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = reg_val;
        cpu->exec_inst();

        reg32_t bank_reg_val = *cpu->bank_reg(bank_reg_no);

        if (bank_reg_val != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex << bank_reg_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_bank(Sh4 *cpu, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                failure = failure ||
                    do_binary_ldc_gen_bank(cpu, mem, reg_no, bank_reg_no,
                                           randgen32->pick_val(0));
            }
        }

        return failure;
    }

    // LDC.L @Rm+, SR
    // 0100mmmm00000111
    static int do_binary_ldcl_indgeninc_sr(Sh4 *cpu, Memory *mem,
                                           unsigned reg_src, addr32_t addr,
                                           uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        /*
         * Need to restore the original SR because editing SR can cause us to
         * do things that interfere with the test (such as bank-switching).
         */
        reg32_t old_sr = cpu->reg.sr;
        cpu->exec_inst();
        reg32_t new_sr = cpu->reg.sr;
        cpu->reg.sr = old_sr;

        if ((new_sr != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << new_sr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_sr(Sh4 *cpu, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_sr(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, GBR
    // 0100mmmm00010111
    static int do_binary_ldcl_indgeninc_gbr(Sh4 *cpu, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, GBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        cpu->exec_inst();

        if ((cpu->reg.gbr != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg.gbr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_gbr(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_gbr(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, VBR
    // 0100mmmm00100111
    static int do_binary_ldcl_indgeninc_vbr(Sh4 *cpu, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, VBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        cpu->exec_inst();

        if ((cpu->reg.vbr != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg.vbr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_vbr(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_vbr(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, SSR
    // 0100mmmm00110111
    static int do_binary_ldcl_indgeninc_ssr(Sh4 *cpu, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SSR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        cpu->exec_inst();

        if ((cpu->reg.ssr != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg.ssr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_ssr(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_ssr(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, SPC
    // 0100mmmm01000111
    static int do_binary_ldcl_indgeninc_spc(Sh4 *cpu, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SPC\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        cpu->exec_inst();

        if ((cpu->reg.spc != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg.spc << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_spc(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_spc(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, DBR
    // 0100mmmm11110110
    static int do_binary_ldcl_indgeninc_dbr(Sh4 *cpu, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, DBR\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->gen_reg(reg_src) = addr;
        cpu->write_mem(&val, addr, sizeof(val));

        cpu->exec_inst();

        if ((cpu->reg.dbr != val) || (*cpu->gen_reg(reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg.dbr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *cpu->gen_reg(reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_dbr(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_dbr(cpu, mem, reg_src, addr, val);
        }

        return failure;
    }

    // STC SR, Rn
    // 0000nnnn00000010
    static int do_binary_stc_sr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                    reg32_t sr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        /*
         * using random values for SR is a little messy 'cause it has side
         * effects.  In the future I may decide not to use random values for
         * this test.
         */
        sr_val |= Sh4::SR_MD_MASK;

        ss << "STC SR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.sr = sr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != sr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.sr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_sr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_sr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC GBR, Rn
    // 0000nnnn00010010
    static int do_binary_stc_gbr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t gbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC GBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.gbr = gbr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != gbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << gbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.gbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_gbr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_gbr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC VBR, Rn
    // 0000nnnn00100010
    static int do_binary_stc_vbr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t vbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC VBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.vbr = vbr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != vbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << vbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.vbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_vbr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_vbr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SSR, Rn
    // 0000nnnn00110010
    static int do_binary_stc_ssr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t ssr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SSR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.ssr = ssr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != ssr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << ssr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.ssr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_ssr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_ssr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SPC, Rn
    // 0000nnnn01000010
    static int do_binary_stc_spc_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t spc_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SPC, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.spc = spc_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != spc_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << spc_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.spc << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_spc_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_spc_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SGR, Rn
    // 0000nnnn00111010
    static int do_binary_stc_sgr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t sgr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SGR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.sgr = sgr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != sgr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sgr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.sgr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_sgr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_sgr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC DBR, Rn
    // 0000nnnn11111010
    static int do_binary_stc_dbr_gen(Sh4 *cpu, Memory *mem, unsigned reg_dst,
                                     reg32_t dbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC DBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.dbr = dbr_val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_dst) != dbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << dbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg.dbr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_dbr_gen(Sh4 *cpu, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_dbr_gen(cpu, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC.L SR, @-Rn
    // 0100nnnn00000011
    static int do_binary_stcl_sr_inddecgen(Sh4 *cpu, Memory *mem,
                                           unsigned reg_no, reg32_t sr_val,
                                           addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // obviously this needs to run in privileged mode
        sr_val |= Sh4::SR_MD_MASK;

        ss << "STC.L SR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.sr = sr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (sr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_sr_inddecgen(Sh4 *cpu, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_sr_inddecgen(cpu, mem, reg_no,
                                            randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L GBR, @-Rn
    // 0100nnnn00010011
    static int do_binary_stcl_gbr_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t gbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L GBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.gbr = gbr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (gbr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << gbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_gbr_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_gbr_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L VBR, @-Rn
    // 01n00nnnn00100011
    static int do_binary_stcl_vbr_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t vbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L VBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.vbr = vbr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (vbr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << vbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_vbr_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_vbr_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SSR, @-Rn
    // 0100nnnn00110011
    static int do_binary_stcl_ssr_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t ssr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SSR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.ssr = ssr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (ssr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << ssr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_ssr_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_ssr_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SPC, @-Rn
    // 0100nnnn01000011
    static int do_binary_stcl_spc_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t spc_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SPC, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.spc = spc_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (spc_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << spc_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_spc_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_spc_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SGR, @-Rn
    // 0100nnnn00110010
    static int do_binary_stcl_sgr_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t sgr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SGR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.sgr = sgr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (sgr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sgr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_sgr_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_sgr_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L DBR, @-Rn
    // 0100nnnn11110010
    static int do_binary_stcl_dbr_inddecgen(Sh4 *cpu, Memory *mem,
                                            unsigned reg_no, reg32_t dbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L DBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg.dbr = dbr_val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (dbr_val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << dbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_dbr_inddecgen(Sh4 *cpu, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
            failure = failure ||
                do_binary_stcl_dbr_inddecgen(cpu, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // LDC.L @Rm+, Rn_BANK
    // 0100mmmm1nnn0111
    static int do_binary_ldcl_indgeninc_bank(Sh4 *cpu, Memory *mem,
                                             unsigned reg_no,
                                             unsigned bank_reg_no,
                                             addr32_t addr, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_no << "+, R" << bank_reg_no << "_BANK\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *cpu->gen_reg(reg_no) = addr;
        cpu->write_mem(&val, addr, sizeof(val));
        cpu->exec_inst();

        reg32_t bank_reg_val = *cpu->bank_reg(bank_reg_no);

        if (bank_reg_val != val || *cpu->gen_reg(reg_no) != (addr + 4)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input address is " << addr << std::endl;
            std::cout << "val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << std::hex << bank_reg_val <<
                std::endl;
            std::cout << "expected output address is " << addr + 4 << std::endl;
            std::cout << "actual output address is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_bank(Sh4 *cpu, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                addr32_t addr = randgen32->pick_range(0, mem->get_size() - 5);
                failure = failure ||
                    do_binary_ldcl_indgeninc_bank(cpu, mem, reg_no, bank_reg_no,
                                                  addr, randgen32->pick_val(0));
            }
        }

        return failure;
    }

    // STC Rm_BANK, Rn
    // 0000nnnn1mmm0010
    static int do_binary_stc_bank_gen(Sh4 *cpu, Memory *mem,
                                      unsigned bank_reg_no, unsigned reg_no,
                                      reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC R" << bank_reg_no << "_BANK, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->bank_reg(bank_reg_no) = val;
        cpu->exec_inst();

        if (*cpu->gen_reg(reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << val <<
                std::endl;
            std::cout << "Actual value is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_bank_gen(Sh4 *cpu, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                failure = failure ||
                    do_binary_stc_bank_gen(cpu, mem, bank_reg_no, reg_no,
                                           randgen32->pick_val(0));
            }
        }
        return failure;
    }

    // STC.L Rm_BANK, @-Rn
    // 0100nnnn1mmm0011
    static int do_binary_stcl_bank_inddecgen(Sh4 *cpu, Memory *mem,
                                             unsigned bank_reg_no,
                                             unsigned reg_no, reg32_t val,
                                             addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L R" << bank_reg_no << "_BANK, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.assemble(cmd);
        const Sh4Prog::InstList& inst = test_prog.get_prog();
        mem->load_program(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *cpu->bank_reg(bank_reg_no) = val;
        *cpu->gen_reg(reg_no) = addr;
        cpu->exec_inst();

        uint32_t mem_val;
        cpu->read_mem(&mem_val, addr - 4, sizeof(mem_val));

        if (val != mem_val || *cpu->gen_reg(reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *cpu->gen_reg(reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_bank_inddecgen(Sh4 *cpu, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                addr32_t addr = randgen32->pick_range(4, mem->get_size() - 4);
                failure = failure ||
                    do_binary_stcl_bank_inddecgen(cpu, mem, bank_reg_no, reg_no,
                                                  randgen32->pick_val(0), addr);
            }
        }

        return failure;
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
    { "movb_gen_binind_r0_gen", &Sh4InstTests::movb_gen_binind_r0_gen },
    { "movw_gen_binind_r0_gen", &Sh4InstTests::movw_gen_binind_r0_gen },
    { "movl_gen_binind_r0_gen", &Sh4InstTests::movl_gen_binind_r0_gen },
    { "binary_movb_binind_r0_gen_gen",
      &Sh4InstTests::binary_movb_binind_r0_gen_gen },
    { "binary_movw_binind_r0_gen_gen",
      &Sh4InstTests::binary_movw_binind_r0_gen_gen },
    { "binary_movl_binind_r0_gen_gen",
      &Sh4InstTests::binary_movl_binind_r0_gen_gen },
    { "binary_movb_r0_binind_disp_gbr",
      &Sh4InstTests::binary_movb_r0_binind_disp_gbr },
    { "binary_movw_r0_binind_disp_gbr",
      &Sh4InstTests::binary_movw_r0_binind_disp_gbr },
    { "binary_movl_r0_binind_disp_gbr",
      &Sh4InstTests::binary_movl_r0_binind_disp_gbr },
    { "binary_movb_binind_disp_gbr_r0",
      &Sh4InstTests::binary_movb_binind_disp_gbr_r0 },
    { "binary_movw_binind_disp_gbr_r0",
      &Sh4InstTests::binary_movw_binind_disp_gbr_r0 },
    { "binary_movl_binind_disp_gbr_r0",
      &Sh4InstTests::binary_movl_binind_disp_gbr_r0 },
    { "binary_mova_binind_disp_pc_r0",
      &Sh4InstTests::binary_mova_binind_disp_pc_r0 },
    { "binary_ldc_gen_sr", &Sh4InstTests::binary_ldc_gen_sr },
    { "binary_ldc_gen_gbr", &Sh4InstTests::binary_ldc_gen_gbr },
    { "binary_ldc_gen_vbr", &Sh4InstTests::binary_ldc_gen_vbr },
    { "binary_ldc_gen_ssr", &Sh4InstTests::binary_ldc_gen_ssr },
    { "binary_ldc_gen_spc", &Sh4InstTests::binary_ldc_gen_spc },
    { "binary_ldc_gen_bank", &Sh4InstTests::binary_ldc_gen_bank },
    { "binary_ldcl_indgeninc_sr", &Sh4InstTests::binary_ldcl_indgeninc_sr },
    { "binary_ldcl_indgeninc_gbr", &Sh4InstTests::binary_ldcl_indgeninc_gbr },
    { "binary_ldcl_indgeninc_vbr", &Sh4InstTests::binary_ldcl_indgeninc_vbr },
    { "binary_ldcl_indgeninc_ssr", &Sh4InstTests::binary_ldcl_indgeninc_ssr },
    { "binary_ldcl_indgeninc_spc", &Sh4InstTests::binary_ldcl_indgeninc_spc },
    { "binary_ldcl_indgeninc_dbr", &Sh4InstTests::binary_ldcl_indgeninc_dbr },
    { "binary_stc_sr_gen", &Sh4InstTests::binary_stc_sr_gen },
    { "binary_stc_gbr_gen", &Sh4InstTests::binary_stc_gbr_gen },
    { "binary_stc_vbr_gen", &Sh4InstTests::binary_stc_vbr_gen },
    { "binary_stc_ssr_gen", &Sh4InstTests::binary_stc_ssr_gen },
    { "binary_stc_spc_gen", &Sh4InstTests::binary_stc_spc_gen },
    { "binary_stc_sgr_gen", &Sh4InstTests::binary_stc_sgr_gen },
    { "binary_stc_dbr_gen", &Sh4InstTests::binary_stc_dbr_gen },
    { "binary_stcl_sr_inddecgen", &Sh4InstTests::binary_stcl_sr_inddecgen },
    { "binary_stcl_gbr_inddecgen", &Sh4InstTests::binary_stcl_gbr_inddecgen },
    { "binary_stcl_vbr_inddecgen", &Sh4InstTests::binary_stcl_vbr_inddecgen },
    { "binary_stcl_ssr_inddecgen", &Sh4InstTests::binary_stcl_ssr_inddecgen },
    { "binary_stcl_spc_inddecgen", &Sh4InstTests::binary_stcl_spc_inddecgen },
    { "binary_stcl_sgr_inddecgen", &Sh4InstTests::binary_stcl_sgr_inddecgen },
    { "binary_stcl_dbr_inddecgen", &Sh4InstTests::binary_stcl_dbr_inddecgen },
    { "binary_ldcl_indgeninc_bank", &Sh4InstTests::binary_ldcl_indgeninc_bank },
    { "binary_stc_bank_gen", &Sh4InstTests::binary_stc_bank_gen },
    { "binary_stcl_bank_inddecgen", &Sh4InstTests::binary_stcl_bank_inddecgen },
    { NULL }
};

int main(int argc, char **argv) {
    Memory mem(16 * 1024 * 1024);
    Sh4 cpu(&mem);
    struct inst_test *test = inst_tests;
    int n_success = 0, n_tests = 0;
    unsigned int seed = time(NULL);
    int opt;

    while ((opt = getopt(argc, argv, "s:")) > 0) {
        if (opt == 's')
            seed = atoi(optarg);
    }

    try {
        RandGen32 randgen32(seed);
        randgen32.reset();

        while (test->name) {
            std::cout << "Trying " << test->name << "..." << std::endl;

            int test_ret = test->func(&cpu, &mem, &randgen32);

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
