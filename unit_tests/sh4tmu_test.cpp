/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

/*
 * unit-test the sh4's tmu by running a program that sets up, channel 0
 * then spins until a TUNI0 interrupt.  This program returns 0 on success and
 * 1 on timeout.
 */

#include <string>
#include <iostream>

#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_excp.h"
#include "tool/sh4asm/sh4asm.hpp"
#include "BaseException.hpp"
#include "memory.h"
#include "BiosFile.h"
#include "MemoryMap.hpp"

/*
 * loads a program into the given address.  the InputIterator's
 * indirect method (overload*) should return a data_tp.
 */
static void bios_load_binary(BiosFile *bios, addr32_t where,
                             Sh4Prog::ByteList::const_iterator start,
                             Sh4Prog::ByteList::const_iterator end) {
    size_t bytes_written = 0;

    bios_file_clear(bios);

    for (Sh4Prog::ByteList::const_iterator it = start; it != end; it++) {
        uint8_t tmp = *it;

        if (bytes_written + sizeof(uint8_t) >= bios->dat_len)
            BOOST_THROW_EXCEPTION(InvalidParamError());

        memcpy(bios->dat + bytes_written, &tmp, sizeof(tmp));
        bytes_written += sizeof(tmp);
    }
}

char const prog_asm[] =
    /*
     * the interrupt vector will be at 0x0c000600 (VBR == 0x0c000000)
     * so load that address into R1
     */
    "MOV #0x0c, R1\n"
    "SHLL8 R1\n"
    "SHLL8 R1\n"
    "SHLL8 R1\n"
    "LDC R1, VBR\n" /* set VBR, this part is very important! */
    "MOV #6, R2\n"
    "SHLL8 R2\n"
    "OR R2, R1\n"

    /*
     * now load the branch instruction into R0.
     * The value should be equivalent to:
     * 0xad7e == "BRA 0xd7e" (which should branch to 0x0c000100)
     */
    "MOV #0x0a, R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "OR #0x0d, R0\n"
    "SHLL8 R0\n"
    "OR #0x7e, R0\n"

    /* now write the exception handler's first instruction */
    "MOV.W R0, @R1\n"

    /* now load a NOP instruction into the next instruction */
    "MOV #9, R0\n"
    "MOV.W R0, @(2, R1)\n"

    /*
     * so at this point we have an instruction vector programmed,
     * now let's turn on the TMU.
     *
     * We'll be using the default prescaler, which is the peripheral clock
     * divided by four.  The peripheral clock is 1/4 the CPU clock.
     * Ergo, there will be one tmu tick every 16 cycles.
     *
     * We'll have it count down from 16, so there should be 16*17 = 272 cycles
     * until the interrupt occurs (because the interrupt occurs after the
     * value in TCNT undeflows)
     *
     * first move 0xffd80000 into R2, this is the base we'll use to
     * reference the TMU registers.
     */
    "MOV #0xd8, R2\n"
    "SHLL8 R2\n"
    "SHLL8 R2\n"
    "SHLL8 R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"
    "SHAR R2\n"

    /*
     * make sure R15 is clear.  If the exception handling works, then the
     * exception handler will take us to the exit address before we have a
     * chance to write 1 to it.
     */
    "XOR R15, R15\n"

    /*
     * R3 is the number of times to loop.  This should be 136 rather than
     * 272 because I execute two instructions on every iteration of the loop
     */
    "MOV #0x8, R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "ADD #8, R0\n"
    "MOV R0, R3\n"

    /* set TMU0 priority to 1 (lowest) */
    "MOV #0xfd, R1\n"
    "SHLL8 R1\n"
    "SHLL8 R1\n"
    "SHLL R1\n"
    "SHLL R1\n"
    "SHLL R1\n"
    "SHLL R1\n"
    /* R1 now holds ICR address (0xffd00000) */
    "MOV.W @(4, R1), R0\n" /* move IPRA into R0 */
    /* R5 will hold the new value for the TMU priority (0x1000) */
    "MOV #0x10, R5\n"
    "SHLL8 R5\n"
    /* no need to clear the old TMU prio because it defautls to 0 */
    "OR R5, R0\n"
    "MOV.W R0, @(4, R1)\n"

    /* now unmask the TMU0 interrupt and clear the BL bit */
    "STC SR, R5\n"
    "MOV #0xf, R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "SHLL R0\n"
    "NOT R0, R0\n"
    "AND R0, R5\n"
    "MOV #0x10, R0\n"
    "SHLL8 R0\n"
    "SHLL8 R0\n"
    "SHLL8 R0\n"
    "NOT R0, R0\n"
    "AND R0, R5\n"
    "LDC R5, SR\n"

    /* now move 16 into TCNT0 */
    "MOV #0x10, R0\n"
    "MOV.L R0, @(12, R2)\n"

    /* and move 16 into TCOR0 */
    "MOV.L R0, @(8, R2)\n"

    /* and enable the underflow interrupt in TCR0 */
    "MOV.W @(16, R2), R0\n"
    "OR #0x20, R0\n"
    "MOV.W R0, @(16, R2)\n"

    /* and start the countdown by writing 1 into TSTR */
    "MOV #1, R0\n"
    "MOV.B R0, @(4, R2)\n"

    /* now loop */
    "DT R3\n"
    "BF 0xfd\n" /* branch back two bytes to DT again */

    /*
     * if we reach this point, it means the test has failed.
     * Move 0xff into R15 to signal failure and jump to the
     * exit point (0x0c000100)
     */
    "MOV #1, R15\n"

    /* now jump */
    "MOV #0x0c, R0\n"
    "SHLL8 R0\n"
    "SHLL8 R0\n"
    "SHLL8 R0\n"
    "MOV #1, R1\n"
    "SHLL8 R1\n"
    "OR R1, R0\n"
    "JMP @R0\n"
    "NOP\n"
    "";

