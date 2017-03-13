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

#include <unistd.h>
#include <iostream>
#include <sstream>
#include <limits>

#include "BaseException.hpp"
#include "Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "tool/sh4asm/sh4asm.hpp"
#include "RandGenerator.hpp"
#include "arch/arch_fpu.hpp"

#ifdef ENABLE_SH4_ICACHE
#include "hw/sh4/Icache.hpp"
#endif

#ifdef ENABLE_SH4_OCACHE
#include "hw/sh4/Ocache.hpp"
#endif

static const size_t MEM_SZ = 16 * 1024 * 1024;

typedef RandGenerator<boost::uint32_t> RandGen32;
typedef int(*inst_test_func_t)(Sh4 *cpu, BiosFile *bios, Memory *mem,
                               RandGen32 *randgen32);

class AddrRange {
public:
    AddrRange(RandGen32 *randgen32,
              addr32_t min = 0, addr32_t max = MEM_SZ - 1) {
        this->randgen32 = randgen32;
        this->min = min;
        this->max = max;
    }

    addr32_t operator()() {
        return randgen32->pick_range(min, max);
    }

private:
    RandGen32 *randgen32;
    addr32_t min;
    addr32_t max;
};

template <class AddrFunc>
static addr32_t pick_addr(AddrFunc func) {
    return func() + ADDR_RAM_FIRST;
}

class Sh4InstTests {
public:
    /*
     * Put the cpu in a "clean" default state.
     */
    static void reset_cpu(Sh4 *cpu) {
        sh4_on_hard_reset(cpu);

#ifdef ENABLE_SH4_OCACHE
        sh4_ocache_reset(&cpu->op_cache);
#endif
#ifdef ENABLE_SH4_ICACHE
        sh4_icache_reset(&cpu->inst_cache);
#endif

        sh4_enter(cpu);
    }

    // very basic test that does a whole lot of nothing
    static int nop_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                        RandGen32 *randgen32) {
        Sh4Prog test_prog;
        test_prog.add_txt("NOP\n");
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t,
                         Sh4Prog::ByteList::const_iterator>(0, inst.begin(),
                                                            inst.end());

        reset_cpu(cpu);

        sh4_exec_inst(cpu);

        return 0;
    }

    // ADD #imm, Rn
    // 0111nnnniiiiiiii
    static int add_immed_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                              RandGen32 *randgen32) {
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg_no) = initial_val;
                sh4_exec_inst(cpu);

                reg32_t expected_val = (initial_val + int32_t(int8_t(imm_val)));
                reg32_t actual_val = *sh4_gen_reg(cpu, reg_no);

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
    static int add_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                RandGen32 *randgen32) {
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;
                sh4_exec_inst(cpu);

                reg32_t expected_val = (initial_val1 + initial_val2);
                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);

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
    static int do_addc_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    reg32_t src1, reg32_t src2, bool carry_in) {
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;

                if (carry_in)
                    cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;

                sh4_exec_inst(cpu);

                reg32_t expected_val = (initial_val2 + initial_val1);
                bool expected_carry = false;
                if (initial_val2 > expected_val)
                    expected_carry = true;
                if (carry_in) {
                    expected_val++;
                    if ((initial_val2 + initial_val1) > expected_val)
                        expected_carry = true;
                }

                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);
                bool actual_carry = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

                if (actual_val != expected_val || expected_carry != actual_carry) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "carry_in was " << carry_in << std::endl;
                    std::cout << "initial_val1 is " << std::hex <<
                        initial_val1 << std::endl;
                    std::cout << "initial_val2 is " << std::hex <<
                        initial_val2 << std::endl;
                    std::cout << "expected_val is " << expected_val <<
                        std::endl;
                    std::cout << "expected_carry is " << expected_carry <<
                        std::endl;
                    std::cout << "actual_val is " << actual_val << std::endl;
                    std::cout << "actual_carry is " << actual_carry <<
                        std::endl;
                    return 1;
                }
            }
        }
        return 0;
    }

    // ADDC Rm, Rn
    // 0011nnnnmmmm1110
    static int addc_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_addc_gen_gen_test(cpu, bios, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0), false);
        failed = failed || do_addc_gen_gen_test(cpu, bios, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0), true);

        // make sure we get at least one value in that should not cause a carry
        failed = failed || do_addc_gen_gen_test(cpu, bios, mem, 0, 0, false);
        failed = failed || do_addc_gen_gen_test(cpu, bios, mem, 0, 0, true);

        // make sure we get at least one value in that should cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max(), false);
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max(), true);

        // test a value that should *almost* cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem, 1,
                                 std::numeric_limits<reg32_t>::max() - 1,
                                 false);
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem, 1,
                                 std::numeric_limits<reg32_t>::max() - 1,
                                 true);

        // test a value pair that should barely cause a carry
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<reg32_t>::max() - 1, 2,
                                 false);
        failed = failed ||
            do_addc_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<reg32_t>::max() - 1, 2,
                                 true);

        return failed;
    }

    // ADDV Rm, Rn
    // 0011nnnnmmmm1111
    static int do_addv_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;
                sh4_exec_inst(cpu);

                reg32_t expected_val = (initial_val1 + initial_val2);
                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }

                // now check the overflow-bit
                bool overflow_flag = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK;
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
    static int addv_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failed = 0;
        randgen32->reset();

        // this should not overflow
        failed = failed || do_addv_gen_gen_test(cpu, bios, mem, 0, 0);

        // random values for good measure
        failed = failed || do_addv_gen_gen_test(cpu, bios, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0));

        // *almost* overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem, 1,
                                 std::numeric_limits<int32_t>::max() - 1);

        // slight overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem, 2,
                                 std::numeric_limits<int32_t>::max() - 1);

        // massive overflow positive to negative
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::max());

        // *almost* overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<int32_t>::min() + 1, 1);

        // slight overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<int32_t>::min() + 1, 2);

        // massive overflow negative to positive
        failed = failed ||
            do_addv_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<int32_t>::min(),
                                 std::numeric_limits<int32_t>::min());

        return failed;
    }

    // SUB Rm, Rn
    // 0011nnnnmmmm1000
    static int sub_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                RandGen32 *randgen32) {
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;
                sh4_exec_inst(cpu);

                reg32_t expected_val = initial_val2 - initial_val1;
                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);

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
    static int do_subc_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    reg32_t src1, reg32_t src2, bool carry_in) {
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;
                if (carry_in)
                    cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;

                sh4_exec_inst(cpu);

                reg32_t expected_val = initial_val2 - initial_val1;
                bool expected_carry = false;

                if (initial_val2 < expected_val)
                    expected_carry = true;
                if (carry_in) {
                    expected_val -= 1;
                    if ((initial_val2 - initial_val1) < expected_val)
                        expected_carry = true;
                }

                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);
                bool actual_carry = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK;

                if ((actual_val != expected_val) ||
                    (actual_carry != expected_carry)) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "initial_val1 is " << std::hex <<
                        initial_val1 << std::endl;
                    std::cout << "initial_val2 is " << initial_val2 <<
                        std::endl;
                    std::cout << "expected_val is " << expected_val <<
                        std::endl;
                    std::cout << "expected_carry is " << expected_carry <<
                        std::endl;
                    std::cout << "actual_val is " << actual_val << std::endl;
                    std::cout << "carry_in is " << carry_in << std::endl;
                    return 1;
                }
            }
        }
        return 0;
    }

    static int subc_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failed = 0;

        // run the test with a couple random values
        failed = failed || do_subc_gen_gen_test(cpu, bios, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0), false);
        failed = failed || do_subc_gen_gen_test(cpu, bios, mem,
                                                randgen32->pick_val(0),
                                                randgen32->pick_val(0), true);


        // make sure we get at least one value in that should not cause a carry
        failed = failed || do_subc_gen_gen_test(cpu, bios, mem, 0, 0, false);
        failed = failed || do_subc_gen_gen_test(cpu, bios, mem, 0, 0, true);

        // make sure we get at least one value in that should cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<reg32_t>::max(), 0, false);
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem,
                                 std::numeric_limits<reg32_t>::max(), 0, true);


        // test a value that should *almost* cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max(), false);
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem, std::numeric_limits<reg32_t>::max(),
                                 std::numeric_limits<reg32_t>::max(), true);


        // test a value pair that should barely cause a carry
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem, 1, 0, false);
        failed = failed ||
            do_subc_gen_gen_test(cpu, bios, mem, 1, 0, true);


        return failed;
    }

    static int do_subv_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                *sh4_gen_reg(cpu, reg1_no) = initial_val1;
                *sh4_gen_reg(cpu, reg2_no) = initial_val2;
                sh4_exec_inst(cpu);

                reg32_t expected_val = initial_val2 - initial_val1;
                reg32_t actual_val = *sh4_gen_reg(cpu, reg2_no);

                if (actual_val != expected_val) {
                    std::cout << "ERROR running: " << std::endl
                              << "\t" << ss.str() << std::endl;
                    std::cout << "Expected " << std::hex <<
                        (initial_val1 + initial_val2) << " but got " <<
                        actual_val << std::endl;
                    return 1;
                }

                // now check the overflow-bit
                bool overflow_flag = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK;
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

    static int subv_gen_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failed = 0;

        // do one at random...
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 randgen32->pick_val(0), randgen32->pick_val(0));

        // now do one that's trivial
        failed = failed || do_subv_gen_gen_test(cpu, bios, mem, 0, 0);

        // now do one that *almost* causes a negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1), 0);

        // now do one that barely causes a negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1), -1);

        // now do a massive negative overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -(std::numeric_limits<int32_t>::min() + 1),
                                 std::numeric_limits<int32_t>::min());

        // now do one that *almost* causes a positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -std::numeric_limits<int32_t>::max(), 0);

        // now do one that barely causes a positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -std::numeric_limits<int32_t>::max(), 1);

        // now do a massive positive overflow
        failed = failed ||
            do_subv_gen_gen_test(cpu, bios, mem,
                                 -std::numeric_limits<int32_t>::max(),
                                 std::numeric_limits<int32_t>::max());

        return failed;
    }

    // MOVT Rn
    // 0000nnnn00101001
    static int movt_unary_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned t_val = 0; t_val < 2; t_val++) {
                Sh4Prog test_prog;
                std::stringstream ss;

                ss << "MOVT R" << reg_no << "\n";
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
                if (t_val)
                    cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;

                 sh4_exec_inst(cpu);

                if (*sh4_gen_reg(cpu, reg_no) != t_val)
                    return 1;
            }
        }

        return 0;
    }

    // MOV #imm, Rn
    // 1110nnnniiiiiiii
    static int mov_binary_imm_gen_test(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
                test_prog.add_txt(ss.str());
                const Sh4Prog::ByteList& inst = test_prog.get_prog();
                bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

                reset_cpu(cpu);

                sh4_exec_inst(cpu);

                if (*sh4_gen_reg(cpu, reg_no) != reg32_t(int32_t(int8_t(imm_val))))
                    return 1;
            }
        }

        return 0;
    }

    // MOV.W @(disp, PC), Rn
    // 1001nnnndddddddd
    static int do_movw_binary_binind_disp_pc_gen(Sh4 *cpu, BiosFile *bios,
                                                 Memory *mem, unsigned disp,
                                                 unsigned pc, unsigned reg_no,
                                                 int16_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV.W @(" << (disp * 2) << ", PC), R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_PC] = pc;
        sh4_write_mem(cpu, &mem_val, disp * 2 + pc + 4, sizeof(mem_val));

        sh4_exec_inst(cpu);

        if (int32_t(*sh4_gen_reg(cpu, reg_no)) != int32_t(mem_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "pc is " << std::hex << pc << std::endl;
            std::cout << "expected mem_val is " << std::hex << mem_val
                      << std::endl;
            std::cout << "actual mem_val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_binind_disp_pc_gen(Sh4 *cpu, BiosFile *bios,
                                              Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int disp = 0; disp < 256; disp++) {
            for (int reg_no = 0; reg_no < 16; reg_no++) {
                addr32_t pc_max = MEM_SZ - 1 - 4 - disp * 2;
                addr32_t pc_val = pick_addr(AddrRange(randgen32, 0, pc_max)) & ~1;

                failed = failed ||
                    do_movw_binary_binind_disp_pc_gen(cpu, bios, mem, disp,
                                                      pc_val, reg_no,
                                                      randgen32->pick_val(0) &
                                                      0xffff);
            }
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        addr32_t pc_val = pick_addr(AddrRange(randgen32)) & ~1;
        failed = failed ||
            do_movw_binary_binind_disp_pc_gen(cpu, bios, mem, 48, pc_val, 2,
                                              randgen32->pick_val(0) & 0xffff);
        return failed;
    }

    // MOV.L @(disp, PC), Rn
    // 1001nnnndddddddd
    static int do_movl_binary_binind_disp_pc_gen(Sh4 *cpu, BiosFile *bios,
                                                 Memory *mem, unsigned disp,
                                                 unsigned pc, unsigned reg_no,
                                                 int32_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV.L @(" << (disp * 4) << ", PC), R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_PC] = pc;
        sh4_write_mem(cpu, &mem_val, disp * 4 + (pc & ~3) + 4, sizeof(mem_val));

        sh4_exec_inst(cpu);

        if (int32_t(*sh4_gen_reg(cpu, reg_no)) != int32_t(mem_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "pc is " << std::hex << pc << std::endl;
            std::cout << "expected mem_val is " << std::hex << mem_val
                      << std::endl;
            std::cout << "actual mem_val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_binind_disp_pc_gen(Sh4 *cpu, BiosFile *bios,
                                              Memory *mem, RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned disp = 0; disp < 256; disp++) {
            for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            /*
             * the reason that pc_max gets OR'd with three is that those lower
             * two bits will get cleared when the instruction calculates the
             * actual address.
             */
            addr32_t pc_max = (MEM_SZ - 1 - 4 - disp * 4) | 3;
            addr32_t pc_val = pick_addr(AddrRange(randgen32, 0, pc_max)) & ~1;
            failed = failed ||
                do_movl_binary_binind_disp_pc_gen(cpu, bios, mem, disp, pc_val,
                                                  reg_no,
                                                  randgen32->pick_val(0));
            }
        }

        // not much rhyme or reason to this test case, but it did
        // actually catch a bug once
        addr32_t pc_val = pick_addr(AddrRange(randgen32)) & ~1;
        failed = failed ||
            do_movl_binary_binind_disp_pc_gen(cpu, bios, mem, 48, pc_val, 2,
                                              randgen32->pick_val(0));
        return failed;
    }

    // MOV Rm, Rn
    // 0110nnnnmmmm0011
    static int do_mov_binary_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     reg32_t src_val, unsigned reg_src,
                                     unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MOV R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != src_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int mov_binary_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                failed = failed ||
                    do_mov_binary_gen_gen(cpu, bios, mem, randgen32->pick_val(0),
                                          reg_src, reg_dst);
            }
        }
        return failed;
    }

    // MOV.B Rm, @Rn
    // 0010nnnnmmmm0000
    static int do_movb_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr, sizeof(mem_val));

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

    static int movb_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32));
                failed = failed ||
                    do_movb_binary_gen_indgen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W Rm, @Rn
    // 0010nnnnmmmm0001
    static int do_movw_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr, sizeof(mem_val));

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

    static int movw_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32));
                failed = failed ||
                    do_movw_binary_gen_indgen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0) % 0xffff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L Rm, @Rn
    // 0010nnnnmmmm0010
    static int do_movl_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         unsigned addr, uint32_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint32_t mem_val;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L R" << reg_src << ", @R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr, sizeof(mem_val));

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

    static int movl_binary_gen_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 4));
                failed = failed ||
                    do_movl_binary_gen_indgen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0),
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B @Rm, Rn
    // 0110nnnnmmmm0000
    static int do_movb_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         unsigned addr, int8_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.B @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movb_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32));
                failed = failed ||
                    do_movb_binary_indgen_gen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W @Rm, Rn
    // 0110nnnnmmmm0001
    static int do_movw_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         unsigned addr, int16_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.W @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movw_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 2));
                failed = failed ||
                    do_movw_binary_indgen_gen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L @Rm, Rn
    // 0110nnnnmmmm0010
    static int do_movl_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         unsigned addr, uint32_t val,
                                         unsigned reg_src, unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        if (reg_src == reg_dst)
            val = addr;

        ss << "MOV.L @R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int movl_binary_indgen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0,
                                                    memory_size(mem) - 4));
                failed = failed ||
                    do_movl_binary_indgen_gen(cpu, bios, mem, addr,
                                              randgen32->pick_val(0) % 0xff,
                                              reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B Rm, @-Rn
    // 0010nnnnmmmm0100
    static int do_movb_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr-1, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_dst) != addr - 1) {
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

    static int movb_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 1,
                                                    memory_size(mem) - 2));
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, bios, mem, addr,
                                                 randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W Rm, @-Rn
    // 0010nnnnmmmm0101
    static int do_movw_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr-2, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_dst) != addr - 2) {
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

    static int movw_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 2, MEM_SZ - 2));
                failed = failed ||
                    do_movw_binary_gen_inddecgen(cpu, bios, mem, addr,
                                                 randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L Rm, @-Rn
    // 0010nnnnmmmm0110
    static int do_movl_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr-4, sizeof(mem_val));

        if (mem_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                (unsigned)mem_val << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_dst) != addr - 4) {
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

    static int movl_binary_gen_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
                failed = failed ||
                    do_movl_binary_gen_inddecgen(cpu, bios, mem, addr,
                                                 randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B @Rm+, Rn
    // 0110nnnnmmmm0100
    static int do_movb_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_src) != 1 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
        }

        return 0;
    }

    static int movb_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32,
                                                    0, MEM_SZ - 2));
                failed = failed ||
                    do_movb_binary_gen_inddecgen(cpu, bios, mem, addr,
                                                 randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.W @Rm+, Rn
    // 0110nnnnmmmm0101
    static int do_movw_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != int32_t(val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_src) != 2 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
        }

        return 0;
    }

    static int movw_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 3));
                failed = failed ||
                    do_movw_binary_gen_inddecgen(cpu, bios, mem,
                                                 addr, randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.L @Rm+, Rn
    // 0110nnnnmmmm0110
    static int do_movl_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        if (*sh4_gen_reg(cpu, reg_src) != 4 + addr) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "The source register did not incrment properly" << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
        }

        return 0;
    }

    static int movl_binary_indgeninc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                failed = failed ||
                    do_movl_binary_gen_inddecgen(cpu, bios, mem, addr,
                                                 randgen32->pick_val(0),
                                                 reg_src, reg_dst);
            }
        }

        return failed;
    }

    // MOV.B R0, @(disp, Rn)
    // 10000000nnnndddd
    static int do_movb_binary_r0_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = val;
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_exec_inst(cpu);

        uint8_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp + base, sizeof(mem_val));
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

    static int movb_binary_r0_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base = pick_addr(AddrRange(randgen32,
                                                   0, MEM_SZ - 1 - 0xf));
                failed = failed ||
                    do_movb_binary_r0_binind_disp_gen(cpu, bios, mem, disp, base,
                                                      randgen32->pick_val(0),
                                                      reg_no);
            }
        }

        return failed;
    }

    static int do_movw_binary_r0_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 uint16_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.W R0, @(" << int(disp * 2) << ", R" << reg_base << ")\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = val;
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_exec_inst(cpu);

        uint16_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp * 2 + base, sizeof(mem_val));
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

    static int movw_binary_r0_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base = pick_addr(AddrRange(randgen32,
                                                   0, MEM_SZ - 2 - 0xf * 2));
                failed = failed ||
                    do_movw_binary_r0_binind_disp_gen(cpu, bios, mem, disp, base,
                                                      randgen32->pick_val(0),
                                                      reg_no);
            }
        }

        return failed;
    }

    static int do_movl_binary_gen_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                  uint8_t disp, reg32_t base,
                                                  uint32_t val, int reg_base,
                                                  int reg_src) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == reg_src) {
            val = base;
        }

        ss << "MOV.L R" << reg_src << ", @(" << int(disp * 4) << ", R" <<
            reg_base << ")\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp * 4 + base, sizeof(mem_val));
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

    static int movl_binary_gen_binind_disp_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                               RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_src = 0; reg_src < 16; reg_src++) {
            for (int reg_base = 0; reg_base < 16; reg_base++) {
                for (int disp = 0; disp < 4; disp++) {
                    reg32_t base = pick_addr(AddrRange(randgen32, 0,
                                                       MEM_SZ - 4 - 0xf * 4));
                    reg32_t val = randgen32->pick_val(0);
                    failed = failed ||
                        do_movl_binary_gen_binind_disp_gen(cpu, bios, mem, disp,
                                                           base, val,
                                                           reg_base, reg_src);
                }
            }
        }

        return failed;
    }

    // MOV.B @(disp, Rm), R0
    // 10000100mmmmdddd
    static int do_movb_binary_binind_disp_gen_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_write_mem(cpu, &val, disp + base, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            return 1;
        }
        return 0;
    }

    static int movb_binary_binind_disp_gen_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base = pick_addr(AddrRange(randgen32,
                                                   0, MEM_SZ - 1 - 0xf));
                failed = failed ||
                    do_movb_binary_binind_disp_gen_r0(cpu, bios, mem, disp, base,
                                                      randgen32->pick_val(0),
                                                      reg_no);
            }
        }
        return failed;
    }

    // MOV.W @(disp, Rm), R0
    // 10000101mmmmdddd
    static int do_movw_binary_binind_disp_gen_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 uint8_t disp, reg32_t base,
                                                 int16_t val, int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0) {
            val = base;
        }

        ss << "MOV.W @(" << int(disp * 2) << ", R" << reg_base << "), R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_write_mem(cpu, &val, disp * 2 + base, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            return 1;
        }
        return 0;
    }

    static int movw_binary_binind_disp_gen_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            for (int disp = 0; disp < 4; disp++) {
                reg32_t base = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 2 - 0xf * 2));
                failed = failed ||
                    do_movw_binary_binind_disp_gen_r0(cpu, bios, mem, disp, base,
                                                      randgen32->pick_val(0),
                                                      reg_no);
            }
        }
        return failed;
    }

    // MOV.L @(disp, Rm), Rn
    // 0101nnnnmmmmdddd
    static int do_movl_binary_binind_disp_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                  uint8_t disp, reg32_t base,
                                                  int32_t val, int reg_base,
                                                  int reg_dst) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;
        addr32_t addr = disp * 4 + base;

        if (reg_base == reg_dst) {
            val = base;
        }

        ss << "MOV.L @(" << int(disp * 4) << ", R" << reg_base << "), R" <<
            reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << std::hex << (unsigned)val << std::endl;
            std::cout << "disp is " << std::hex << (unsigned)disp << std::endl;
            std::cout << "base is " << std::hex << base << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            std::cout << "addr is " << addr << std::endl;
            return 1;
        }
        return 0;
    }

    static int movl_binary_binind_disp_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                               RandGen32 *randgen32) {
        int failed = 0;

        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                    for (int disp = 0; disp < 4; disp++) {
                        reg32_t base = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 4 - 0xf * 4));
                        uint32_t val = randgen32->pick_val(0);
                        failed = failed ||
                            do_movl_binary_binind_disp_gen_gen(cpu, bios, mem, disp,
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
    static int do_movb_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, reg32_t src_val,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        uint8_t mem_val;
        sh4_read_mem(cpu, &mem_val, r0_val + base_val, sizeof(mem_val));

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

    static int movb_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 1) / 2;
        addr32_t r0_val = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 1) / 2;

        failure = failure ||
            do_movb_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 1) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 1) / 2;

                failure = failure ||
                    do_movb_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }


    // MOV.W R0, @(disp, Rn)
    // 10000001nnnndddd
    static int do_movw_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, reg32_t src_val,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        uint16_t mem_val;
        sh4_read_mem(cpu, &mem_val, r0_val + base_val, sizeof(mem_val));

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

    static int movw_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 2) / 2;
        addr32_t r0_val = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 2) / 2;

        failure = failure ||
            do_movw_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);

        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 2) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 2) / 2;

                failure = failure ||
                    do_movw_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }

    // MOV.L Rm, @(disp, Rn)
    // 0001nnnnmmmmdddd
    static int do_movl_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, reg32_t src_val,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, r0_val + base_val, sizeof(mem_val));

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

    static int movl_gen_binind_r0_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;
        addr32_t r0_val = (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;

        failure = failure ||
            do_movl_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_src = 0; reg_src < 16; reg_src++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;

                failure = failure ||
                    do_movl_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                              r0_val, base_addr,
                                              reg_src, reg_base);
            }
        }

        return failure;
    }

    // MOV.B @(R0, Rm), Rn
    // 0000nnnnmmmm1100
    static int do_binary_movb_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_write_mem(cpu, &src_val, r0_val + base_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(src_val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movb_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (pick_addr(AddrRange(randgen32)) - 1) / 2;
        addr32_t r0_val = (pick_addr(AddrRange(randgen32)) - 1) / 2;

        failure = failure ||
            do_movb_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 1) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 1) / 2;

                failure = failure ||
                    do_binary_movb_binind_r0_gen_gen(cpu, bios, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.W @(R0, Rm), Rn
    // 0000nnnnmmmm1101
    static int do_binary_movw_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
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
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_write_mem(cpu, &src_val, r0_val + base_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != reg32_t(int32_t(src_val))) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movw_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr = (pick_addr(AddrRange(randgen32)) - 2) / 2;
        addr32_t r0_val = (pick_addr(AddrRange(randgen32)) - 2) / 2;
        failure = failure ||
            do_movw_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 2) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 2) / 2;

                failure = failure ||
                    do_binary_movw_binind_r0_gen_gen(cpu, bios, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.L @(R0, Rm), Rn
    // 0000nnnnmmmm1110
    static int do_binary_movl_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                uint32_t src_val,
                                                reg32_t r0_val,
                                                reg32_t base_val, int reg_dst,
                                                int reg_base) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        if (reg_base == 0)
            base_val = r0_val;

        ss << "MOV.L @(R0, R" << reg_base << "), R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_base) = base_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_write_mem(cpu, &src_val, r0_val + base_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != src_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << (unsigned)src_val << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "base_val is " << std::hex << base_val << std::endl;
            std::cout << "reg_base is " << reg_base << std::endl;
            std::cout << "reg_dst is " << reg_dst << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movl_binind_r0_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        addr32_t base_addr =
            (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;
        addr32_t r0_val =
            (pick_addr(AddrRange(randgen32, 0, MEM_SZ)) - 4) / 2;
        failure = failure ||
            do_movl_gen_binind_r0_gen(cpu, bios, mem, randgen32->pick_val(0),
                                      r0_val, base_addr, 1, 1);


        for (int reg_base = 0; reg_base < 16; reg_base++) {
            for (int reg_dst = 0; reg_dst < 16; reg_dst++) {
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;

                failure = failure ||
                    do_binary_movl_binind_r0_gen_gen(cpu, bios, mem,
                                                     randgen32->pick_val(0),
                                                     r0_val, base_addr,
                                                     reg_dst, reg_base);
            }
        }

        return failure;
    }

    // MOV.B R0, @(disp, GBR)
    // 11000000dddddddd
    static int do_binary_movb_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.B R0, @(" << (unsigned)disp << ", GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_exec_inst(cpu);

        int8_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp + gbr_val, sizeof(mem_val));
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

    static int binary_movb_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = randgen32->pick_val(0);
            reg32_t gbr_val = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 1 - disp));
            failure = failure ||
                do_binary_movb_r0_binind_disp_gbr(cpu, bios, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.W R0, @(disp, GBR)
    // 11000001dddddddd
    static int do_binary_movw_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.W R0, @(" << unsigned(disp * 2) << ", GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_exec_inst(cpu);

        int16_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp * 2 + gbr_val, sizeof(mem_val));
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

    static int binary_movw_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = pick_addr(AddrRange(randgen32, 0, MEM_SZ));
            reg32_t gbr_val =
                pick_addr(AddrRange(randgen32, 0, MEM_SZ - 2 - disp * 2));
            failure = failure ||
                do_binary_movw_r0_binind_disp_gbr(cpu, bios, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.L R0, @(disp, GBR)
    // 11000010dddddddd
    static int do_binary_movl_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 reg32_t r0_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.L R0, @(" << unsigned(disp * 4) << ", GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_exec_inst(cpu);

        int32_t mem_val;
        sh4_read_mem(cpu, &mem_val, disp * 4 + gbr_val, sizeof(mem_val));
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

    static int binary_movl_r0_binind_disp_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t r0_val = pick_addr(AddrRange(randgen32));
            reg32_t gbr_val =
                pick_addr(AddrRange(randgen32, 0, MEM_SZ - 4 - disp * 4));
            failure = failure ||
                do_binary_movl_r0_binind_disp_gbr(cpu, bios, mem, r0_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.B @(disp, GBR), R0
    // 11000100dddddddd
    static int do_binary_movb_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 int8_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.B @(" << (unsigned)disp << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &src_val, disp + gbr_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != reg32_t(int32_t(src_val))) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                int32_t(src_val) << std::endl;
            std::cout << "actual value was " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movb_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                pick_addr(AddrRange(randgen32, 0, MEM_SZ - 1 - disp));
            failure = failure ||
                do_binary_movb_binind_disp_gbr_r0(cpu, bios, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.W @(disp, GBR), R0
    // 11000101dddddddd
    static int do_binary_movw_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 int16_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.W @(" << unsigned(disp * 2) << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &src_val, disp * 2 + gbr_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != reg32_t(int32_t(src_val))) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                int32_t(src_val) << std::endl;
            std::cout << "actual value was " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movw_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                pick_addr(AddrRange(randgen32, 0, MEM_SZ - 2 - disp * 2));
            failure = failure ||
                do_binary_movw_binind_disp_gbr_r0(cpu, bios, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOV.L @(disp, GBR), R0
    // 11000110dddddddd
    static int do_binary_movl_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                 uint32_t src_val, uint8_t disp,
                                                 reg32_t gbr_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOV.L @(" << unsigned(disp * 4) << ", GBR), R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &src_val, disp * 4 + gbr_val, sizeof(src_val));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != src_val) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                src_val << std::endl;
            std::cout << "actual value was " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            std::cout << "GBR value was " << std::hex << gbr_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_movl_binind_disp_gbr_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                              RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            int8_t src_val = randgen32->pick_val(0);
            reg32_t gbr_val =
                pick_addr(AddrRange(randgen32, 0, MEM_SZ - 4 - disp * 4));
            failure = failure ||
                do_binary_movl_binind_disp_gbr_r0(cpu, bios, mem, src_val, disp,
                                                  gbr_val);
        }

        return failure;
    }

    // MOVA @(disp, PC), R0
    // 11000111dddddddd
    static int do_binary_mova_binind_disp_pc_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                uint8_t disp, reg32_t pc_val) {
        std::stringstream ss;
        std::string cmd;
        Sh4Prog test_prog;

        ss << "MOVA @(" << unsigned(disp * 4) << ", PC), R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_val - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_PC] = pc_val;
        sh4_exec_inst(cpu);

        reg32_t expected_val = disp * 4 + (pc_val & ~3) + 4;

        if (*sh4_gen_reg(cpu, 0) != expected_val) {
            std::cout << "ERROR while running \"" << cmd << "\"" << std::endl;
            std::cout << "expected value was " << std::hex <<
                expected_val << std::endl;
            std::cout << "actual value was " << std::hex << *sh4_gen_reg(cpu, 0) <<
                std::endl;
            std::cout << "PC value was " << std::hex << pc_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_mova_binind_disp_pc_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        for (int disp = 0; disp <= 0xff; disp++) {
            reg32_t pc_val = pick_addr(AddrRange(randgen32, 0, (MEM_SZ - 4 - disp * 4) & ~1));
            failure = failure ||
                do_binary_mova_binind_disp_pc_r0(cpu, bios, mem, disp, pc_val);
        }

        return failure;
    }

    // LDC Rm, SR
    // 0100mmmm00001110
    static int do_binary_ldc_gen_sr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SR] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_sr(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_sr(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, GBR
    // 0100mmmm00011110
    static int do_binary_ldc_gen_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", GBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_GBR] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_GBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_gbr(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, VBR
    // 0100mmmm00101110
    static int do_binary_ldc_gen_vbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", VBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_VBR] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_VBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_vbr(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_vbr(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, SSR
    // 0100mmmm00111110
    static int do_binary_ldc_gen_ssr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SSR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SSR] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_SSR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_ssr(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_ssr(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, SPC
    // 0100mmmm01001110
    static int do_binary_ldc_gen_spc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", SPC\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SPC] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_SPC] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_spc(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_spc(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, DBR
    // 0100mmmm11111010
    static int do_binary_ldc_gen_dbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_no, reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", DBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_DBR] != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex <<
                cpu->reg[SH4_REG_DBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_dbr(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_ldc_gen_dbr(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // LDC Rm, Rn_BANK
    // 0100mmmm1nnn1110
    static int do_binary_ldc_gen_bank(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, unsigned bank_reg_no,
                                      reg32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC R" << reg_no << ", R" << bank_reg_no << "_BANK\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        reg32_t bank_reg_val = *sh4_bank_reg(cpu, bank_reg_no);

        if (bank_reg_val != reg_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_val is " << std::hex << reg_val << std::endl;
            std::cout << "actual val is " << std::hex << bank_reg_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldc_gen_bank(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                failure = failure ||
                    do_binary_ldc_gen_bank(cpu, bios, mem, reg_no, bank_reg_no,
                                           randgen32->pick_val(0));
            }
        }

        return failure;
    }

    // LDC.L @Rm+, SR
    // 0100mmmm00000111
    static int do_binary_ldcl_indgeninc_sr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                           unsigned reg_src, addr32_t addr,
                                           uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        /*
         * Need to restore the original SR because editing SR can cause us to
         * do things that interfere with the test (such as bank-switching).
         */
        reg32_t old_sr = cpu->reg[SH4_REG_SR];
        sh4_exec_inst(cpu);
        reg32_t new_sr = cpu->reg[SH4_REG_SR];
        cpu->reg[SH4_REG_SR] = old_sr;

        if ((new_sr != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << new_sr << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_sr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_sr(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, GBR
    // 0100mmmm00010111
    static int do_binary_ldcl_indgeninc_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, GBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if ((cpu->reg[SH4_REG_GBR] != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg[SH4_REG_GBR] << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_gbr(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, VBR
    // 0100mmmm00100111
    static int do_binary_ldcl_indgeninc_vbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, VBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if ((cpu->reg[SH4_REG_VBR] != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg[SH4_REG_VBR] << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_vbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_vbr(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, SSR
    // 0100mmmm00110111
    static int do_binary_ldcl_indgeninc_ssr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SSR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if ((cpu->reg[SH4_REG_SSR] != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg[SH4_REG_SSR] << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_ssr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_ssr(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, SPC
    // 0100mmmm01000111
    static int do_binary_ldcl_indgeninc_spc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, SPC\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if ((cpu->reg[SH4_REG_SPC] != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg[SH4_REG_SPC] << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_spc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_spc(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // LDC.L @Rm+, DBR
    // 0100mmmm11110110
    static int do_binary_ldcl_indgeninc_dbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_src, addr32_t addr,
                                            uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_src << "+, DBR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if ((cpu->reg[SH4_REG_DBR] != val) || (*sh4_gen_reg(cpu, reg_src) != 4 + addr)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "address is " << std::hex << addr << std::endl;
            std::cout << "expected value is " << val << std::endl;
            std::cout << "actual value is " << cpu->reg[SH4_REG_DBR] << std::endl;
            std::cout << "expected output address is " << (4 + addr) <<
                std::endl;
            std::cout << "actual output address is " <<
                *sh4_gen_reg(cpu, reg_src) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_dbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            addr32_t val = randgen32->pick_val(0);

            failure = failure ||
                do_binary_ldcl_indgeninc_dbr(cpu, bios, mem, reg_src, addr, val);
        }

        return failure;
    }

    // STC SR, Rn
    // 0000nnnn00000010
    static int do_binary_stc_sr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    unsigned reg_dst, reg32_t sr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        /*
         * using random values for SR is a little messy 'cause it has side
         * effects.  In the future I may decide not to use random values for
         * this test.
         */
        sr_val |= SH4_SR_MD_MASK;

        ss << "STC SR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SR] = sr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != sr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_sr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_sr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC GBR, Rn
    // 0000nnnn00010010
    static int do_binary_stc_gbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t gbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC GBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != gbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << gbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_GBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_gbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_gbr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC VBR, Rn
    // 0000nnnn00100010
    static int do_binary_stc_vbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t vbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC VBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_VBR] = vbr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != vbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << vbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_VBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_vbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_vbr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SSR, Rn
    // 0000nnnn00110010
    static int do_binary_stc_ssr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t ssr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SSR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SSR] = ssr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != ssr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << ssr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_SSR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_ssr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_ssr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SPC, Rn
    // 0000nnnn01000010
    static int do_binary_stc_spc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t spc_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SPC, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SPC] = spc_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != spc_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << spc_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_SPC] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_spc_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_spc_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC SGR, Rn
    // 0000nnnn00111010
    static int do_binary_stc_sgr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t sgr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC SGR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SGR] = sgr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != sgr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sgr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_SGR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_sgr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_sgr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC DBR, Rn
    // 0000nnnn11111010
    static int do_binary_stc_dbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_dst,
                                     reg32_t dbr_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC DBR, R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_DBR] = dbr_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != dbr_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << dbr_val <<
                std::endl;
            std::cout << "Actual value is " << cpu->reg[SH4_REG_DBR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_dbr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_stc_dbr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // STC.L SR, @-Rn
    // 0100nnnn00000011
    static int do_binary_stcl_sr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                           unsigned reg_no, reg32_t sr_val,
                                           addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // obviously this needs to run in privileged mode
        sr_val |= SH4_SR_MD_MASK;

        ss << "STC.L SR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SR] = sr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (sr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_sr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_sr_inddecgen(cpu, bios, mem, reg_no,
                                            randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L GBR, @-Rn
    // 0100nnnn00010011
    static int do_binary_stcl_gbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t gbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L GBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_GBR] = gbr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (gbr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << gbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_gbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_gbr_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L VBR, @-Rn
    // 01n00nnnn00100011
    static int do_binary_stcl_vbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t vbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L VBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_VBR] = vbr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (vbr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << vbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_vbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_vbr_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SSR, @-Rn
    // 0100nnnn00110011
    static int do_binary_stcl_ssr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t ssr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SSR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SSR] = ssr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (ssr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << ssr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_ssr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_ssr_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SPC, @-Rn
    // 0100nnnn01000011
    static int do_binary_stcl_spc_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t spc_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SPC, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SPC] = spc_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (spc_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << spc_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_spc_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_spc_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L SGR, @-Rn
    // 0100nnnn00110010
    static int do_binary_stcl_sgr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t sgr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L SGR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SGR] = sgr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (sgr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << sgr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_sgr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_sgr_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // STC.L DBR, @-Rn
    // 0100nnnn11110010
    static int do_binary_stcl_dbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                            unsigned reg_no, reg32_t dbr_val,
                                            addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L DBR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_DBR] = dbr_val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (dbr_val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << dbr_val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_dbr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
            failure = failure ||
                do_binary_stcl_dbr_inddecgen(cpu, bios, mem, reg_no,
                                             randgen32->pick_val(0), addr);
        }

        return failure;
    }

    // LDC.L @Rm+, Rn_BANK
    // 0100mmmm1nnn0111
    static int do_binary_ldcl_indgeninc_bank(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no,
                                             unsigned bank_reg_no,
                                             addr32_t addr, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDC.L @R" << reg_no << "+, R" << bank_reg_no << "_BANK\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        reg32_t bank_reg_val = *sh4_bank_reg(cpu, bank_reg_no);

        if (bank_reg_val != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input address is " << addr << std::endl;
            std::cout << "val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << std::hex << bank_reg_val <<
                std::endl;
            std::cout << "expected output address is " << addr + 4 << std::endl;
            std::cout << "actual output address is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldcl_indgeninc_bank(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                failure = failure ||
                    do_binary_ldcl_indgeninc_bank(cpu, bios, mem, reg_no, bank_reg_no,
                                                  addr, randgen32->pick_val(0));
            }
        }

        return failure;
    }

    // STC Rm_BANK, Rn
    // 0000nnnn1mmm0010
    static int do_binary_stc_bank_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned bank_reg_no, unsigned reg_no,
                                      reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC R" << bank_reg_no << "_BANK, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_bank_reg(cpu, bank_reg_no) = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << val <<
                std::endl;
            std::cout << "Actual value is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stc_bank_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                failure = failure ||
                    do_binary_stc_bank_gen(cpu, bios, mem, bank_reg_no, reg_no,
                                           randgen32->pick_val(0));
            }
        }
        return failure;
    }

    // STC.L Rm_BANK, @-Rn
    // 0100nnnn1mmm0011
    static int do_binary_stcl_bank_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned bank_reg_no,
                                             unsigned reg_no, reg32_t val,
                                             addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STC.L R" << bank_reg_no << "_BANK, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_bank_reg(cpu, bank_reg_no) = val;
        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (val != mem_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "Expected value was " << std::hex << val <<
                std::endl;
            std::cout << "Actual value is " << mem_val << std::endl;
            std::cout << "expected output addr is " << (addr - 4) << std::endl;
            std::cout << "actual output addr is " << *sh4_gen_reg(cpu, reg_no) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stcl_bank_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            for (unsigned bank_reg_no = 0; bank_reg_no < 8; bank_reg_no++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
                failure = failure ||
                    do_binary_stcl_bank_inddecgen(cpu, bios, mem, bank_reg_no, reg_no,
                                                  randgen32->pick_val(0), addr);
            }
        }

        return failure;
    }

    // LDS Rm, MACH
    // 0100mmmm00001010
    static int do_binary_lds_gen_mach(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS R" << reg_no << ", MACH\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_MACH] != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACH] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_lds_gen_mach(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_lds_gen_mach(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // LDS Rm, MACL
    // 0100mmmm00011010
    static int do_binary_lds_gen_macl(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS R" << reg_no << ", MACL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_MACL] != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_lds_gen_macl(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_lds_gen_macl(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // LDS Rm, PR
    // 0100mmmm00101010
    static int do_binary_lds_gen_pr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS R" << reg_no << ", PR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_PR] != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_PR] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_lds_gen_pr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_lds_gen_pr(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // STS MACH, Rn
    // 0000nnnn00001010
    static int do_binary_sts_mach_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS MACH, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_MACH] = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_sts_mach_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_sts_mach_gen(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // STS MACL, Rn
    // 0000nnnn00011010
    static int do_binary_sts_macl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS MACL, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_MACL] = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_sts_macl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_sts_macl_gen(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // STS PR, Rn
    // 0000nnnn00101010
    static int do_binary_sts_pr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS PR, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_PR] = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_sts_pr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_sts_pr_gen(cpu, bios, mem, reg_no,
                                     randgen32->pick_val(0));
        }

        return failure;
    }

    // LDS.L @Rm+, MACH
    // 0100mmmm00000110
    static int do_binary_ldsl_indgeninc_mach(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no, addr32_t addr,
                                             uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS.L @R" << reg_no << "+, MACH\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_MACH] != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr + 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldsl_indgeninc_mach(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            uint32_t val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_ldsl_indgeninc_mach(cpu, bios, mem, reg_no, addr, val);
        }

        return failure;
    }

    // LDS.L @Rm+, MACL
    // 0100mmmm00010110
    static int do_binary_ldsl_indgeninc_macl(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no, addr32_t addr,
                                             uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS.L @R" << reg_no << "+, MACL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_MACL] != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACL] << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr + 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldsl_indgeninc_macl(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            uint32_t val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_ldsl_indgeninc_macl(cpu, bios, mem, reg_no, addr, val);
        }

        return failure;
    }

    // LDS.L @Rm+, PR
    // 0100mmmm00100110
    static int do_binary_ldsl_indgeninc_pr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                           unsigned reg_no, addr32_t addr,
                                           uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS.L @R" << reg_no << "+, PR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_PR] != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_PR] << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr + 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldsl_indgeninc_pr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            uint32_t val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_ldsl_indgeninc_pr(cpu, bios, mem, reg_no, addr, val);
        }

        return failure;
    }

    // STS.L MACH, @-Rn
    // 0100mmmm00000010
    static int do_binary_stsl_mach_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no, reg32_t mach_val,
                                             addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS.L MACH, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        cpu->reg[SH4_REG_MACH] = mach_val;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (mem_val != mach_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << mach_val << std::endl;
            std::cout << "actual val is " << mem_val << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr - 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stsl_mach_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 1));
            reg32_t mach_val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_stsl_mach_inddecgen(cpu, bios, mem, reg_no,
                                              mach_val, addr);
        }

        return failure;
    }

    // STS.L MACL, @-Rn
    // 0100mmmm00010010
    static int do_binary_stsl_macl_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no, reg32_t macl_val,
                                             addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS.L MACL, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        cpu->reg[SH4_REG_MACL] = macl_val;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (mem_val != macl_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << macl_val << std::endl;
            std::cout << "actual val is " << mem_val << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr - 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stsl_macl_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 1));
            reg32_t macl_val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_stsl_macl_inddecgen(cpu, bios, mem, reg_no,
                                              macl_val, addr);
        }

        return failure;
    }

    // STS.L PR, @-Rn
    // 0100nnnn00100010
    static int do_binary_stsl_pr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                           unsigned reg_no, reg32_t pr_val,
                                           addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS.L PR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        cpu->reg[SH4_REG_PR] = pr_val;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (mem_val != pr_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << pr_val << std::endl;
            std::cout << "actual val is " << mem_val << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr - 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stsl_pr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 1));
            reg32_t pr_val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_stsl_pr_inddecgen(cpu, bios, mem, reg_no,
                                              pr_val, addr);
        }

        return failure;
    }

    // CMP/PZ Rn
    // 0100nnnn00010001
    static int do_unary_cmppz_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                                  int32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/PZ R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        bool t_expect = (reg_val >= 0);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "input val is " << reg_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_cmppz_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_cmppz_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // CMP/PL Rn
    // 0100nnnn00010101
    static int do_unary_cmppl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                                  int32_t reg_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/PL R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = reg_val;
        sh4_exec_inst(cpu);

        bool t_expect = (reg_val > 0);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "input val is " << reg_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_cmppl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_cmppl_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // CMP/EQ #imm, R0
    // 10001000iiiiiiii
    static int do_binary_cmpeq_imm_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       uint8_t imm_val, reg32_t r0_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/EQ #" << unsigned(imm_val) << std::dec << ", R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        bool t_expect = (r0_val == reg32_t(int32_t(int8_t(imm_val))));
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "r0_val is " << r0_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmpeq_imm_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned val = 0; val <= 255; val++) {
            uint8_t imm_val = val;
            // test equality
            failure = failure ||
                do_binary_cmpeq_imm_gen(cpu, bios, mem, imm_val, imm_val);

            // test inequality
            failure = failure ||
                do_binary_cmpeq_imm_gen(cpu, bios, mem, imm_val,
                                        int32_t(int8_t(imm_val)));
        }

        return failure;
    }

    // CMP/EQ Rm, Rn
    // 0011nnnnmmmm0000
    static int do_binary_cmpeq_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg1, unsigned reg2,
                                       reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/EQ R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = (reg2_val == reg1_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmpeq_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            reg32_t val1 = randgen32->pick_val(0);
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2;
                if (reg1 == reg2)
                    val2 = val1;
                else
                    val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmpeq_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                // test (probable) inequality
                failure = failure ||
                    do_binary_cmpeq_gen_gen(cpu, bios, mem, reg1, reg2, val1, val2);
            }
        }

        return failure;
    }

    // CMP/HS Rm, Rn
    // 0011nnnnmmmm0010
    static int do_binary_cmphs_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg1, unsigned reg2,
                                       reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/HS R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = (reg2_val >= reg1_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmphs_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmphs_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                if (reg1 != reg2) {
                    // test val1 < val2
                    failure = failure ||
                        do_binary_cmphs_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 - 1, val2);

                    // test val1 > val2
                    failure = failure ||
                        do_binary_cmphs_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 + 1, val2);
                }
            }
        }

        return failure;
    }

    // CMP/GE Rm, Rn
    // 0011nnnnmmmm0011
    static int do_binary_cmpge_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg1, unsigned reg2,
                                       reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/GE R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = (int32_t(reg2_val) >= int32_t(reg1_val));
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmpge_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmpge_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                if (reg1 != reg2) {
                    // test val1 < val2
                    failure = failure ||
                        do_binary_cmpge_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 - 1, val2);

                    // test val1 > val2
                    failure = failure ||
                        do_binary_cmpge_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 + 1, val2);
                }
            }
        }

        return failure;
    }

    // CMP/HI Rm, Rn
    // 0011nnnnmmmm0110
    static int do_binary_cmphi_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg1, unsigned reg2,
                                       reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/HI R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = (reg2_val > reg1_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmphi_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmphi_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                if (reg1 != reg2) {
                    // test val1 < val2
                    failure = failure ||
                        do_binary_cmphi_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 - 1, val2);

                    // test val1 > val2
                    failure = failure ||
                        do_binary_cmphi_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 + 1, val2);
                }
            }
        }

        return failure;
    }

    // CMP/GT Rm, Rn
    // 0011nnnnmmmm0111
    static int do_binary_cmpgt_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg1, unsigned reg2,
                                       reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/GT R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = (int32_t(reg2_val) > int32_t(reg1_val));
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmpgt_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmpgt_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                if (reg1 != reg2) {
                    // test val1 < val2
                    failure = failure ||
                        do_binary_cmpgt_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 - 1, val2);

                    // test val1 > val2
                    failure = failure ||
                        do_binary_cmpgt_gen_gen(cpu, bios, mem, reg1, reg2,
                                                val2 + 1, val2);
                }
            }
        }

        return failure;
    }

    static int do_binary_cmpstr_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        unsigned reg1, unsigned reg2,
                                        reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CMP/STR R" << reg1 << ", R" << reg2 << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1) = reg1_val;
        *sh4_gen_reg(cpu, reg2) = reg2_val;
        sh4_exec_inst(cpu);

        bool t_expect = false;
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        for (int i = 0; i < 4; i++)
            if ((reg1_val & (0xff << (i * 8))) ==
                (reg2_val & (0xff << (i * 8))))
                t_expect = true;

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_cmpstr_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1 = 0; reg1 < 16; reg1++) {
            for (unsigned reg2 = 0; reg2 < 16; reg2++) {
                reg32_t val2 = randgen32->pick_val(0);

                // test equality
                failure = failure ||
                    do_binary_cmpstr_gen_gen(cpu, bios, mem, reg1, reg2, val2, val2);

                // test partial equality
                reg32_t val_tmp =
                    val2 ^ ~(0xff << (8 * randgen32->pick_range(0, 3)));
                failure = failure ||
                    do_binary_cmpstr_gen_gen(cpu, bios, mem, reg1, reg2, val2, val_tmp);

                // test (probable) inequality
                if (reg1 != reg2) {
                    failure = failure ||
                        do_binary_cmpstr_gen_gen(cpu, bios, mem, reg1, reg2,
                                                 val2, randgen32->pick_val(0));
                }
            }
        }

        return failure;
    }

    // TST Rm, Rn
    // 0010nnnnmmmm1000
    static int do_binary_tst_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg1_no, unsigned reg2_no,
                                     reg32_t reg1_val, reg32_t reg2_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "TST R" << reg1_no << ", R" << reg2_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg1_no) = reg1_val;
        *sh4_gen_reg(cpu, reg2_no) = reg2_val;
        sh4_exec_inst(cpu);
        bool t_expect = !(reg1_val & reg2_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "reg1_val is " << std::hex << reg1_val << std::endl;
            std::cout << "reg2_val is " << std::hex << reg2_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_tst_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg1_no = 0; reg1_no < 16; reg1_no++) {
            for (unsigned reg2_no = 0; reg2_no < 16; reg2_no++) {
                reg32_t reg1_val = randgen32->pick_val(0);
                reg32_t reg2_val = reg1_val;

                if (reg1_no != reg2_no)
                    reg2_val = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_tst_gen_gen(cpu, bios, mem, reg1_no, reg2_no,
                                          reg1_val, reg2_val);
            }
        }

        return failure;
    }

    // TAS.B @Rn
    // 0100nnnn00011011
    static int do_unary_tasb_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    unsigned reg_no, addr32_t addr,
                                    uint8_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "TAS.B @R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        bool t_expect = !val;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK;

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual t val is " <<
                bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) << std::endl;
            std::cout << "val is " << std::hex << unsigned(val) << std::endl;
            std::cout << "addr is " << addr << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_tasb_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32));
            uint8_t val = randgen32->pick_val(0);

            // make sure 0 gets tested
            failure = failure ||
                do_unary_tasb_indgen(cpu, bios, mem, reg_no, addr, 0);

            failure = failure ||
                do_unary_tasb_indgen(cpu, bios, mem, reg_no, addr, val);
        }
        return failure;
    }

    // TST #imm, R0
    // 11001000iiiiiiii
    static int do_binary_tst_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    uint8_t imm_val, reg32_t r0_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "TST #" << unsigned(imm_val) << ", R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        bool t_expect = !(r0_val & imm_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "imm_val is " << std::hex << unsigned(imm_val) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_tst_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            failure = failure ||
                do_binary_tst_imm_r0(cpu, bios, mem, imm_val,
                                     randgen32->pick_val(0));
        }

        return failure;
    }

    // TST.B #imm, @(R0, GBR)
    // 11001100iiiiiiii
    static int do_binary_tstb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                uint8_t imm_val, reg32_t r0_val,
                                                reg32_t gbr_val,
                                                uint8_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        addr32_t addr = gbr_val + r0_val;

        ss << "TST.B #" << unsigned(imm_val) << ", @(R0, GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &mem_val, addr, sizeof(mem_val));

        sh4_exec_inst(cpu);

        bool t_expect = !(mem_val & imm_val);
        bool t_actual = bool(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK);

        if ((t_expect && !t_actual) || (!t_expect && t_actual)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual val is " << t_actual << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "imm_val is " << std::hex << unsigned(imm_val) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_tstb_imm_ind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            reg32_t gbr_val = pick_addr(AddrRange(randgen32)) / 2;
            reg32_t r0_val = pick_addr(AddrRange(randgen32)) / 2;

            failure = failure ||
                do_binary_tstb_imm_binind_r0_gbr(cpu, bios, mem, imm_val, r0_val,
                                                 gbr_val,
                                                 randgen32->pick_val(0));
        }

        return failure;
    }

    // AND Rm, Rn
    // 0010nnnnmmmm1001
    static int do_binary_and_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_src,
                                     unsigned reg_dst, uint32_t src_val,
                                     uint32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "AND R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != (src_val & dst_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "dst_val is " << std::hex << dst_val << std::endl;
            std::cout << "expected val is " << (src_val & dst_val) << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_and_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);
                uint32_t dst_val = src_val;
                if (reg_src != reg_dst) {
                    dst_val = randgen32->pick_val(0);
                }

                failure = failure ||
                    do_binary_and_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                          src_val, dst_val);
            }
        }

        return failure;
    }

    // AND #imm, R0
    // 11001001iiiiiiii
    static int do_binary_and_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    uint8_t imm_val, uint32_t r0_val) {

        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "AND #" << unsigned(imm_val) << ", R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != (r0_val & imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "output val is " << (r0_val & imm_val) << std::endl;
            std::cout << "expected val is " << (r0_val & imm_val) << std::endl;
            std::cout << "r0_val is " << r0_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_and_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            failure =  failure ||
                do_binary_and_imm_r0(cpu, bios, mem, imm_val,
                                     randgen32->pick_val(0));
        }

        return failure;
    }

    // AND.B #imm, @(R0, GBR)
    // 11001101iiiiiiii
    static int do_binary_andb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                uint8_t imm_val, reg32_t r0_val,
                                                reg32_t gbr_val,
                                                uint8_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        addr32_t addr = gbr_val + r0_val;

        ss << "AND.B #" << unsigned(imm_val) << ", @(R0, GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &mem_val, addr, sizeof(mem_val));

        sh4_exec_inst(cpu);

        uint8_t result;
        sh4_read_mem(cpu, &result, addr, sizeof(result));

        if (result != (mem_val & imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << (mem_val & gbr_val) << std::endl;
            std::cout << "actual val is " << unsigned(result) << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "imm_val is " << std::hex << unsigned(imm_val) <<
                std::endl;
            std::cout << "mem_val is " << unsigned(mem_val) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_andb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            reg32_t gbr_val = pick_addr(AddrRange(randgen32)) / 2;
            reg32_t r0_val = pick_addr(AddrRange(randgen32)) / 2;

            failure = failure ||
                do_binary_andb_imm_binind_r0_gbr(cpu, bios, mem, imm_val, r0_val,
                                                 gbr_val,
                                                 randgen32->pick_val(0));
        }

        return failure;
    }

    // OR Rm, Rn
    // 0010nnnnmmmm1011
    static int do_binary_or_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_src,
                                    unsigned reg_dst, uint32_t src_val,
                                    uint32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "OR R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != (src_val | dst_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "dst_val is " << std::hex << dst_val << std::endl;
            std::cout << "expected val is " << (src_val | dst_val) << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_or_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);
                uint32_t dst_val = src_val;
                if (reg_src != reg_dst) {
                    dst_val = randgen32->pick_val(0);
                }

                failure = failure ||
                    do_binary_or_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                          src_val, dst_val);
            }
        }

        return failure;
    }

    // OR #imm, R0
    // 11001011iiiiiiii
    static int do_binary_or_imm_r0(Sh4 *cpu, BiosFile *bios, Memory  *mem,
                                    uint8_t imm_val, uint32_t r0_val) {

        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "OR #" << unsigned(imm_val) << ", R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != (r0_val | imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "output val is " << (r0_val & imm_val) << std::endl;
            std::cout << "expected val is " << (r0_val | imm_val) << std::endl;
            std::cout << "r0_val is " << r0_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_or_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            failure =  failure ||
                do_binary_or_imm_r0(cpu, bios, mem, imm_val, randgen32->pick_val(0));
        }

        return failure;
    }

    // OR.B #imm, @(R0, GBR)
    // 11001111iiiiiiii
    static int do_binary_orb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios,
                                               Memory *mem, uint8_t imm_val,
                                               reg32_t r0_val, reg32_t gbr_val,
                                               uint8_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        addr32_t addr = gbr_val + r0_val;

        ss << "OR.B #" << unsigned(imm_val) << ", @(R0, GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &mem_val, addr, sizeof(mem_val));

        sh4_exec_inst(cpu);

        uint8_t result;
        sh4_read_mem(cpu, &result, addr, sizeof(result));

        if (result != (mem_val | imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << (mem_val | gbr_val) << std::endl;
            std::cout << "actual val is " << unsigned(result) << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "imm_val is " << std::hex << unsigned(imm_val) <<
                std::endl;
            std::cout << "mem_val is " << unsigned(mem_val) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_orb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios,
                                            Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            reg32_t gbr_val = pick_addr(AddrRange(randgen32)) / 2;
            reg32_t r0_val = pick_addr(AddrRange(randgen32)) / 2;

            failure = failure ||
                do_binary_orb_imm_binind_r0_gbr(cpu, bios, mem, imm_val, r0_val,
                                                gbr_val,
                                                randgen32->pick_val(0));
        }

        return failure;
    }

    // XOR Rm, Rn
    // 0010nnnnmmmm1010
    static int do_binary_xor_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_src, unsigned reg_dst,
                                     uint32_t src_val, uint32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "XOR R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != (src_val ^ dst_val)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "dst_val is " << std::hex << dst_val << std::endl;
            std::cout << "expected val is " << (src_val ^ dst_val) << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_xor_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);
                uint32_t dst_val = src_val;
                if (reg_src != reg_dst) {
                    dst_val = randgen32->pick_val(0);
                }

                failure = failure ||
                    do_binary_xor_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                          src_val, dst_val);
            }
        }

        return failure;
    }

    // XOR #imm, R0
    // 11001010iiiiiiii
    static int do_binary_xor_imm_r0(Sh4 *cpu, BiosFile *bios, Memory  *mem,
                                    uint8_t imm_val, uint32_t r0_val) {

        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "XOR #" << unsigned(imm_val) << ", R0\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, 0) != (r0_val ^ imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "output val is " << (r0_val & imm_val) << std::endl;
            std::cout << "expected val is " << (r0_val ^ imm_val) << std::endl;
            std::cout << "r0_val is " << r0_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_xor_imm_r0(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            failure =  failure ||
                do_binary_xor_imm_r0(cpu, bios, mem, imm_val, randgen32->pick_val(0));
        }

        return failure;
    }

    // XOR.B #imm, @(R0, GBR)
    // 11001110iiiiiiii
    static int do_binary_xorb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                uint8_t imm_val, reg32_t r0_val,
                                                reg32_t gbr_val,
                                                uint8_t mem_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        addr32_t addr = gbr_val + r0_val;

        ss << "XOR.B #" << unsigned(imm_val) << ", @(R0, GBR)\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, 0) = r0_val;
        cpu->reg[SH4_REG_GBR] = gbr_val;
        sh4_write_mem(cpu, &mem_val, addr, sizeof(mem_val));

        sh4_exec_inst(cpu);

        uint8_t result;
        sh4_read_mem(cpu, &result, addr, sizeof(result));

        if (result != (mem_val ^ imm_val)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << (mem_val ^ gbr_val) << std::endl;
            std::cout << "actual val is " << unsigned(result) << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "imm_val is " << std::hex << unsigned(imm_val) <<
                std::endl;
            std::cout << "mem_val is " << unsigned(mem_val) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_xorb_imm_binind_r0_gbr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned imm_val = 0; imm_val < 256; imm_val++) {
            reg32_t gbr_val = pick_addr(AddrRange(randgen32)) / 2;
            reg32_t r0_val = pick_addr(AddrRange(randgen32)) / 2;

            failure = failure ||
                do_binary_xorb_imm_binind_r0_gbr(cpu, bios, mem, imm_val, r0_val,
                                                 gbr_val,
                                                 randgen32->pick_val(0));
        }

        return failure;
    }

    // NOT Rm, Rn
    // 0110nnnnmmmm0111
    static int do_binary_not_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_src,
                                    unsigned reg_dst, uint32_t src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "NOT R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != ~src_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "src_val is " << std::hex << src_val << std::endl;
            std::cout << "expected val is " << (~src_val) << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_not_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_not_gen_gen(cpu, bios, mem, reg_src, reg_dst, src_val);
            }
        }

        return failure;
    }

    // NEG Rm, Rn
    // 0110nnnnmmmm1011
    static int do_binary_neg_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     unsigned reg_src, unsigned reg_dst,
                                     uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "NEG R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != -val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val is " << std::hex << val << std::endl;
            std::cout << "expected output val is " << (-val) << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_neg_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_neg_gen_gen(cpu, bios, mem, reg_src, reg_dst, val);
            }
        }
        return failure;
    }

    // NEGC Rm, Rn
    // 0110nnnnmmmm1010
    static int do_binary_negc_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_src, unsigned reg_dst,
                                      uint32_t val, bool t_flag_in) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "NEGC R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        if (t_flag_in)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
        sh4_exec_inst(cpu);

        bool t_expect = val > 0;
        bool t_actual = bool(!!(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK));

        reg32_t val_expect;
        val_expect = 0 - val;
        if (t_flag_in)
            val_expect--;

        if ((*sh4_gen_reg(cpu, reg_dst) != val_expect) ||
            (t_expect != t_actual)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val is " << std::hex << val << std::endl;
            std::cout << "input T flag is " << (int)t_flag_in << std::endl;
            std::cout << "expected output val is " << val_expect << std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_dst) << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual t val is " << t_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_negc_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_negc_gen_gen(cpu, bios, mem, reg_src,
                                           reg_dst, val, false);
                failure = failure ||
                    do_binary_negc_gen_gen(cpu, bios, mem, reg_src,
                                           reg_dst, val, true);
            }
        }
        return failure;
    }

    // DT Rn
    // 0100nnnn00010000
    static int do_unary_dt_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32,
                               unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "DT R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t output_expect = val - 1;
        bool t_expect = !val;
        bool t_actual = bool(!!(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK));

        if ((*sh4_gen_reg(cpu, reg_no) != output_expect) ||
            (t_expect != t_actual)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val is " << std::hex << val << std::endl;
            std::cout << "expected output val is " << output_expect <<
                std::endl;
            std::cout << "actual val is " << std::hex <<
                *sh4_gen_reg(cpu, reg_no) << std::endl;
            std::cout << "expected t val is " << t_expect << std::endl;
            std::cout << "actual t val is " << t_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_dt_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_dt_gen(cpu, bios, mem, randgen32,
                                reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SWAP.B Rm, Rn
    // 0110nnnnmmmm1000
    static int do_binary_swapb_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        unsigned val_low = val & 0xff;
        unsigned val_hi = (val & 0xff00) >> 8;
        unsigned val_expect = (val_low << 8) | val_hi | (val & ~0xffff);

        ss << "SWAP.B R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " << std::hex << val << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_swapb_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0;reg_dst < 16; reg_dst++) {
                failure = failure ||
                    do_binary_swapb_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                            randgen32->pick_val(0));
            }
        return failure;
    }

    // SWAP.W Rm, Rn
    // 0110nnnnmmmm1001
    static int do_binary_swapw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        unsigned val_low = val & 0x0000ffff;
        unsigned val_hi = (val & 0xffff0000) >> 16;
        unsigned val_expect = (val_low << 16) | val_hi;

        ss << "SWAP.W R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " << std::hex << val << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_swapw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0;reg_dst < 16; reg_dst++) {
                failure = failure ||
                    do_binary_swapw_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                            randgen32->pick_val(0));
            }
        return failure;
    }

    // XTRCT Rm, Rn
    // 0110nnnnmmmm1101
    static int do_binary_xtrct_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val_src, unsigned val_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        unsigned val_src_low = val_src & 0x0000ffff;
        unsigned val_dst_hi = (val_dst & 0xffff0000) >> 16;
        unsigned val_expect = (val_src_low << 16) | val_dst_hi;

        ss << "XTRCT R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        *sh4_gen_reg(cpu, reg_dst) = val_dst;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input values were " << std::hex << val_src <<
                ", " << val_dst << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_xtrct_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                unsigned val_src = randgen32->pick_val(0);
                unsigned val_dst = val_src;

                if (reg_src != reg_dst)
                    randgen32->pick_val(0);
                failure = failure ||
                    do_binary_xtrct_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                            val_src, val_dst);
            }
        return failure;
    }

    // EXTS.B Rm, Rn
    // 0110nnnnmmmm1110
    static int do_binary_extsb_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val_src) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        unsigned val_expect = int32_t(int8_t(val_src & 0xff));

        ss << "EXTS.B R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " <<
                std::hex << val_src << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_extsb_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                unsigned val_src = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_extsb_gen_gen(cpu, bios, mem, reg_src,
                                            reg_dst, val_src);
            }
        return failure;
    }

    // EXTS.W Rm, Rnn
    // 0110nnnnmmmm1111
    static int do_binary_extsw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val_src) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        unsigned val_expect = int32_t(int16_t(val_src & 0xffff));

        ss << "EXTS.W R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " <<
                std::hex << val_src << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_extsw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                unsigned val_src = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_extsw_gen_gen(cpu, bios, mem, reg_src,
                                            reg_dst, val_src);
            }
        return failure;
    }

    // EXTU.B Rm, Rn
    // 0110nnnnmmmm1100
    static int do_binary_extub_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val_src) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        unsigned val_expect = uint32_t(uint8_t(val_src & 0xff));

        ss << "EXTU.B R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " <<
                std::hex << val_src << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_extub_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                unsigned val_src = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_extub_gen_gen(cpu, bios, mem, reg_src,
                                            reg_dst, val_src);
            }
        return failure;
    }

    // EXTU.W Rm, Rn
    // 0110nnnnmmmm1101
    static int do_binary_extuw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       unsigned val_src) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        unsigned val_expect = uint32_t(uint16_t(val_src & 0xffff));

        ss << "EXTU.W R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_dst) != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input value was " <<
                std::hex << val_src << std::endl;
            std::cout << "Expected output was " << val_expect << std::endl;
            std::cout << "actual output was " << *sh4_gen_reg(cpu, reg_dst) <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_extuw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_src = 0; reg_src < 16; reg_src++)
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                unsigned val_src = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_extuw_gen_gen(cpu, bios, mem, reg_src,
                                            reg_dst, val_src);
            }
        return failure;
    }

    // ROTL Rn
    // 0100nnnn00000100
    static int do_unary_rotl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "ROTL R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 1;
        if (val & 0x80000000)
            val_expect |= 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 0x80000000 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_rotl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_rotl_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // ROTR Rn
    // 0100nnnn00000101
    static int do_unary_rotr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "ROTR R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 1;
        if (val & 1)
            val_expect |= 0x80000000;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 1 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_rotr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_rotr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // ROTCL Rn
    // 0100nnnn00100100
    static int do_unary_rotcl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val, bool t_flag) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "ROTCL R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        if (t_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 1;
        if (t_flag)
            val_expect |= 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 0x80000000 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_rotcl_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_rotcl_gen(cpu, bios, mem, reg_no,
                                   randgen32->pick_val(0), false);
            failure = failure ||
                do_unary_rotcl_gen(cpu, bios, mem, reg_no,
                                   randgen32->pick_val(0), true);
        }

        return failure;
    }

    // ROTCR Rn
    // 0100nnnn00100101
    static int do_unary_rotcr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val, bool t_flag) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "ROTCR R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        if (t_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 1;
        if (t_flag)
            val_expect |= 0x80000000;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 1 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_rotcr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_rotcr_gen(cpu, bios, mem, reg_no,
                                   randgen32->pick_val(0), false);
            failure = failure ||
                do_unary_rotcr_gen(cpu, bios, mem, reg_no,
                                   randgen32->pick_val(0), true);
        }

        return failure;
    }

    // SHAD Rm, Rn
    // 0100nnnnmmmm1100
    static int do_binary_shad_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_src, unsigned reg_dst,
                                  uint32_t val_src, uint32_t val_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHAD R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        *sh4_gen_reg(cpu, reg_dst) = val_dst;
        sh4_exec_inst(cpu);

        uint32_t val_expect;
        if (int32_t(val_src) >= 0) {
            val_expect = val_dst << val_src;
        } else {
            val_expect = int32_t(val_dst) >> -int32_t(val_src);
        }

        uint32_t val_actual = *sh4_gen_reg(cpu, reg_dst);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val were " << val_src << ", " <<
                val_dst << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_shad_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);
                uint32_t dst_val = src_val;
                if (reg_src != reg_dst)
                    dst_val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_shad_gen(cpu, bios, mem, reg_src, reg_dst,
                                       src_val, dst_val);
            }
        }

        return failure;
    }

    // SHAL Rn
    // 0100nnnn00100000
    static int do_unary_shal_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHAL R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 0x80000000 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shal_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shal_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHAR Rn
    // 0100nnnn00100001
    static int do_unary_shar_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHAR R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = int32_t(val) >> 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 1 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shar_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shar_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLD Rm, Rn
    // 0100nnnnmmmm1101
    static int do_binary_shld_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_src, unsigned reg_dst,
                                  uint32_t val_src, uint32_t val_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLD R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = val_src;
        *sh4_gen_reg(cpu, reg_dst) = val_dst;
        sh4_exec_inst(cpu);

        uint32_t val_expect;
        if (int32_t(val_src) >= 0) {
            val_expect = val_dst << val_src;
        } else {
            val_expect = val_dst >> -int32_t(val_src);
        }

        uint32_t val_actual = *sh4_gen_reg(cpu, reg_dst);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val were " << val_src << ", " <<
                val_dst << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_shld_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                uint32_t src_val = randgen32->pick_val(0);
                uint32_t dst_val = src_val;
                if (reg_src != reg_dst)
                    dst_val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_shld_gen(cpu, bios, mem, reg_src, reg_dst,
                                       src_val, dst_val);
            }
        }

        return failure;
    }

    // SHLL Rn
    // 0100nnnn00000000
    static int do_unary_shll_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLL R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 0x80000000 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shll_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shll_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLR Rn
    // 0100nnnn00000001
    static int do_unary_shlr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLR R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 1;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        bool t_expect = val & 1 ? true : false;
        bool t_actual = cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK ? true : false;

        if (val_actual != val_expect || t_actual != t_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            std::cout << "expected output T flag was " << t_expect << std::endl;
            std::cout << "actual output T flag was "<< t_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shlr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shlr_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLL2 Rn
    // 0100nnnn00001000
    static int do_unary_shll2_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLL2 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 2;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shll2_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shll2_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLR2 Rn
    // 0100nnnn00001001
    static int do_unary_shlr2_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLR2 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 2;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shlr2_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shlr2_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLL8 Rn
    // 0100nnnn00011000
    static int do_unary_shll8_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLL8 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 8;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shll8_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shll8_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLR8 Rn
    // 0100nnnn00011001
    static int do_unary_shlr8_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                  unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLR8 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 8;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shlr8_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shlr8_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLL16 Rn
    // 0100nnnn00101000
    static int do_unary_shll16_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLL16 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val << 16;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shll16_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shll16_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // SHLR16 Rn
    // 0100nnnn00101001
    static int do_unary_shlr16_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   unsigned reg_no, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SHLR16 R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        uint32_t val_expect = val >> 16;
        uint32_t val_actual = *sh4_gen_reg(cpu, reg_no);

        if (val_actual != val_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "input val was " << val << std::endl;
            std::cout << "expected output val was " << val_expect << std::endl;
            std::cout << "actual output val was " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int unary_shlr16_gen(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_unary_shlr16_gen(cpu, bios, mem, reg_no, randgen32->pick_val(0));
        }

        return failure;
    }

    // MUL.L Rm, Rn
    // 0000nnnnmmmm0111
    static int do_binary_mull_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_src, unsigned reg_dst,
                                      reg32_t src_val, reg32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MUL.L R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        reg32_t val_expect = dst_val * src_val;
        if (cpu->reg[SH4_REG_MACL] != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "inputs are " << std::hex <<
                src_val << ", " << dst_val << std::endl;
            std::cout << "expected output is " << val_expect << std::endl;
            std::cout << "actual output is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_mull_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                reg32_t src_val = randgen32->pick_val(0);
                reg32_t dst_val = src_val;
                if (reg_src != reg_dst)
                    dst_val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_mull_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                           src_val, dst_val);
            }
        }

        return failure;
    }

    // MULS.W Rm, Rn
    // 0010nnnnmmmm1111
    static int do_binary_mulsw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       reg32_t src_val, reg32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MULS.W R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        reg32_t val_expect = int32_t(int16_t(dst_val)) *
            int32_t(int16_t(src_val));

        if (cpu->reg[SH4_REG_MACL] != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "inputs are " << std::hex <<
                src_val << ", " << dst_val << std::endl;
            std::cout << "expected output is " << val_expect << std::endl;
            std::cout << "actual output is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_mulsw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                reg32_t src_val = randgen32->pick_val(0);
                reg32_t dst_val = src_val;
                if (reg_src != reg_dst)
                    dst_val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_mulsw_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                            src_val, dst_val);
            }
        }

        return failure;
    }

    // MULU.W Rm, Rn
    // 0010nnnnmmmm1110
    static int do_binary_muluw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_src, unsigned reg_dst,
                                       reg32_t src_val, reg32_t dst_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MULU.W R" << reg_src << ", R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        sh4_exec_inst(cpu);

        reg32_t val_expect = uint32_t(uint16_t(dst_val)) *
            uint32_t(uint16_t(src_val));

        if (cpu->reg[SH4_REG_MACL] != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "inputs are " << std::hex <<
                src_val << ", " << dst_val << std::endl;
            std::cout << "expected output is " << val_expect << std::endl;
            std::cout << "actual output is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_muluw_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                reg32_t src_val = randgen32->pick_val(0);
                reg32_t dst_val = src_val;
                if (reg_src != reg_dst)
                    dst_val = randgen32->pick_val(0);
                failure = failure ||
                    do_binary_muluw_gen_gen(cpu, bios, mem, reg_src, reg_dst,
                                            src_val, dst_val);
            }
        }

        return failure;
    }

    // MAC.L @Rm+, @Rn+
    // 0000nnnnmmmm1111
    static int do_binary_macl_indgeninc_indgeninc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                  unsigned reg_src,
                                                  unsigned reg_dst,
                                                  addr32_t src_addr,
                                                  addr32_t dst_addr,
                                                  uint32_t src_val,
                                                  uint32_t dst_val,
                                                  uint32_t macl_init,
                                                  uint32_t mach_init,
                                                  bool sat_flag) {
        static const int64_t MAX48 = 0x7fffffffffff;
        static const int64_t MIN48 = 0xffff800000000000;

        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MAC.L @R" << reg_src << "+, @R" << reg_dst << "+\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_addr;
        *sh4_gen_reg(cpu, reg_dst) = dst_addr;
        cpu->reg[SH4_REG_MACL] = macl_init;
        cpu->reg[SH4_REG_MACH] = mach_init;
        sh4_write_mem(cpu, &src_val, src_addr, sizeof(src_val));
        sh4_write_mem(cpu, &dst_val, dst_addr, sizeof(dst_val));
        if (sat_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_S_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_S_MASK;
        sh4_exec_inst(cpu);

        /*
         * TODO: Ideally this would not be using the exact
         * same code as the actual implementation...
         */
        reg32_t macl_expect, mach_expect;
        int64_t prod = int64_t(int32_t(dst_val)) * int64_t(int32_t(src_val));
        int64_t sum;

        if (sat_flag) {
            // 48-bit saturation addition
            int64_t mac = int64_t(uint64_t(macl_init) |
                                  (uint64_t(mach_init) << 32));
            sum = mac + prod;
            if (sum < 0) {
                if (mac >= 0 && prod >= 0) {
                    // overflow positive to negative
                    sum = MAX48;
                } else if (sum < MIN48) {
                    sum = MIN48;
                }
            } else {
                if (mac < 0 && prod < 0) {
                    // overflow negative to positive
                    sum = MIN48;
                } else if (sum > MAX48) {
                    sum = MAX48;
                }
            }
        } else {
            sum = prod +
                int64_t(uint64_t(macl_init) | (uint64_t(mach_init) << 32));
        }

        macl_expect = uint64_t(sum) & 0xffffffff;
        mach_expect = uint64_t(sum) >> 32;

        reg32_t out_src_addr_expect = src_addr + 4;
        reg32_t out_dst_addr_expect = dst_addr + 4;
        if (reg_src == reg_dst)
            out_src_addr_expect = out_dst_addr_expect = src_addr + 8;

        if (cpu->reg[SH4_REG_MACL] != macl_expect || cpu->reg[SH4_REG_MACH] != mach_expect ||
            (*sh4_gen_reg(cpu, reg_src) != out_src_addr_expect) ||
            (*sh4_gen_reg(cpu, reg_dst) != out_dst_addr_expect)) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "the saturation flag is " << sat_flag << std::endl;
            std::cout << "inputs are " << std::hex <<
                src_val << ", " << dst_val << std::endl;
            std::cout << "input addresses are " << src_addr << ", " <<
                dst_addr << std::endl;
            std::cout << "initial mac is " << mach_init << ", " << macl_init <<
                std::endl;
            std::cout << "expected macl is " << macl_expect << std::endl;
            std::cout << "expected mach is " << mach_expect << std::endl;
            std::cout << "expected output addresses are " <<
                out_src_addr_expect << ", " << out_dst_addr_expect << std::endl;
            std::cout << "output macl is " << cpu->reg[SH4_REG_MACL] << std::endl;
            std::cout << "output mach is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "output addresses are " << *sh4_gen_reg(cpu, reg_src) <<
                ", " << *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_macl_indgeninc_indgeninc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                               RandGen32 *randgen32) {
        int failure = 0;


        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t src_addr =
                    pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                addr32_t dst_addr = src_addr;
                reg32_t src_val = randgen32->pick_val(0);
                reg32_t dst_val = src_val;
                if (reg_src != reg_dst) {
                    dst_addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                    dst_val = randgen32->pick_val(0);
                }
                uint32_t macl_init = randgen32->pick_val(0);
                uint32_t mach_init = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_macl_indgeninc_indgeninc(cpu, bios, mem, reg_src,
                                                       reg_dst, src_addr,
                                                       dst_addr, src_val,
                                                       dst_val, macl_init,
                                                       mach_init, false);

                failure = failure ||
                    do_binary_macl_indgeninc_indgeninc(cpu, bios, mem, reg_src,
                                                       reg_dst, src_addr,
                                                       dst_addr, src_val,
                                                       dst_val, macl_init,
                                                       mach_init, true);
            }
        }

        return failure;
    }

    // MAC.W @Rm+, @Rn+
    // 0100nnnnmmmm1111
    static int do_binary_macw_indgeninc_indgeninc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                                  unsigned reg_src,
                                                  unsigned reg_dst,
                                                  addr32_t src_addr,
                                                  addr32_t dst_addr,
                                                  uint32_t src_val,
                                                  uint32_t dst_val,
                                                  uint32_t macl_init,
                                                  uint32_t mach_init,
                                                  bool sat_flag) {
        static const int64_t MAX32 = 0x7fffffff;
        static const int64_t MIN32 = 0x80000000;

        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "MAC.W @R" << reg_src << "+, @R" << reg_dst << "+\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_addr;
        *sh4_gen_reg(cpu, reg_dst) = dst_addr;
        cpu->reg[SH4_REG_MACL] = macl_init;
        cpu->reg[SH4_REG_MACH] = mach_init;
        sh4_write_mem(cpu, &src_val, src_addr, sizeof(src_val));
        sh4_write_mem(cpu, &dst_val, dst_addr, sizeof(dst_val));
        if (sat_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_S_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_S_MASK;
        sh4_exec_inst(cpu);

        reg32_t macl_expect, mach_expect;

        if (sat_flag) {
            // 32-bit saturation arithmetic
            int32_t prod = int32_t(int16_t(dst_val)) *
                int32_t(int16_t(src_val));
            int32_t signed_macl = int32_t(macl_init);
            int32_t sum = prod + signed_macl;

            mach_expect = mach_init;
            if (sum < 0) {
                if (signed_macl >= 0 && prod >= 0) {
                    // overflow positive to negative
                    sum = MAX32;
                    mach_expect |= 1;
                }
            } else {
                if (signed_macl < 0 && prod < 0) {
                    // overflow negative to positive
                    sum = MIN32;
                    mach_expect |= 1;
                }
            }

            macl_expect = sum;
        } else {
            int64_t prod = int64_t(int16_t(dst_val)) * int64_t(int16_t(src_val));
            int64_t sum = prod +
                int64_t(uint64_t(macl_init) | (uint64_t(mach_init) << 32));
            macl_expect = uint64_t(sum) & 0xffffffff;
            mach_expect = uint64_t(sum) >> 32;
        }

        reg32_t out_src_addr_expect = src_addr + 2;
        reg32_t out_dst_addr_expect = dst_addr + 2;
        if (reg_src == reg_dst)
            out_src_addr_expect = out_dst_addr_expect = src_addr + 4;

        if (cpu->reg[SH4_REG_MACL] != macl_expect ||
            (cpu->reg[SH4_REG_MACH] & 1) != (mach_expect & 1) || // only check the LSB
            (*sh4_gen_reg(cpu, reg_src) != out_src_addr_expect) ||
            (*sh4_gen_reg(cpu, reg_dst) != out_dst_addr_expect)) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "the saturation flag is " << sat_flag << std::endl;
            std::cout << "inputs are " << std::hex <<
                src_val << ", " << dst_val << std::endl;
            std::cout << "input addresses are " << src_addr << ", " <<
                dst_addr << std::endl;
            std::cout << "initial mac is " << mach_init << ", " << macl_init <<
                std::endl;
            std::cout << "expected macl is " << macl_expect << std::endl;
            std::cout << "expected mach is " << mach_expect << std::endl;
            std::cout << "expected output addresses are " <<
                out_src_addr_expect << ", " << out_dst_addr_expect << std::endl;
            std::cout << "output macl is " << cpu->reg[SH4_REG_MACL] << std::endl;
            std::cout << "output mach is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "output addresses are " << *sh4_gen_reg(cpu, reg_src) <<
                ", " << *sh4_gen_reg(cpu, reg_dst) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_macw_indgeninc_indgeninc(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                               RandGen32 *randgen32) {
        int failure = 0;


        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t src_addr =
                    pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                addr32_t dst_addr = src_addr;
                reg32_t src_val = randgen32->pick_val(0);
                reg32_t dst_val = src_val;
                if (reg_src != reg_dst) {
                    dst_addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                    dst_val = randgen32->pick_val(0);
                }
                uint32_t macl_init = randgen32->pick_val(0);
                uint32_t mach_init = randgen32->pick_val(0);

                failure = failure ||
                    do_binary_macw_indgeninc_indgeninc(cpu, bios, mem, reg_src,
                                                       reg_dst, src_addr,
                                                       dst_addr, src_val,
                                                       dst_val, macl_init,
                                                       mach_init, false);

                failure = failure ||
                    do_binary_macw_indgeninc_indgeninc(cpu, bios, mem, reg_src,
                                                       reg_dst, src_addr,
                                                       dst_addr, src_val,
                                                       dst_val, macl_init,
                                                       mach_init, true);
            }
        }

        return failure;
    }

    // CLRMAC
    // 0000000000101000
    static int noarg_clrmac(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CLRMAC\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_MACH] = randgen32->pick_val(0);
        cpu->reg[SH4_REG_MACL] = randgen32->pick_val(0);
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_MACH] || cpu->reg[SH4_REG_MACL]) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of MACH is " << std::hex << cpu->reg[SH4_REG_MACH] <<
                std::endl;
            std::cout << "value of MACL is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }
        return 0;
    }

    // CLRS
    // 0000000001001000
    static int noarg_clrs(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CLRS\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_SR] = randgen32->pick_val(0) | SH4_SR_MD_MASK;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_S_MASK) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of SR is " << cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }
        return 0;
    }

    // CLRT
    // 0000000000001000
    static int noarg_clrt(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "CLRT\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_SR] = randgen32->pick_val(0) | SH4_SR_MD_MASK;
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of SR is " << cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }
        return 0;
    }

    // SETS
    // 0000000001011000
    static int noarg_sets(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SETS\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_SR] = randgen32->pick_val(0) | SH4_SR_MD_MASK;
        sh4_exec_inst(cpu);

        if (!(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_S_MASK)) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of SR is " << cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }
        return 0;
    }

    // SETT
    // 0000000000011000
    static int noarg_sett(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "SETT\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        cpu->reg[SH4_REG_SR] = randgen32->pick_val(0) | SH4_SR_MD_MASK;
        sh4_exec_inst(cpu);

        if (!(cpu->reg[SH4_REG_SR] & SH4_SR_FLAG_T_MASK)) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of SR is " << cpu->reg[SH4_REG_SR] << std::endl;
            return 1;
        }
        return 0;
    }

    // MOVCA.L R0, @Rn
    // 0000nnnn11000011
    static int do_movcal_binary_r0_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          unsigned addr, uint32_t val,
                                          unsigned reg_dst) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        uint32_t mem_val;

        if (0 == reg_dst)
            val = addr;

        ss << "MOVCA.L R0" << ", @R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, 0) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        sh4_read_mem(cpu, &mem_val, addr, sizeof(mem_val));

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

    static int movcal_binary_r0_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 4));
            failed = failed ||
                do_movcal_binary_r0_indgen(cpu, bios, mem, addr,
                                           randgen32->pick_val(0),
                                           reg_dst);
        }

        return failed;
    }

    // BT label
    // 10001001dddddddd
    static int do_bt_label(Sh4 *cpu, BiosFile *bios, Memory *mem, int8_t label,
                           addr32_t pc_init, bool t_flag) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BT 0x" << std::hex << unsigned(label) << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        if (t_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
        cpu->reg[SH4_REG_PC] = pc_init;
        sh4_exec_inst(cpu);

        addr32_t pc_expect;
        if (t_flag)
            pc_expect = int32_t(label) * 2 + 4 + pc_init;
        else
            pc_expect = pc_init + 2;

        if (cpu->reg[SH4_REG_PC] != pc_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << pc_init << std::endl;
            std::cout << "t flag is " << t_flag << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int bt_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int i = 0; i < 16; i++) {
            addr32_t pc = pick_addr(AddrRange(randgen32, 0,
                                              MEM_SZ - (1 + 256 * 2 + 4)));
            uint8_t label = randgen32->pick_val(0) & 0xff;

            failure = failure ||
                do_bt_label(cpu, bios, mem, label, pc, false);

            failure = failure ||
                do_bt_label(cpu, bios, mem, label, pc, true);
        }

        return failure;
    }

    // BF label
    // 10001011dddddddd
    static int do_bf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, int8_t label,
                           addr32_t pc_init, bool t_flag) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BF 0x" << std::hex << unsigned(label) << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        if (t_flag)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;
        cpu->reg[SH4_REG_PC] = pc_init;
        sh4_exec_inst(cpu);

        addr32_t pc_expect;
        if (!t_flag)
            pc_expect = int32_t(label) * 2 + 4 + pc_init;
        else
            pc_expect = pc_init + 2;

        if (cpu->reg[SH4_REG_PC] != pc_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << pc_init << std::endl;
            std::cout << "t flag is " << t_flag << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int bf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int i = 0; i < 16; i++) {
            addr32_t pc = pick_addr(AddrRange(randgen32, 0,
                                              MEM_SZ - (1 + 256 * 2 + 4)));
            uint8_t label = randgen32->pick_val(0) & 0xff;

            failure = failure ||
                do_bf_label(cpu, bios, mem, label, pc, false);

            failure = failure ||
                do_bf_label(cpu, bios, mem, label, pc, true);
        }

        return failure;
    }

    // BRAF Rn
    // 0000nnnn00100011
    static int do_braf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                             unsigned reg_src_mov, unsigned reg_dst_mov,
                             reg32_t reg_val, reg32_t mov_src_val,
                             reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BRAF R" << reg_no << "\n" <<
            "MOV R" << reg_src_mov << ", R" << reg_dst_mov << "\n" ;
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = 4 + pc_init + reg_val;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "addr is " << reg_val << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int braf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32, 0,
                                                      (MEM_SZ - 6) / 2));
                    reg32_t reg_val = pick_addr(AddrRange(randgen32, 0,
                                                          (MEM_SZ - 6) / 2));
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_src_mov == reg_no)
                        mov_src_val = reg_val;
                    if (reg_dst_mov == reg_no)
                        mov_dst_val = reg_val;
                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_braf_label(cpu, bios, mem, reg_no, reg_src_mov,
                                      reg_dst_mov, reg_val, mov_src_val,
                                      mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // BSRF Rn
    // 0000nnnn00000011
    static int do_bsrf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                             unsigned reg_src_mov, unsigned reg_dst_mov,
                             reg32_t reg_val, reg32_t mov_src_val,
                             reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BSRF R" << reg_no << "\n" <<
            "MOV R" << reg_src_mov << ", R" << reg_dst_mov << "\n" ;
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = 4 + pc_init + reg_val;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t pr_expect = 4 + pc_init;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);
        reg32_t pr_actual = cpu->reg[SH4_REG_PR];

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect ||
            pr_actual != pr_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "addr is " << reg_val << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pr_actual is " << pr_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "pr_expect is " << pr_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int bsrf_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32,
                                                      0, (MEM_SZ - 6) / 2));
                    reg32_t reg_val = pick_addr(AddrRange(randgen32,
                                                          0, (MEM_SZ - 6) / 2));
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_src_mov == reg_no)
                        mov_src_val = reg_val;
                    if (reg_dst_mov == reg_no)
                        mov_dst_val = reg_val;
                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_bsrf_label(cpu, bios, mem, reg_no, reg_src_mov,
                                      reg_dst_mov, reg_val, mov_src_val,
                                      mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // RTS
    // 0000000000001011
    static int do_rts_label(Sh4 *cpu, BiosFile *bios, Memory *mem,
                            unsigned reg_src_mov, unsigned reg_dst_mov,
                            reg32_t pr_val, reg32_t mov_src_val,
                            reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "RTS\n" <<
            "MOV R" << reg_src_mov << ", R" << reg_dst_mov << "\n" ;
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        cpu->reg[SH4_REG_PC] = pc_init;
        cpu->reg[SH4_REG_PR] = pr_val;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = pr_val;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t pr_expect = pr_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);
        reg32_t pr_actual = cpu->reg[SH4_REG_PR];

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect ||
            pr_actual != pr_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pr_actual is " << pr_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "pr_expect is " << pr_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int rts_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                addr32_t pc = pick_addr(AddrRange(randgen32,
                                                  0, (MEM_SZ - 6) / 2));
                reg32_t pr_val = pick_addr(AddrRange(randgen32,
                                                  0, (MEM_SZ - 6) / 2));
                reg32_t mov_src_val = randgen32->pick_val(0);
                reg32_t mov_dst_val = randgen32->pick_val(0);

                if (reg_dst_mov == reg_src_mov)
                    mov_dst_val = mov_src_val;

                failure = failure ||
                    do_rts_label(cpu, bios, mem, reg_src_mov, reg_dst_mov, pr_val,
                                 mov_src_val, mov_dst_val, pc);
            }
        }

        return failure;
    }

    // BSR label
    // 1011dddddddddddd
    static int do_bsr_label(Sh4 *cpu, BiosFile *bios, Memory *mem,
                            unsigned reg_src_mov, unsigned reg_dst_mov,
                            int16_t disp12, reg32_t mov_src_val,
                            reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BSR 0x" << std::hex << (disp12 & 0xfff) << "\n" <<
            "MOV R" << std::dec << reg_src_mov << ", R" << reg_dst_mov << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = 4 + pc_init + (disp12 << 1);
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t pr_expect = 4 + pc_init;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);
        reg32_t pr_actual = cpu->reg[SH4_REG_PR];

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect ||
            pr_actual != pr_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "disp12 is " << disp12 << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pr_actual is " << pr_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "pr_expect is " << pr_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int bsr_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(0, (MEM_SZ - 6) / 2));
                    int16_t disp12 = randgen32->pick_val(0) & 0xfff;
                    if (disp12 & 0x800)
                        disp12 |= ~0xfff;// sign-extend to 16 bits
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_bsr_label(cpu, bios, mem, reg_src_mov,
                                      reg_dst_mov, disp12, mov_src_val,
                                      mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // BRA label
    // 1010dddddddddddd
    static int do_bra_label(Sh4 *cpu, BiosFile *bios, Memory *mem,
                            unsigned reg_src_mov, unsigned reg_dst_mov,
                            int16_t disp12, reg32_t mov_src_val,
                            reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BRA 0x" << std::hex << (disp12 & 0xfff) << "\n" <<
            "MOV R" << std::dec << reg_src_mov << ", R" << reg_dst_mov << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = 4 + pc_init + (disp12 << 1);
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "disp12 is " << disp12 << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int bra_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(0, (MEM_SZ - 6) / 2));
                    int16_t disp12 = randgen32->pick_val(0) & 0xfff;
                    if (disp12 & 0x800)
                        disp12 |= ~0xfff;// sign-extend to 16 bits
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_bra_label(cpu, bios, mem, reg_src_mov,
                                      reg_dst_mov, disp12, mov_src_val,
                                      mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // BF/S label
    // 10001111dddddddd
    static int do_bfs_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_src_mov,
                            unsigned reg_dst_mov, int8_t disp8, bool t_val,
                            reg32_t mov_src_val, reg32_t mov_dst_val,
                            addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BF/S 0x" << std::hex << int(disp8) << "\n" <<
            "MOV R" << std::dec << reg_src_mov << ", R" << reg_dst_mov << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        cpu->reg[SH4_REG_PC] = pc_init;
        if (t_val)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect;
        if (t_val)
            pc_expect = pc_init + 4;
        else
            pc_expect = 4 + pc_init + (int32_t(disp8) << 1);
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "disp8 is " << int(disp8) << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "t flag is " << t_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int bfs_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32, 0,
                                                      (MEM_SZ - 6) / 2));
                    int8_t disp8 = randgen32->pick_val(0) & 0xfff;
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_bfs_label(cpu, bios, mem, reg_src_mov,
                                     reg_dst_mov, disp8, true, mov_src_val,
                                     mov_dst_val, pc);

                    failure = failure ||
                        do_bfs_label(cpu, bios, mem, reg_src_mov,
                                     reg_dst_mov, disp8, true, mov_src_val,
                                     mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // BT/S label
    // 10001101dddddddd
    static int do_bts_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_src_mov,
                            unsigned reg_dst_mov, int8_t disp8, bool t_val,
                            reg32_t mov_src_val, reg32_t mov_dst_val,
                            addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "BT/S 0x" << std::hex << int(disp8) << "\n" <<
            "MOV R" << std::dec << reg_src_mov << ", R" << reg_dst_mov << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        cpu->reg[SH4_REG_PC] = pc_init;
        if (t_val)
            cpu->reg[SH4_REG_SR] |= SH4_SR_FLAG_T_MASK;
        else
            cpu->reg[SH4_REG_SR] &= ~SH4_SR_FLAG_T_MASK;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect;
        if (t_val)
            pc_expect = 4 + pc_init + (int32_t(disp8) << 1);
        else
            pc_expect = pc_init + 4;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "disp8 is " << int(disp8) << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "t flag is " << t_val << std::endl;
            return 1;
        }

        return 0;
    }

    static int bts_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32, 0,
                                                      (MEM_SZ - 6) / 2));
                    int8_t disp8 = randgen32->pick_val(0) & 0xfff;
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_bts_label(cpu, bios, mem, reg_src_mov,
                                     reg_dst_mov, disp8, true, mov_src_val,
                                     mov_dst_val, pc);

                    failure = failure ||
                        do_bts_label(cpu, bios, mem, reg_src_mov,
                                     reg_dst_mov, disp8, true, mov_src_val,
                                     mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // JMP @Rn
    // 0100nnnn00101011
    static int do_jmp_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                            unsigned reg_src_mov, unsigned reg_dst_mov,
                            reg32_t reg_val, reg32_t mov_src_val,
                            reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "JMP @R" << reg_no << "\n" <<
            "MOV R" << reg_src_mov << ", R" << reg_dst_mov << "\n" ;
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = reg_val;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "addr is " << reg_val << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int jmp_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32,
                                                      0, MEM_SZ - 4));
                    reg32_t reg_val = pick_addr(AddrRange(randgen32,
                                                          0, MEM_SZ - 4));
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_src_mov == reg_no)
                        mov_src_val = reg_val;
                    if (reg_dst_mov == reg_no)
                        mov_dst_val = reg_val;
                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_jmp_label(cpu, bios, mem, reg_no, reg_src_mov,
                                     reg_dst_mov, reg_val, mov_src_val,
                                     mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // JSR @Rn
    // 0100nnnn00001011
    static int do_jsr_label(Sh4 *cpu, BiosFile *bios, Memory *mem, unsigned reg_no,
                            unsigned reg_src_mov, unsigned reg_dst_mov,
                            reg32_t reg_val, reg32_t mov_src_val,
                            reg32_t mov_dst_val, addr32_t pc_init) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "JSR @R" << reg_no << "\n" <<
            "MOV R" << reg_src_mov << ", R" << reg_dst_mov << "\n" ;
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        memory_load_binary<uint8_t>(mem, pc_init - ADDR_RAM_FIRST,
                                    inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src_mov) = mov_src_val;
        *sh4_gen_reg(cpu, reg_dst_mov) = mov_dst_val;
        *sh4_gen_reg(cpu, reg_no) = reg_val;
        cpu->reg[SH4_REG_PC] = pc_init;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        addr32_t pc_expect = reg_val;
        reg32_t reg_src_expect = mov_src_val;
        reg32_t reg_dst_expect = mov_src_val;
        reg32_t pr_expect = pc_init + 4;
        reg32_t reg_src_actual = *sh4_gen_reg(cpu, reg_src_mov);
        reg32_t reg_dst_actual = *sh4_gen_reg(cpu, reg_dst_mov);
        reg32_t pr_actual = cpu->reg[SH4_REG_PR];

        if (cpu->reg[SH4_REG_PC] != pc_expect ||
            reg_src_actual != reg_src_expect ||
            reg_dst_actual != reg_dst_expect ||
            pr_actual != pr_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "initial pc is " << std::hex << pc_init << std::endl;
            std::cout << "addr is " << reg_val << std::endl;
            std::cout << "reg_src_mov is " << reg_src_mov << std::endl;
            std::cout << "reg_dst_mov is " << reg_dst_mov << std::endl;
            std::cout << "reg_src_actual is " << reg_src_actual << std::endl;
            std::cout << "reg_dst_actual is " << reg_dst_actual << std::endl;
            std::cout << "pr_actual is " << pr_actual << std::endl;
            std::cout << "pc is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "expected pc is " << pc_expect << std::endl;
            std::cout << "reg_src_expect is " << reg_src_expect << std::endl;
            std::cout << "reg_dst_expect is " << reg_dst_expect << std::endl;
            std::cout << "pr_expect is " << pr_expect;
            return 1;
        }

        return 0;
    }

    static int jsr_label(Sh4 *cpu, BiosFile *bios, Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (int reg_src_mov = 0; reg_src_mov < 16; reg_src_mov++) {
            for (int reg_dst_mov = 0; reg_dst_mov < 16; reg_dst_mov++) {
                for (int reg_no = 0; reg_no < 16; reg_no++) {
                    addr32_t pc = pick_addr(AddrRange(randgen32,
                                                      0, MEM_SZ - 4));
                    reg32_t reg_val = pick_addr(AddrRange(randgen32,
                                                          0, MEM_SZ - 2));
                    reg32_t mov_src_val = randgen32->pick_val(0);
                    reg32_t mov_dst_val = randgen32->pick_val(0);

                    if (reg_src_mov == reg_no)
                        mov_src_val = reg_val;
                    if (reg_dst_mov == reg_no)
                        mov_dst_val = reg_val;
                    if (reg_dst_mov == reg_src_mov)
                        mov_dst_val = mov_src_val;

                    failure = failure ||
                        do_jsr_label(cpu, bios, mem, reg_no, reg_src_mov,
                                     reg_dst_mov, reg_val, mov_src_val,
                                     mov_dst_val, pc);
                }
            }
        }

        return failure;
    }

    // DMULS.L Rm, Rn
    // 0011nnnnmmmm1101
    static int do_dmulsl_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_m, unsigned reg_n,
                                 reg32_t reg_m_val, reg32_t reg_n_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "DMULS.L R" << reg_m << ", R" << reg_n << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_m) = reg_m_val;
        *sh4_gen_reg(cpu, reg_n) = reg_n_val;
        sh4_exec_inst(cpu);

        int64_t res = int64_t(reg_n_val) * int64_t(reg_m_val);
        reg32_t mach_expect = uint64_t(res) >> 32;
        reg32_t macl_expect = res & 0xffffffff;

        if (cpu->reg[SH4_REG_MACH] != mach_expect || cpu->reg[SH4_REG_MACL] != macl_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_m_val is " << std::hex << reg_m_val << std::endl;
            std::cout << "reg_n_val is " << reg_n_val << std::endl;
            std::cout << "mach_expect is " << mach_expect << std::endl;
            std::cout << "macl_expect is " << macl_expect << std::endl;
            std::cout << "mach is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "macl is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int dmulsl_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                              RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_m_no = 0; reg_m_no < 16; reg_m_no++) {
            for (unsigned reg_n_no = 0; reg_n_no < 16; reg_n_no++) {
                reg32_t reg_m_val = randgen32->pick_val(0);
                reg32_t reg_n_val = reg_m_val;

                if (reg_m_no != reg_n_no)
                    reg_n_val = randgen32->pick_val(0);

                failure = failure ||
                    do_dmulsl_gen_gen(cpu, bios, mem, reg_m_no, reg_n_no,
                                      reg_m_val, reg_n_val);
            }
        }

        return failure;
    }

    // DMULU.L Rm, Rn
    // 0011nnnnmmmm0101
    static int do_dmulul_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 unsigned reg_m, unsigned reg_n,
                                 reg32_t reg_m_val, reg32_t reg_n_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "DMULU.L R" << reg_m << ", R" << reg_n << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_m) = reg_m_val;
        *sh4_gen_reg(cpu, reg_n) = reg_n_val;
        sh4_exec_inst(cpu);

        uint64_t res = uint64_t(reg_n_val) * uint64_t(reg_m_val);
        reg32_t mach_expect = res >> 32;
        reg32_t macl_expect = res & 0xffffffff;

        if (cpu->reg[SH4_REG_MACH] != mach_expect || cpu->reg[SH4_REG_MACL] != macl_expect) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "reg_m_val is " << std::hex << reg_m_val << std::endl;
            std::cout << "reg_n_val is " << reg_n_val << std::endl;
            std::cout << "mach_expect is " << mach_expect << std::endl;
            std::cout << "macl_expect is " << macl_expect << std::endl;
            std::cout << "mach is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "macl is " << cpu->reg[SH4_REG_MACL] << std::endl;
            return 1;
        }

        return 0;
    }

    static int dmulul_gen_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                              RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned reg_m_no = 0; reg_m_no < 16; reg_m_no++) {
            for (unsigned reg_n_no = 0; reg_n_no < 16; reg_n_no++) {
                reg32_t reg_m_val = randgen32->pick_val(0);
                reg32_t reg_n_val = reg_m_val;

                if (reg_m_no != reg_n_no)
                    reg_n_val = randgen32->pick_val(0);

                failure = failure ||
                    do_dmulul_gen_gen(cpu, bios, mem, reg_m_no, reg_n_no,
                                      reg_m_val, reg_n_val);
            }
        }

        return failure;
    }

    // LDS Rm, FPSCR
    // 0100mmmm01101010
    static int do_binary_lds_gen_fpscr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS R" << reg_no << ", FPSCR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        if (cpu->fpu.fpscr != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << cpu->fpu.fpscr << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_lds_gen_fpscr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_lds_gen_fpscr(cpu, bios, mem, reg_no,
                                        randgen32->pick_val(0));
        }

        return failure;
    }

    // LDS.L @Rm+, FPSCR
    // 0100mmmm01100110
    static int do_binary_ldsl_indgeninc_fpscr(Sh4 *cpu, BiosFile *bios,
                                              Memory *mem, unsigned reg_no,
                                              addr32_t addr, uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS.L @R" << reg_no << "+, FPSCR\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        if (cpu->fpu.fpscr != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << cpu->fpu.fpscr << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr + 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldsl_indgeninc_fpscr(Sh4 *cpu, BiosFile *bios,
                                           Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            uint32_t val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_ldsl_indgeninc_fpscr(cpu, bios, mem,
                                               reg_no, addr, val);
        }

        return failure;
    }

    // STS FPSCR, Rn
    // 0000nnnn01101010
    static int do_binary_sts_fpscr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS FPSCR, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->fpu.fpscr = val;
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_sts_fpscr_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                    RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_sts_fpscr_gen(cpu, bios, mem, reg_no,
                                        randgen32->pick_val(0));
        }

        return failure;
    }

    // STS.L FPSCR, @-Rn
    // 0100nnnn01100010
    static int do_binary_stsl_fpscr_inddecgen(Sh4 *cpu, BiosFile *bios,
                                              Memory *mem, unsigned reg_no,
                                              reg32_t fpscr_val, addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS.L FPSCR, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        cpu->fpu.fpscr = fpscr_val;
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (mem_val != fpscr_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << fpscr_val <<
                std::endl;
            std::cout << "actual val is " << mem_val << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr - 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stsl_fpscr_inddecgen(Sh4 *cpu, BiosFile *bios,
                                           Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 1));
            reg32_t fpscr_val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_stsl_fpscr_inddecgen(cpu, bios, mem, reg_no,
                                               fpscr_val, addr);
        }

        return failure;
    }

    // FMOV FRm, FRn
    // 1111nnnnmmmm1100
    static int do_binary_fmov_fr_fr(Sh4 *cpu, BiosFile *bios,
                                    Memory *mem, unsigned src_reg_no,
                                    unsigned dst_reg_no, float val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV FR" << src_reg_no << ", FR" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_fpu_fr(cpu, src_reg_no) = val;
        sh4_exec_inst(cpu);

        float actual_val = *sh4_fpu_fr(cpu, dst_reg_no);
        if (actual_val != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected value of FR" << dst_reg_no << " is " <<
                val << std::endl;
            std::cout << "actual value is " << actual_val << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_fmov_fr_fr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < SH4_N_FLOAT_REGS; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < SH4_N_FLOAT_REGS; dst_reg++) {
                float f_val = randgen32->pick_double();

                failure = failure ||
                    do_binary_fmov_fr_fr(cpu, bios, mem, src_reg, dst_reg,
                                         f_val);
            }
        }

        return failure;
    }

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    static int do_binary_fmovs_indgen_fr(Sh4 *cpu, BiosFile *bios,
                                         Memory *mem, unsigned src_reg_no,
                                         unsigned dst_reg_no, addr32_t addr,
                                         float val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV.S @R" << src_reg_no << ", FR" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, src_reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);

        float actual_val = *sh4_fpu_fr(cpu, dst_reg_no);
        if (actual_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex << actual_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmovs_indgen_fr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < 16; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < SH4_N_FLOAT_REGS; dst_reg++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0,
                                                    memory_size(mem) - 4));
                float f_val = randgen32->pick_double();
                failure = failure ||
                    do_binary_fmovs_indgen_fr(cpu, bios, mem, src_reg,
                                              dst_reg, addr, f_val);
            }
        }

        return failure;
    }

    // FMOV.S @(R0,Rm), FRn
    // 1111nnnnmmmm0110
    static int do_binary_fmovs_ind_r0_gen_fr(Sh4 *cpu, BiosFile *bios,
                                             Memory *mem, unsigned reg_src,
                                             unsigned reg_dst, unsigned r0_val,
                                             unsigned src_val, float f_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV.S @(R0, R" << reg_src << "), FR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_write_mem(cpu, &f_val, r0_val + src_val, sizeof(f_val));
        sh4_exec_inst(cpu);

        float val_actual = *sh4_fpu_fr(cpu, reg_dst);

        if (val_actual != f_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "src_val is " << src_val << std::endl;
            std::cout << "f_val is " << f_val << std::endl;
            std::cout << "actual output is " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmovs_ind_r0_gen_fr(Sh4 *cpu, BiosFile *bios,
                                          Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < SH4_N_FLOAT_REGS; reg_dst++) {
                float f_val = randgen32->pick_double();
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;

                if (reg_src == 0)
                    base_addr = r0_val;

                failure = failure ||
                    do_binary_fmovs_ind_r0_gen_fr(cpu, bios, mem, reg_src,
                                                  reg_dst, r0_val, base_addr,
                                                  f_val);
            }
        }

        return failure;
    }

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    static int do_fmovs_binary_indgeninc_fr(Sh4 *cpu, BiosFile *bios,
                                            Memory *mem, unsigned reg_src,
                                            unsigned reg_dst, addr32_t addr,
                                            float f_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV.S @R" << reg_src << "+, FR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &f_val, addr, sizeof(f_val));
        sh4_exec_inst(cpu);

        reg32_t expected_addr_out = addr + 4;
        reg32_t actual_addr_out = *sh4_gen_reg(cpu, reg_src);

        float actual_val = *sh4_fpu_fr(cpu, reg_dst);

        if ((actual_val != f_val) || (expected_addr_out != actual_addr_out)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "expected val is " << f_val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << *sh4_fpu_fr(cpu, reg_dst) << std::endl;
            std::cout << "expected_addr_out is " << expected_addr_out <<
                std::endl;
            std::cout << "actual_addr_out is " << actual_addr_out << std::endl;
            return 1;
        }

        return 0;
    }

    static int fmovs_binary_indgeninc_fr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < SH4_N_FLOAT_REGS; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
                float f_val = randgen32->pick_double();
                failed = failed ||
                    do_fmovs_binary_indgeninc_fr(cpu, bios, mem, reg_src,
                                                 reg_dst, addr, f_val);
            }
        }

        return failed;
    }

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    static int do_binary_fmovs_fr_indgen(Sh4 *cpu, BiosFile *bios,
                                         Memory *mem, unsigned src_reg_no,
                                         unsigned dst_reg_no, addr32_t addr,
                                         float f_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV.S FR" << src_reg_no << ", @R" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, dst_reg_no) = addr;
        *sh4_fpu_fr(cpu, src_reg_no) = f_val;
        sh4_exec_inst(cpu);

        float val_actual;
        sh4_read_mem(cpu, &val_actual, addr, sizeof(val_actual));

        if (val_actual != f_val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "f_val is " << f_val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex << val_actual <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmovs_fr_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < 16; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < SH4_N_FLOAT_REGS; dst_reg++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0,
                                                    memory_size(mem) - 4));
                float f_val = randgen32->pick_double();
                failure = failure ||
                    do_binary_fmovs_fr_indgen(cpu, bios, mem, src_reg,
                                              dst_reg, addr, f_val);
            }
        }

        return failure;
    }

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    static int do_fmovs_binary_fr_inddecgen(Sh4 *cpu, BiosFile *bios,
                                            Memory *mem, unsigned reg_src,
                                            unsigned reg_dst, addr32_t addr,
                                            float f_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // increment addr 'cause the opcode is going to decrement it
        addr += 4;

        ss << "FMOV.S FR" << reg_src << ", @-R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_fr(cpu, reg_src) = f_val;
        *sh4_gen_reg(cpu, reg_dst) = addr;
        sh4_exec_inst(cpu);

        float val_actual;
        sh4_read_mem(cpu, &val_actual, addr - 4, sizeof(val_actual));

        addr32_t addr_out_expect = addr - 4;
        addr32_t addr_out_actual = *sh4_gen_reg(cpu, reg_dst);

        if ((val_actual != f_val) || (addr_out_actual != addr_out_expect)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << f_val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            std::cout << "addr_out_actual is " << addr_out_actual << std::endl;
            std::cout << "addr_out_expect is " << addr_out_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int fmovs_binary_fr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                         RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < SH4_N_FLOAT_REGS; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 4));
                float f_val = randgen32->pick_double();
                failed = failed ||
                    do_fmovs_binary_fr_inddecgen(cpu, bios, mem, reg_src,
                                                 reg_dst, addr, f_val);
            }
        }

        return failed;
    }

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    static int do_binary_fmovs_fr_ind_r0_gen(Sh4 *cpu, BiosFile *bios,
                                             Memory *mem, unsigned reg_src,
                                             unsigned reg_dst, unsigned r0_val,
                                             unsigned dst_val, float f_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FMOV.S FR" << reg_src << ", @(R0, R" << reg_dst << ")\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        *sh4_fpu_fr(cpu, reg_src) = f_val;
        sh4_exec_inst(cpu);

        float val_actual;
        sh4_read_mem(cpu, &val_actual, r0_val + dst_val, sizeof(val_actual));

        if (val_actual != f_val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "dst_val is " << dst_val << std::endl;
            std::cout << "f_val is " << f_val << std::endl;
            std::cout << "actual output val is " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmovs_fr_ind_r0_gen(Sh4 *cpu, BiosFile *bios,
                                          Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < SH4_N_FLOAT_REGS; reg_dst++) {
                float f_val = randgen32->pick_double();
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 4) / 2;

                if (reg_dst == 0)
                    base_addr = r0_val;

                failure = failure ||
                    do_binary_fmovs_fr_ind_r0_gen(cpu, bios, mem, reg_src,
                                                  reg_dst, r0_val, base_addr,
                                                  f_val);
            }
        }

        return failure;
    }

    /*
     * test the frchg opcode by filling up both banks, switching them, checking
     * the values for correctness then filling them up again and switching them
     * again and checking for correctness again.
     */
    static int noarg_frchg(Sh4 *cpu, BiosFile *bios, Memory *mem,
                           RandGen32 *randgen32) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;
        float val_bank0[SH4_N_FLOAT_REGS], val_bank1[SH4_N_FLOAT_REGS];
        static const addr32_t FVAL_START = 0x0c000000;
        int ret_val = 0;

        static const addr32_t FVAL_START_BYTE0 =  FVAL_START & 0x000000ff;
        static const addr32_t FVAL_START_BYTE1 = (FVAL_START & 0x0000ff00) >> 8;
        static const addr32_t FVAL_START_BYTE2 = (FVAL_START & 0x00ff0000) >> 16;
        static const addr32_t FVAL_START_BYTE3 = (FVAL_START & 0xff000000) >> 24;
        ss << "MOV #0x" << std::hex << FVAL_START_BYTE3 << ", R0\n" <<
            "SHLL8 R0\n" <<
            "OR #0x" << std::hex << FVAL_START_BYTE2 << ", R0\n" <<
            "SHLL8 R0\n" <<
            "OR #0x" << std::hex << FVAL_START_BYTE1 << ", R0\n" <<
            "SHLL8 R0\n" <<
            "OR #0x" << std::hex << FVAL_START_BYTE0 << ", R0\n" <<

            // load first bank
            "FMOV.S @R0+, FR0\n" <<
            "FMOV.S @R0+, FR1\n" <<
            "FMOV.S @R0+, FR2\n" <<
            "FMOV.S @R0+, FR3\n" <<
            "FMOV.S @R0+, FR4\n" <<
            "FMOV.S @R0+, FR5\n" <<
            "FMOV.S @R0+, FR6\n" <<
            "FMOV.S @R0+, FR7\n" <<
            "FMOV.S @R0+, FR8\n" <<
            "FMOV.S @R0+, FR9\n" <<
            "FMOV.S @R0+, FR10\n" <<
            "FMOV.S @R0+, FR11\n" <<
            "FMOV.S @R0+, FR12\n" <<
            "FMOV.S @R0+, FR13\n" <<
            "FMOV.S @R0+, FR14\n" <<
            "FMOV.S @R0+, FR15\n" <<

            // load second bank
            "FRCHG\n" <<
            "FMOV.S @R0+, FR0\n" <<
            "FMOV.S @R0+, FR1\n" <<
            "FMOV.S @R0+, FR2\n" <<
            "FMOV.S @R0+, FR3\n" <<
            "FMOV.S @R0+, FR4\n" <<
            "FMOV.S @R0+, FR5\n" <<
            "FMOV.S @R0+, FR6\n" <<
            "FMOV.S @R0+, FR7\n" <<
            "FMOV.S @R0+, FR8\n" <<
            "FMOV.S @R0+, FR9\n" <<
            "FMOV.S @R0+, FR10\n" <<
            "FMOV.S @R0+, FR11\n" <<
            "FMOV.S @R0+, FR12\n" <<
            "FMOV.S @R0+, FR13\n" <<
            "FMOV.S @R0+, FR14\n" <<
            "FMOV.S @R0+, FR15\n" <<

            "FRCHG\n"
            ;

        test_prog.add_txt(ss.str());
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        static const unsigned N_INSTS = 41;
        /*
         * Yes, I did actually hardcode the number of instructions
         * to execute.
         *
         * Deal with it.
         */
        reset_cpu(cpu);

        for (unsigned idx = 0; idx < SH4_N_FLOAT_REGS; idx++) {
            val_bank0[idx] = randgen32->pick_double();
            val_bank1[idx] = randgen32->pick_double();

            sh4_write_mem(cpu, val_bank0 + idx,
                           idx * sizeof(val_bank0[0]) + FVAL_START,
                           sizeof(val_bank0[0]));
            sh4_write_mem(cpu, val_bank1 + idx,
                           idx * sizeof(val_bank1[0]) + FVAL_START +
                           SH4_N_FLOAT_REGS * sizeof(val_bank0[0]),
                           sizeof(val_bank1[0]));
        }

        for (unsigned count = 0; count < N_INSTS; count++)
            sh4_exec_inst(cpu);

        if (cpu->fpu.fpscr & SH4_FPSCR_FR_MASK) {
            std::cout << "While testing FRCHG: the FR bit in FPSCR was set "
                "(it should have been cleared)" << std::endl;
            ret_val = 1;
        }

        for (unsigned idx = 0; idx < SH4_N_FLOAT_REGS; idx++) {
            if (val_bank0[idx] != cpu->fpu.reg_bank0.fr[idx]) {
                std::cout << "While testing FRCHG: bank0, register " << idx <<
                    " was expected to be " << val_bank0[idx] << "; the " <<
                    "actual value is " << cpu->fpu.reg_bank0.fr[idx] <<
                    std::endl;
                ret_val = 1;
            }

            if (val_bank1[idx] != cpu->fpu.reg_bank1.fr[idx]) {
                std::cout << "While testing FRCHG: bank1, register " << idx <<
                    " was expected to be " << val_bank1[idx] << "; the " <<
                    "actual value is " << cpu->fpu.reg_bank1.fr[idx] <<
                    std::endl;
                ret_val = 1;
            }
        }

        return ret_val;
    }

    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    static int do_binary_fmov_dr_dr(Sh4 *cpu, BiosFile *bios,
                                    Memory *mem, unsigned src_reg_no,
                                    unsigned dst_reg_no, double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV DR" << src_reg_no << ", DR" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_fpu_dr(cpu, src_reg_no >> 1) = val;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double actual_val = *sh4_fpu_dr(cpu, dst_reg_no >> 1);
        if (actual_val != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected value of DR" << dst_reg_no << " is " <<
                val << std::endl;
            std::cout << "actual value is " << actual_val << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_fmov_dr_dr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                 RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < SH4_N_DOUBLE_REGS; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < SH4_N_DOUBLE_REGS; dst_reg++) {
                double f_val = randgen32->pick_double();

                failure = failure ||
                    do_binary_fmov_dr_dr(cpu, bios, mem,
                                         src_reg * 2, dst_reg * 2, f_val);
            }
        }

        return failure;
    }

    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    static int do_binary_fmov_indgen_dr(Sh4 *cpu, BiosFile *bios,
                                        Memory *mem, unsigned src_reg_no,
                                        unsigned dst_reg_no, addr32_t addr,
                                        double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV @R" << src_reg_no << ", DR" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, src_reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));
        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double actual_val = *sh4_fpu_dr(cpu, dst_reg_no >> 1);
        if (actual_val != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex << actual_val <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmov_indgen_dr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < 16; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < SH4_N_DOUBLE_REGS;
                 dst_reg++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0,
                                                    memory_size(mem) - 8));
                double val = randgen32->pick_double();
                failure = failure ||
                    do_binary_fmov_indgen_dr(cpu, bios, mem, src_reg,
                                             dst_reg * 2, addr, val);
            }
        }

        return failure;
    }

    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    static int do_binary_fmov_ind_r0_gen_dr(Sh4 *cpu, BiosFile *bios,
                                            Memory *mem, unsigned reg_src,
                                            unsigned reg_dst, unsigned r0_val,
                                            unsigned src_val, double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV @(R0, R" << reg_src << "), DR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = src_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        sh4_write_mem(cpu, &val, r0_val + src_val, sizeof(val));

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double val_actual = *sh4_fpu_dr(cpu, reg_dst >> 1);

        if (val_actual != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "src_val is " << src_val << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "actual output is " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmov_ind_r0_gen_dr(Sh4 *cpu, BiosFile *bios,
                                         Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < SH4_N_DOUBLE_REGS;
                 reg_dst++) {
                double val = randgen32->pick_double();
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 8) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 8) / 2;

                if (reg_src == 0)
                    base_addr = r0_val;

                failure = failure ||
                    do_binary_fmov_ind_r0_gen_dr(cpu, bios, mem, reg_src,
                                                 reg_dst * 2, r0_val, base_addr,
                                                 val);
            }
        }

        return failure;
    }

    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    static int do_fmov_binary_indgeninc_dr(Sh4 *cpu, BiosFile *bios,
                                           Memory *mem, unsigned reg_src,
                                           unsigned reg_dst, addr32_t addr,
                                           double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV @R" << reg_src << "+, DR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_src) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        reg32_t expected_addr_out = addr + 8;
        reg32_t actual_addr_out = *sh4_gen_reg(cpu, reg_src);

        double actual_val = *sh4_fpu_dr(cpu, reg_dst >> 1);

        if ((actual_val != val) || (expected_addr_out != actual_addr_out)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << *sh4_fpu_fr(cpu, reg_dst) << std::endl;
            std::cout << "expected_addr_out is " << expected_addr_out <<
                std::endl;
            std::cout << "actual_addr_out is " << actual_addr_out << std::endl;
            return 1;
        }

        return 0;
    }

    static int fmov_binary_indgeninc_dr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < 16; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < SH4_N_DOUBLE_REGS;
                 reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 9));
                double val = randgen32->pick_double();
                failed = failed ||
                    do_fmov_binary_indgeninc_dr(cpu, bios, mem, reg_src,
                                                reg_dst * 2, addr, val);
            }
        }

        return failed;
    }

    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    static int do_binary_fmov_dr_indgen(Sh4 *cpu, BiosFile *bios,
                                        Memory *mem, unsigned src_reg_no,
                                        unsigned dst_reg_no, addr32_t addr,
                                        double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV DR" << src_reg_no << ", @R" << dst_reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, dst_reg_no) = addr;
        *sh4_fpu_dr(cpu, src_reg_no >> 1) = val;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double val_actual;
        sh4_read_mem(cpu, &val_actual, addr, sizeof(val_actual));

        if (val_actual != val) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << std::hex << val_actual <<
                std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmov_dr_indgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                     RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned src_reg = 0; src_reg < SH4_N_DOUBLE_REGS; src_reg++) {
            for (unsigned dst_reg = 0; dst_reg < 16; dst_reg++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 0,
                                                    memory_size(mem) - 8));
                double val = randgen32->pick_double();
                failure = failure ||
                    do_binary_fmov_dr_indgen(cpu, bios, mem, src_reg * 2,
                                             dst_reg, addr, val);
            }
        }

        return failure;
    }

    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    static int do_fmov_binary_dr_inddecgen(Sh4 *cpu, BiosFile *bios,
                                           Memory *mem, unsigned reg_src,
                                           unsigned reg_dst, addr32_t addr,
                                           double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // increment addr 'cause the opcode is going to decrement it
        addr += 8;

        ss << "FSCHG\n";
        ss << "FMOV DR" << reg_src << ", @-R" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_dr(cpu, reg_src >> 1) = val;
        *sh4_gen_reg(cpu, reg_dst) = addr;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double val_actual;
        sh4_read_mem(cpu, &val_actual, addr - 8, sizeof(val_actual));

        addr32_t addr_out_expect = addr - 8;
        addr32_t addr_out_actual = *sh4_gen_reg(cpu, reg_dst);

        if ((val_actual != val) || (addr_out_actual != addr_out_expect)) {
            std::cout << "While running: " << cmd << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "addr is " << std::hex << addr << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            std::cout << "addr_out_actual is " << addr_out_actual << std::endl;
            std::cout << "addr_out_expect is " << addr_out_expect << std::endl;
            return 1;
        }

        return 0;
    }

    static int fmov_binary_dr_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        RandGen32 *randgen32) {
        int failed = 0;

        for (unsigned reg_src = 0; reg_src < SH4_N_DOUBLE_REGS; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                addr32_t addr = pick_addr(AddrRange(randgen32, 8, MEM_SZ - 8));
                double val = randgen32->pick_double();
                failed = failed ||
                    do_fmov_binary_dr_inddecgen(cpu, bios, mem, reg_src * 2,
                                                reg_dst, addr, val);
            }
        }

        return failed;
    }

    // FMOV DRm, @(R0, Rn)
    // 1111nnnnmmm00111
    static int do_binary_fmov_dr_ind_r0_gen(Sh4 *cpu, BiosFile *bios,
                                            Memory *mem, unsigned reg_src,
                                            unsigned reg_dst, unsigned r0_val,
                                            unsigned dst_val, double val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSCHG\n";
        ss << "FMOV DR" << reg_src << ", @(R0, R" << reg_dst << ")\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_gen_reg(cpu, reg_dst) = dst_val;
        *sh4_gen_reg(cpu, 0) = r0_val;
        *sh4_fpu_dr(cpu, reg_src >> 1) = val;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        double val_actual;
        sh4_read_mem(cpu, &val_actual, r0_val + dst_val, sizeof(val_actual));

        if (val_actual != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "r0_val is " << std::hex << r0_val << std::endl;
            std::cout << "dst_val is " << dst_val << std::endl;
            std::cout << "val is " << val << std::endl;
            std::cout << "actual output val is " << val_actual << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_fmov_dr_ind_r0_gen(Sh4 *cpu, BiosFile *bios,
                                         Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_src = 0; reg_src < SH4_N_DOUBLE_REGS; reg_src++) {
            for (unsigned reg_dst = 0; reg_dst < 16; reg_dst++) {
                double val = randgen32->pick_double();
                /*
                 * the reason for the divide-by-two is so that they don't
                 * add up to be more than 16MB
                 */
                addr32_t base_addr =
                    (pick_addr(AddrRange(randgen32)) - 8) / 2;
                addr32_t r0_val =
                    (pick_addr(AddrRange(randgen32)) - 8) / 2;

                if (reg_dst == 0)
                    base_addr = r0_val;

                failure = failure ||
                    do_binary_fmov_dr_ind_r0_gen(cpu, bios, mem, reg_src * 2,
                                                 reg_dst, r0_val,
                                                 base_addr, val);
            }
        }

        return failure;
    }

    // FLDS FRm, FPUL
    // 1111mmmm00011101
    static int do_binary_flds_fr_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_src, float src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FLDS FR" << reg_src << ", FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_fr(cpu, reg_src) = src_val;

        sh4_exec_inst(cpu);

        float val_actual;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        if (val_actual != src_val) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << src_val << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_flds_fr_fpul(Sh4 *cpu, BiosFile *bios,
                                   Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_FLOAT_REGS; reg_no++) {
            failure = failure ||
                do_binary_flds_fr_fpul(cpu, bios, mem, reg_no,
                                       randgen32->pick_double());
        }

        return failure;
    }

    // FSTS FPUL, FRn
    // 1111nnnn00001101
    static int do_binary_fsts_fpul_fr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_dst, float src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FSTS FPUL, FR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        memcpy(&cpu->fpu.fpul, &src_val, sizeof(cpu->fpu.fpul));

        sh4_exec_inst(cpu);

        float val_actual = *sh4_fpu_fr(cpu, reg_dst);

        if (val_actual != src_val) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << src_val << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_fsts_fpul_fr(Sh4 *cpu, BiosFile *bios,
                                   Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_FLOAT_REGS; reg_no++) {
            failure = failure ||
                do_binary_fsts_fpul_fr(cpu, bios, mem, reg_no,
                                       randgen32->pick_double());
        }

        return failure;
    }

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    static int do_binary_float_fpul_fr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_dst, reg32_t src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FLOAT FPUL, FR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        memcpy(&cpu->fpu.fpul, &src_val, sizeof(cpu->fpu.fpul));

        sh4_exec_inst(cpu);

        float val_actual = *sh4_fpu_fr(cpu, reg_dst);

        if (val_actual != float(src_val)) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << src_val << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_float_fpul_fr(Sh4 *cpu, BiosFile *bios,
                                    Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_FLOAT_REGS; reg_no++) {
            failure = failure ||
                do_binary_float_fpul_fr(cpu, bios, mem, reg_no,
                                        randgen32->pick_val(0));
        }

        return failure;
    }

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    static int do_binary_ftrc_fr_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_src, float src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "FTRC FR" << reg_src << ", FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_fr(cpu, reg_src) = src_val;

        sh4_exec_inst(cpu);

        reg32_t val_actual, val_expect;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        int round_mode = arch_fegetround();
        arch_fesetround(ARCH_FE_TOWARDZERO);
        val_expect = src_val;
        arch_fesetround(round_mode);

        if (val_actual != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << val_expect << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_ftrc_fr_fpul(Sh4 *cpu, BiosFile *bios,
                                   Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_FLOAT_REGS; reg_no++) {
            failure = failure ||
                do_binary_ftrc_fr_fpul(cpu, bios, mem, reg_no,
                                       randgen32->pick_double());
        }

        return failure;
    }

    // FCNVDS DRm, FPUL
    // 1111mmm010111101
    static int do_binary_fcnvds_dr_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        unsigned reg_src, double src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // set the PR bit in FPSCR
        ss <<
            "STS FPSCR, R0\n"
            "XOR R1, R1\n"
            "MOV #1, R1\n"
            "SHLL8 R1\n"
            "SHLL8 R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "OR R1, R0\n"
            "LDS R0, FPSCR\n";
        ss << "FCNVDS DR" << reg_src << ", FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_dr(cpu, reg_src >> 1) = src_val;

        for (int i = 0; i < 11; i++)
            sh4_exec_inst(cpu);

        float val_actual, val_expect;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        val_expect = float(src_val);

        if (val_actual != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << val_expect << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_fcnvds_dr_fpul(Sh4 *cpu, BiosFile *bios,
                                     Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_DOUBLE_REGS; reg_no++) {
            failure = failure ||
                do_binary_fcnvds_dr_fpul(cpu, bios, mem, reg_no * 2,
                                         randgen32->pick_double());
        }

        return failure;
    }

    // FCNVSD FPUL, DRn
    // 1111nnn010101101
    static int do_binary_fcnvsd_fpul_dr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                        unsigned reg_dst, float src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // set the PR bit in FPSCR
        ss <<
            "STS FPSCR, R0\n"
            "XOR R1, R1\n"
            "MOV #1, R1\n"
            "SHLL8 R1\n"
            "SHLL8 R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "OR R1, R0\n"
            "LDS R0, FPSCR\n";
        ss << "FCNVSD FPUL, DR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        memcpy(&cpu->fpu.fpul, &src_val, sizeof(cpu->fpu.fpul));

        for (int i = 0; i < 11; i++)
            sh4_exec_inst(cpu);

        double val_actual, val_expect;
        val_actual = *sh4_fpu_dr(cpu, reg_dst >> 1);
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        val_expect = double(src_val);

        if (val_actual != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << val_expect << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_fcnvsd_fpul_dr(Sh4 *cpu, BiosFile *bios,
                                     Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_DOUBLE_REGS; reg_no++) {
            failure = failure ||
                do_binary_fcnvds_dr_fpul(cpu, bios, mem, reg_no * 2,
                                         randgen32->pick_double());
        }

        return failure;
    }

    // FLOAT FPUL, DRn
    // 1111nnn000101101
    static int do_binary_float_fpul_dr(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                       unsigned reg_dst, uint32_t src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // set the PR bit in FPSCR
        ss <<
            "STS FPSCR, R0\n"
            "XOR R1, R1\n"
            "MOV #1, R1\n"
            "SHLL8 R1\n"
            "SHLL8 R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "OR R1, R0\n"
            "LDS R0, FPSCR\n";
        ss << "FLOAT FPUL, DR" << reg_dst << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        memcpy(&cpu->fpu.fpul, &src_val, sizeof(cpu->fpu.fpul));

        for (int i = 0; i < 11; i++)
            sh4_exec_inst(cpu);

        double val_actual, val_expect;
        val_actual = *sh4_fpu_dr(cpu, reg_dst >> 1);
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        val_expect = double(src_val);

        if (val_actual != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << val_expect << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_float_fpul_dr(Sh4 *cpu, BiosFile *bios,
                                    Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_DOUBLE_REGS; reg_no++) {
            failure = failure ||
                do_binary_fcnvds_dr_fpul(cpu, bios, mem, reg_no * 2,
                                         randgen32->pick_double());
        }

        return failure;
    }

    // FTRC DRm, FPUL
    // 1111mmm000111101
    static int do_binary_ftrc_dr_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_src, double src_val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        // set the PR bit in FPSCR
        ss <<
            "STS FPSCR, R0\n"
            "XOR R1, R1\n"
            "MOV #1, R1\n"
            "SHLL8 R1\n"
            "SHLL8 R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "SHLL R1\n"
            "OR R1, R0\n"
            "LDS R0, FPSCR\n";
        ss << "FTRC DR" << reg_src << ", FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);
        *sh4_fpu_dr(cpu, reg_src >> 1) = src_val;

        for (int i = 0; i < 11; i++)
            sh4_exec_inst(cpu);

        reg32_t val_actual, val_expect;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        int round_mode = arch_fegetround();
        arch_fesetround(ARCH_FE_TOWARDZERO);
        val_expect = src_val;
        arch_fesetround(round_mode);

        if (val_actual != val_expect) {
            std::cout << "ERROR: while running " << cmd << std::endl;
            std::cout << "expected val is " << val_expect << std::endl;
            std::cout << "actual val is " << val_actual << std::endl;
            return 1;
        }
        return 0;
    }

    static int binary_ftrc_dr_fpul(Sh4 *cpu, BiosFile *bios,
                                   Memory *mem, RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < SH4_N_DOUBLE_REGS; reg_no++) {
            failure = failure ||
                do_binary_ftrc_dr_fpul(cpu, bios, mem, reg_no << 1,
                                       randgen32->pick_double());
        }

        return failure;
    }

    // LDS Rm, FPUL
    // 0100mmmm01011010
    static int do_binary_lds_gen_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS R" << reg_no << ", FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = val;
        sh4_exec_inst(cpu);

        reg32_t val_actual;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        if (val_actual != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACH] << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_lds_gen_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_lds_gen_fpul(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // LDS.L @Rm+, FPUL
    // 0100mmmm01010110
    static int do_binary_ldsl_indgeninc_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                             unsigned reg_no, addr32_t addr,
                                             uint32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "LDS.L @R" << reg_no << "+, FPUL\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        sh4_write_mem(cpu, &val, addr, sizeof(val));

        sh4_exec_inst(cpu);

        reg32_t val_actual;
        memcpy(&val_actual, &cpu->fpu.fpul, sizeof(val_actual));

        if (val_actual != val || *sh4_gen_reg(cpu, reg_no) != (addr + 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << val << std::endl;
            std::cout << "actual val is " << cpu->reg[SH4_REG_MACH] << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr + 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_ldsl_indgeninc_fpul(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 0, MEM_SZ - 5));
            uint32_t val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_ldsl_indgeninc_fpul(cpu, bios, mem, reg_no, addr, val);
        }

        return failure;
    }

    // STS FPUL, Rn
    // 0000nnnn01011010
    static int do_binary_sts_fpul_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                      unsigned reg_no, reg32_t val) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS FPUL, R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        memcpy(&cpu->fpu.fpul, &val, sizeof(cpu->fpu.fpul));
        sh4_exec_inst(cpu);

        if (*sh4_gen_reg(cpu, reg_no) != val) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << val << std::endl;
            std::cout << "actual val is " << *sh4_gen_reg(cpu, reg_no) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_sts_fpul_gen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                   RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            failure = failure ||
                do_binary_sts_fpul_gen(cpu, bios, mem, reg_no,
                                       randgen32->pick_val(0));
        }

        return failure;
    }

    // STS.L FPUL, @-Rn
    // 0100nnnn01010010
    static int do_binary_stsl_fpul_inddecgen(Sh4 *cpu, BiosFile *bios,
                                             Memory *mem, unsigned reg_no,
                                             reg32_t fpul_val, addr32_t addr) {
        Sh4Prog test_prog;
        std::stringstream ss;
        std::string cmd;

        ss << "STS.L FPUL, @-R" << reg_no << "\n";
        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        *sh4_gen_reg(cpu, reg_no) = addr;
        memcpy(&cpu->fpu.fpul, &fpul_val, sizeof(cpu->fpu.fpul));
        sh4_exec_inst(cpu);

        uint32_t mem_val;
        sh4_read_mem(cpu, &mem_val, addr - 4, sizeof(mem_val));

        if (mem_val != fpul_val || *sh4_gen_reg(cpu, reg_no) != (addr - 4)) {
            std::cout << "ERROR while running " << cmd << std::endl;
            std::cout << "expected val is " << std::hex << fpul_val << std::endl;
            std::cout << "actual val is " << mem_val << std::endl;
            std::cout << "input addr is " << addr << std::endl;
            std::cout << "output addr is " << (addr - 4) << std::endl;
            return 1;
        }

        return 0;
    }

    static int binary_stsl_fpul_inddecgen(Sh4 *cpu, BiosFile *bios, Memory *mem,
                                          RandGen32 *randgen32) {
        int failure = 0;

        for (unsigned reg_no = 0; reg_no < 16; reg_no++) {
            addr32_t addr = pick_addr(AddrRange(randgen32, 4, MEM_SZ - 1));
            reg32_t fpul_val = randgen32->pick_val(0);
            failure = failure ||
                do_binary_stsl_fpul_inddecgen(cpu, bios, mem, reg_no,
                                              fpul_val, addr);
        }

        return failure;
    }

    // RTE
    // 0000000000101011
    static int do_noarg_rte(Sh4 *cpu, BiosFile *bios, Memory *mem,
                            reg32_t ssr_val, reg32_t spc_val, reg32_t r3_val) {
        char const *prog_txt =
            "RTE\n"
            "MOV R3, R4\n";

        Sh4Prog test_prog;
        std::stringstream ss(prog_txt);
        std::string cmd;

        cmd = ss.str();
        test_prog.add_txt(cmd);
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios->load_binary<uint8_t>(0, inst.begin(), inst.end());

        reset_cpu(cpu);

        cpu->reg[SH4_REG_SSR] = ssr_val;
        cpu->reg[SH4_REG_SPC] = spc_val;

        // set r3, taking register bank switching into account
        if ((ssr_val & SH4_SR_RB_MASK) !=
            (cpu->reg[SH4_REG_SR] & SH4_SR_RB_MASK))
            *sh4_bank_reg(cpu, 3) = r3_val;
        else
            *sh4_gen_reg(cpu, 3) = r3_val;

        sh4_exec_inst(cpu);
        sh4_exec_inst(cpu);

        if (cpu->reg[SH4_REG_SR] != ssr_val ||
            cpu->reg[SH4_REG_PC] != spc_val ||
            *sh4_gen_reg(cpu, 4) != r3_val) {
            std::cout << "ERROR: While running " << cmd << std::endl;
            std::cout << "value of SR is " << std::hex <<
                cpu->reg[SH4_REG_SR] << std::endl;
            std::cout << "value of PC is " << cpu->reg[SH4_REG_PC] << std::endl;
            std::cout << "value of r4 is " << *sh4_gen_reg(cpu, 4) << std::endl;
            std::cout << "expected value of SR is " << ssr_val << std::endl;
            std::cout << "expected value of PC is " << spc_val << std::endl;
            std::cout << "expected value of r4 is " << r3_val << std::endl;
            return 1;
        }
        return 0;
    }

    static int noarg_rte(Sh4 *cpu, BiosFile *bios, Memory *mem,
                         RandGen32 *randgen32) {
        int failure = 0;
        for (unsigned i = 0; i < 256; i++) {
            reg32_t ssr_val = randgen32->pick_val(0);
            reg32_t spc_val = pick_addr(AddrRange(randgen32,
                                                  0, MEM_SZ - 4));
            reg32_t r3_val = randgen32->pick_val(0);

            /*
             * this is needed because the PC value set by reset_cpu is
             * 0xa0000000, and the new SR value is applied before the delay
             * slot is executed (as mandated by the sh4 software manual); if
             * the MD flag is not set then it will fail to read the delay slot
             * instruction.
             */
            ssr_val |= SH4_SR_MD_MASK;

            failure = failure ||
                do_noarg_rte(cpu, bios, mem, ssr_val, spc_val, r3_val);
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
    { "binary_lds_gen_mach", &Sh4InstTests::binary_lds_gen_mach },
    { "binary_lds_gen_macl", &Sh4InstTests::binary_lds_gen_macl },
    { "binary_lds_gen_pr", &Sh4InstTests::binary_lds_gen_pr },
    { "binary_sts_mach_gen", &Sh4InstTests::binary_sts_mach_gen },
    { "binary_sts_macl_gen", &Sh4InstTests::binary_sts_macl_gen },
    { "binary_sts_pr_gen", &Sh4InstTests::binary_sts_pr_gen },
    { "binary_ldsl_indgeninc_mach", &Sh4InstTests::binary_ldsl_indgeninc_mach },
    { "binary_ldsl_indgeninc_macl", &Sh4InstTests::binary_ldsl_indgeninc_macl },
    { "binary_ldsl_indgeninc_pr",  &Sh4InstTests::binary_ldsl_indgeninc_pr },
    { "binary_stsl_mach_inddecgen", &Sh4InstTests::binary_stsl_mach_inddecgen },
    { "binary_stsl_macl_inddecgen", &Sh4InstTests::binary_stsl_macl_inddecgen },
    { "binary_stsl_pr_inddecgen", &Sh4InstTests::binary_stsl_pr_inddecgen },
    { "unary_cmppz_gen", &Sh4InstTests::unary_cmppz_gen },
    { "unary_cmppl_gen", &Sh4InstTests::unary_cmppl_gen },
    { "binary_cmpeq_imm_gen", &Sh4InstTests::binary_cmpeq_imm_gen },
    { "binary_cmpeq_gen_gen", &Sh4InstTests::binary_cmpeq_gen_gen },
    { "binary_cmphs_gen_gen", &Sh4InstTests::binary_cmphs_gen_gen },
    { "binary_cmpge_gen_gen", &Sh4InstTests::binary_cmpge_gen_gen },
    { "binary_cmphi_gen_gen", &Sh4InstTests::binary_cmphi_gen_gen },
    { "binary_cmpgt_gen_gen", &Sh4InstTests::binary_cmpgt_gen_gen },
    { "binary_cmpstr_gen_gen", &Sh4InstTests::binary_cmpstr_gen_gen },
    { "binary_tst_gen_gen", &Sh4InstTests::binary_tst_gen_gen },
    { "unary_tasb_indgen", &Sh4InstTests::unary_tasb_indgen },
    { "binary_tst_imm_r0", &Sh4InstTests::binary_tst_imm_r0 },
    { "binary_tstb_imm_ind_r0_gbr", &Sh4InstTests::binary_tstb_imm_ind_r0_gbr },
    { "binary_and_gen_gen", &Sh4InstTests::binary_and_gen_gen },
    { "binary_and_imm_r0", &Sh4InstTests::binary_and_imm_r0 },
    { "binary_andb_imm_binind_r0_gbr", &Sh4InstTests::binary_andb_imm_binind_r0_gbr },
    { "binary_or_gen_gen", &Sh4InstTests::binary_or_gen_gen },
    { "binary_or_imm_r0", &Sh4InstTests::binary_or_imm_r0 },
    { "binary_orb_imm_binind_r0_gbr", &Sh4InstTests::binary_orb_imm_binind_r0_gbr },
    { "binary_xor_gen_gen", &Sh4InstTests::binary_xor_gen_gen },
    { "binary_xor_imm_r0", &Sh4InstTests::binary_xor_imm_r0 },
    { "binary_xorb_imm_binind_r0_gbr", &Sh4InstTests::binary_xorb_imm_binind_r0_gbr },
    { "binary_not_gen_gen", &Sh4InstTests::binary_not_gen_gen },
    { "binary_neg_gen_gen", &Sh4InstTests::binary_neg_gen_gen },
    { "binary_negc_gen_gen", &Sh4InstTests::binary_negc_gen_gen },
    { "unary_dt_gen", &Sh4InstTests::unary_dt_gen },
    { "binary_swapb_gen_gen", &Sh4InstTests::binary_swapb_gen_gen },
    { "binary_swapw_gen_gen", &Sh4InstTests::binary_swapw_gen_gen },
    { "binary_xtrct_gen_gen", &Sh4InstTests::binary_xtrct_gen_gen },
    { "binary_extsb_gen_gen", &Sh4InstTests::binary_extsb_gen_gen },
    { "binary_extsw_gen_gen", &Sh4InstTests::binary_extsw_gen_gen },
    { "binary_extub_gen_gen", &Sh4InstTests::binary_extub_gen_gen },
    { "binary_extuw_gen_gen", &Sh4InstTests::binary_extuw_gen_gen },
    { "unary_rotl_gen", &Sh4InstTests::unary_rotl_gen },
    { "unary_rotr_gen", &Sh4InstTests::unary_rotr_gen },
    { "unary_rotcl_gen", &Sh4InstTests::unary_rotcl_gen },
    { "unary_rotcr_gen", &Sh4InstTests::unary_rotcr_gen },
    { "binary_shad_gen", &Sh4InstTests::binary_shad_gen },
    { "unary_shal_gen", &Sh4InstTests::unary_shal_gen },
    { "unary_shar_gen", &Sh4InstTests::unary_shar_gen },
    { "binary_shld_gen", &Sh4InstTests::binary_shld_gen },
    { "unary_shll_gen", &Sh4InstTests::unary_shll_gen },
    { "unary_shlr_gen", &Sh4InstTests::unary_shlr_gen },
    { "unary_shll2_gen", &Sh4InstTests::unary_shll2_gen },
    { "unary_shlr2_gen", &Sh4InstTests::unary_shlr2_gen },
    { "unary_shll8_gen", &Sh4InstTests::unary_shll8_gen },
    { "unary_shlr8_gen", &Sh4InstTests::unary_shlr8_gen },
    { "unary_shll16_gen", &Sh4InstTests::unary_shll16_gen },
    { "unary_shlr16_gen", &Sh4InstTests::unary_shlr16_gen },
    { "binary_mull_gen_gen", &Sh4InstTests::binary_mull_gen_gen },
    { "binary_mulsw_gen_gen", &Sh4InstTests::binary_mulsw_gen_gen },
    { "binary_muluw_gen_gen", &Sh4InstTests::binary_muluw_gen_gen },
    { "binary_macl_indgeninc_indgeninc",
      &Sh4InstTests::binary_macl_indgeninc_indgeninc },
    { "binary_macw_indgeninc_indgeninc",
      &Sh4InstTests::binary_macw_indgeninc_indgeninc },
    { "noarg_clrmac", &Sh4InstTests::noarg_clrmac },
    { "noarg_clrs", &Sh4InstTests::noarg_clrs },
    { "noarg_clrt", &Sh4InstTests::noarg_clrt },
    { "noarg_sets", &Sh4InstTests::noarg_sets },
    { "noarg_sett", &Sh4InstTests::noarg_sett },
    { "movcal_binary_r0_indgen", &Sh4InstTests::movcal_binary_r0_indgen },
    { "bt_label", &Sh4InstTests::bt_label },
    { "bf_label", &Sh4InstTests::bf_label },
    { "braf_label", &Sh4InstTests::braf_label },
    { "bsrf_label", &Sh4InstTests::bsrf_label },
    { "rts_label", &Sh4InstTests::rts_label },
    { "bsr_label", &Sh4InstTests::bsr_label },
    { "bra_label", &Sh4InstTests::bra_label },
    { "bfs_label", &Sh4InstTests::bfs_label },
    { "bts_label", &Sh4InstTests::bts_label },
    { "jmp_label", &Sh4InstTests::jmp_label },
    { "jsr_label", &Sh4InstTests::jsr_label },
    { "dmulsl_gen_gen", &Sh4InstTests::dmulsl_gen_gen },
    { "dmulul_gen_gen", &Sh4InstTests::dmulul_gen_gen },
    { "binary_lds_gen_fpscr", &Sh4InstTests::binary_lds_gen_fpscr },
    { "binary_ldsl_indgeninc_fpscr",
      &Sh4InstTests::binary_ldsl_indgeninc_fpscr },
    { "binary_sts_fpscr_gen", &Sh4InstTests::binary_sts_fpscr_gen },
    { "binary_stsl_fpscr_inddecgen",
      &Sh4InstTests::binary_stsl_fpscr_inddecgen },
    { "binary_fmov_fr_fr", &Sh4InstTests::binary_fmov_fr_fr },
    { "binary_fmovs_indgen_fr", &Sh4InstTests::binary_fmovs_indgen_fr },
    { "binary_fmovs_ind_r0_gen_fr", &Sh4InstTests::binary_fmovs_ind_r0_gen_fr },
    { "fmovs_binary_indgeninc_fr", &Sh4InstTests::fmovs_binary_indgeninc_fr },
    { "binary_fmovs_fr_indgen", &Sh4InstTests::binary_fmovs_fr_indgen },
    { "fmovs_binary_fr_inddecgen", &Sh4InstTests::fmovs_binary_fr_inddecgen },
    { "binary_fmovs_fr_ind_r0_gen", &Sh4InstTests::binary_fmovs_fr_ind_r0_gen },
    { "noarg_frchg", &Sh4InstTests::noarg_frchg },
    { "binary_fmov_dr_dr", &Sh4InstTests::binary_fmov_dr_dr },
    { "binary_fmov_indgen_dr", &Sh4InstTests::binary_fmov_indgen_dr },
    { "binary_fmov_ind_r0_gen_dr", &Sh4InstTests::binary_fmov_ind_r0_gen_dr },
    { "fmov_binary_indgeninc_dr", &Sh4InstTests::fmov_binary_indgeninc_dr },
    { "binary_fmov_dr_indgen", &Sh4InstTests::binary_fmov_dr_indgen },
    { "fmov_binary_dr_inddecgen", &Sh4InstTests::fmov_binary_dr_inddecgen },
    { "binary_fmov_dr_ind_r0_gen", &Sh4InstTests::binary_fmov_dr_ind_r0_gen },
    { "binary_flds_fr_fpul", &Sh4InstTests::binary_flds_fr_fpul },
    { "binary_fsts_fpul_fr", &Sh4InstTests::binary_fsts_fpul_fr },
    { "binary_float_fpul_fr", &Sh4InstTests::binary_float_fpul_fr },
    { "binary_ftrc_fr_fpul", &Sh4InstTests::binary_ftrc_fr_fpul },
    { "binary_fcnvds_dr_fpul", &Sh4InstTests::binary_fcnvds_dr_fpul },
    { "binary_fcnvsd_fpul_dr", &Sh4InstTests::binary_fcnvsd_fpul_dr },
    { "binary_float_fpul_dr", &Sh4InstTests::binary_float_fpul_dr },
    { "binary_ftrc_dr_fpul", &Sh4InstTests::binary_ftrc_dr_fpul },
    { "binary_lds_gen_fpul", &Sh4InstTests::binary_lds_gen_fpul },
    { "binary_ldsl_indgeninc_fpul", &Sh4InstTests::binary_ldsl_indgeninc_fpul },
    { "binary_sts_fpul_gen", &Sh4InstTests::binary_sts_fpul_gen },
    { "binary_stsl_fpul_inddecgen", &Sh4InstTests::binary_stsl_fpul_inddecgen },
    { "noarg_rte", &Sh4InstTests::noarg_rte },
    { NULL }
};

int main(int argc, char **argv) {
    struct Memory mem;
    memory_init(&mem, 16 * 1024 * 1024);
    BiosFile bios;
    memory_map_init(&bios, &mem);
    Sh4 cpu;
    struct inst_test *test = inst_tests;
    int n_success = 0, n_tests = 0;
    unsigned int seed = time(NULL);
    int opt;
    int ret_val = 0;
    double percent;

    sh4_init(&cpu);

    while ((opt = getopt(argc, argv, "s:")) > 0) {
        if (opt == 's')
            seed = atoi(optarg);
    }

    try {
        RandGen32 randgen32(seed);
        randgen32.reset();

        while (test->name) {
            std::cout << "Trying " << test->name << "..." << std::endl;

            int test_ret = test->func(&cpu, &bios, &mem, &randgen32);

            if (test_ret != 0)
                std::cout << test->name << " FAIL" << std::endl;
            else {
                std::cout << test->name << " SUCCESS" << std::endl;
                n_success++;
            }

            test++;
            n_tests++;
        }
    } catch (BaseException& err) {
        std::cerr << boost::diagnostic_information(err);
        ret_val = 1;
        goto on_exit;
    }

    percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << std::dec << n_tests << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;

    if (n_success == n_tests) {
        ret_val = 0;
        goto on_exit;
    }

    ret_val = 1;
on_exit:
    sh4_cleanup(&cpu);
    return ret_val;
}