int main(int argc, char **argv) {
    BiosFile bios;
    try {
        Sh4Prog test_prog;
        struct Memory mem;
        Sh4 sh4;
        int ret_code = 0;

        bios_file_init_empty(&bios);
        memory_init(&mem, 16 * 1024 * 1024);
        memory_map_init(&bios, &mem);
        sh4_init(&sh4);

        test_prog.add_txt(std::string(prog_asm));
        const Sh4Prog::ByteList& inst = test_prog.get_prog();
        bios_load_binary(&bios, 0, inst.begin(), inst.end());

        sh4_run_until(&sh4, 0x0c000100);

        ret_code = *sh4_gen_reg(&sh4, 15);

        if (ret_code) {
            std::cerr << "Error: timer interrupt even failed to occur; "
                "test returned " << ret_code << std::endl;
        } else {
            std::cout << "the remaining value in TCNT0 is " <<
                sh4.reg[SH4_REG_TCNT0] << std::endl;
            if (sh4.reg[SH4_REG_TCNT0] != 0x10) {
                std::cerr << "remaining TCNT0 should have been inital "
                    "value (0x10)!" << std::endl;
                ret_code = 1;
            }

            /*
             * Predicting the exact number of remaining loops can be hard
             * since it depends on what the value of sh4->tmu.last_tick, and
             * the value of that going into the loop can change if I add more
             * code to the beginning of the loop.  Because of this, I just make
             * sure it seems low enough.  Being within 3 iterations means that
             * it was within one tick of the bus clock that feeds the tmu.
             */
            unsigned rem_loops = *sh4_gen_reg(&sh4, 3);
            std::cout << "There were " << rem_loops << " remaining "
                "iterations of the loop" << std::endl;
            if (rem_loops > 3) {
                std::cerr << "Lower is always better, but I don't accept more "
                    "than 3 iterations!" << std::endl;
                ret_code = 1;
            }

            if (((sh4.reg[SH4_REG_INTEVT] & SH4_INTEVT_CODE_MASK) >>
                 SH4_INTEVT_CODE_SHIFT) != SH4_EXCP_TMU0_TUNI0) {
                std::cerr << "bad intevt code value (interrupt reason "
                    "is not TUNI0)!" << std::endl;
                std::cerr << "intevt value is " << std::hex <<
                    sh4.reg[SH4_REG_INTEVT] << std::endl;
                ret_code = 1;
            }
        }

        if (ret_code)
            std::cout << "TEST FAILURE" << std::endl;
        else
            std::cout << "TEST SUCCESS" << std::endl;

        bios_file_cleanup(&bios);
        sh4_cleanup(&sh4);

        return ret_code;
    } catch (BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
        bios_file_cleanup(&bios);
    }

    return 1;
}
