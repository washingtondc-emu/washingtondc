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

#include <err.h>
#include <string.h>

#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_excp.h"
#include "memory.h"
#include "BiosFile.h"
#include "MemoryMap.h"
#include "sh4asm_core/sh4_bin_emit.h"
#include "dreamcast.h"

#define INST_MAX 256
uint16_t inst_list[INST_MAX];
unsigned inst_count;

static void bios_load_binary(BiosFile *bios, addr32_t where);
static void emit(uint16_t inst);
static void run_until(Sh4 *sh4, addr32_t addr);

static void bios_load_binary(BiosFile *bios, addr32_t where) {
    size_t bytes_written = 0;

    bios_file_clear(bios);

    unsigned inst_no;
    for (inst_no = 0; inst_no < inst_count; inst_no++) {
        if (bytes_written + sizeof(uint16_t) >= bios->dat_len)
            err(1, "out of bios memory");

        memcpy(bios->dat + bytes_written, inst_list + inst_no, sizeof(uint16_t));
        bytes_written += sizeof(uint16_t);
    }
}

static void emit(uint16_t inst) {
    if (inst_count < INST_MAX)
        inst_list[inst_count++] = inst;
}

#define r0 0
#define r1 1
#define r2 2
#define r3 3
#define r4 4
#define r5 5
#define r6 6
#define r7 7
#define r8 8
#define r9 9
#define r10 10
#define r11 11
#define r12 12
#define r13 13
#define r14 14
#define r15 15

static void compile(void) {
    /*
     * the interrupt vector will be at 0x0c000600 (VBR == 0x0c000000)
     * so load that address into R1
     */
    sh4_bin_mov_imm8_rn(emit, 0x0c, r1);
    sh4_bin_shll8_rn(emit, r1);
    sh4_bin_shll8_rn(emit, r1);
    sh4_bin_shll8_rn(emit, 1);
    sh4_bin_ldc_rm_vbr(emit, r1); /* set VBR, this part is very important! */
    sh4_bin_mov_imm8_rn(emit, 6, r2);
    sh4_bin_shll8_rn(emit, r2);
    sh4_bin_or_rm_rn(emit, r2, r1);

    /*
     * now load the branch instruction into R0.
     * The value should be equivalent to:
     * 0xad7e == "BRA 0xd7e" (which should branch to 0x0c000100)
     */
    sh4_bin_mov_imm8_rn(emit, 0xa, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_or_imm8_r0(emit, 0x0d);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_or_imm8_r0(emit, 0x7e);

    /* now write the exception handler's first instruction */
    sh4_bin_movw_rm_arn(emit, r0, r1);

    sh4_bin_mov_imm8_rn(emit, 9, r0);
    sh4_bin_movw_r0_a_disp4_rn(emit, 2, r1);

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
    sh4_bin_mov_imm8_rn(emit, 0xd8, r2);
    sh4_bin_shll8_rn(emit, r2);
    sh4_bin_shll8_rn(emit, r2);
    sh4_bin_shll8_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);
    sh4_bin_shar_rn(emit, r2);

    /*
     * make sure R15 is clear.  If the exception handling works, then the
     * exception handler will take us to the exit address before we have a
     * chance to write 1 to it.
     */
    sh4_bin_xor_rm_rn(emit, r15, r15);

    /*
     * R3 is the number of times to loop.  This should be 136 rather than
     * 272 because I execute two instructions on every iteration of the loop
     */
    sh4_bin_mov_imm8_rn(emit, 8, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_add_imm8_rn(emit, 8, r0);
    sh4_bin_mov_rm_rn(emit, r0, r3);

    /*
     * Double r3 from 136 to 272
     *
     * XXX This didn't used to be in the original sh4tmu_test.cpp.
     * I think it's needed now because of better cycle-counting; the
     * two instructions in the loop may be executing in parallel.
     */
    sh4_bin_shll_rn(emit, r3);

    /* set TMU0 priority to 1 (lowest) */
    sh4_bin_mov_imm8_rn(emit, 0xfd, r1);
    sh4_bin_shll8_rn(emit, r1);
    sh4_bin_shll8_rn(emit, r1);
    sh4_bin_shll_rn(emit, r1);
    sh4_bin_shll_rn(emit, r1);
    sh4_bin_shll_rn(emit, r1);
    sh4_bin_shll_rn(emit, r1);
    /* R1 now holds ICR address (0xffd00000) */
    sh4_bin_movw_a_disp4_rm_r0(emit, 4, r1); /* move IPRA into R0 */
    /* R5 will hold the new value for the TMU priority (0x1000) */
    sh4_bin_mov_imm8_rn(emit, 0x10, r5);
    sh4_bin_shll8_rn(emit, r5);
    /* no need to clear the old TMU prio because it defaults to 0 */
    sh4_bin_or_rm_rn(emit, r5, r0);
    sh4_bin_movw_r0_a_disp4_rn(emit, 4, r1);

    /* now unmask the TMU0 interrupt and clear the BL bit */
    sh4_bin_stc_sr_rn(emit, r5);
    sh4_bin_mov_imm8_rn(emit, 0xf, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_shll_rn(emit, r0);
    sh4_bin_not_rm_rn(emit, r0, r0);
    sh4_bin_and_rm_rn(emit, r0, r5);
    sh4_bin_mov_imm8_rn(emit, 0x10, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_not_rm_rn(emit, r0, r0);
    sh4_bin_and_rm_rn(emit, r0, r5);
    sh4_bin_ldc_rm_sr(emit, r5);

    /* now move 16 into TCNT0 */
    sh4_bin_mov_imm8_rn(emit, 0x10, r0);
    sh4_bin_movl_rm_a_disp4_rn(emit, r0, 12, r2);

    /* and move 16 into TCOR0 */
    sh4_bin_movl_rm_a_disp4_rn(emit, r0, 8, r2);

    /* and enable the underflow interrupt in TCR0 */
    sh4_bin_movw_a_disp4_rm_r0(emit, 16, r2);
    sh4_bin_or_imm8_r0(emit, 0x20);
    sh4_bin_movw_r0_a_disp4_rn(emit, 16, r2);

    /* and start the countdown by writing 1 into TSTR */
    sh4_bin_mov_imm8_rn(emit, 1, r0);
    sh4_bin_movb_r0_a_disp4_rn(emit, 4, r2);

    /* now loop */
    sh4_bin_dt_rn(emit, r3);
    sh4_bin_bf_offs8(emit, -2); /* branch back two bytes to DT again */

    /*
     * if we reach this point, it means the test has failed.
     * Move 0xff into R15 to signal failure and jump to the
     * exit point (0x0c000100)
     */
    sh4_bin_mov_imm8_rn(emit, 1, r15);

    /* now jump */
    sh4_bin_mov_imm8_rn(emit, 0x0c, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_shll8_rn(emit, r0);
    sh4_bin_mov_imm8_rn(emit, 1, r1);
    sh4_bin_shll8_rn(emit, r1);
    sh4_bin_or_rm_rn(emit, r1, r0);
    sh4_bin_jmp_arn(emit, r0);
    sh4_bin_nop(emit);
}

static void run_until(Sh4 *sh4, addr32_t addr) {
    while (sh4->reg[SH4_REG_PC] != addr)
        dc_single_step(sh4);
}

struct tmu_test_state {
    BiosFile bios;
    struct Memory mem;
    Sh4 sh4;
};

int main(int argc, char **argv) {

    compile();

    struct tmu_test_state test_state;
    int ret_code = 0;

    bios_file_init_empty(&test_state.bios);
    memory_init(&test_state.mem);
    memory_map_init(&test_state.bios, &test_state.mem);
    sh4_init(&test_state.sh4);

    bios_load_binary(&test_state.bios, 0);

    run_until(&test_state.sh4, 0x0c000100);

    ret_code = *sh4_gen_reg(&test_state.sh4, 15);

    if (ret_code) {
        fprintf(stderr, "Error: timer interrupt even failed to occur; "
                "test returned %d\n", ret_code);
        fprintf(stderr, "r3 is %d\n", (unsigned)*sh4_gen_reg(&test_state.sh4, 3));
    } else {
        printf("the remaining value in TCNT0 is %d\n",
               (int)test_state.sh4.reg[SH4_REG_TCNT0]);
        if (test_state.sh4.reg[SH4_REG_TCNT0] != 0x10) {
            fprintf(stderr, "remaining TCNT0 should have been inital value "
                    "(0x10)!\n");
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
        unsigned rem_loops = *sh4_gen_reg(&test_state.sh4, 3);
        printf("There were %u remaining iterations of the loop\n", rem_loops);
        if (rem_loops > 3) {
            fprintf(stderr, "Lower is always better, but I don't accept more "
                    "than 3 iterations\n");
            ret_code = 1;
        }

        if (((test_state.sh4.reg[SH4_REG_INTEVT] & SH4_INTEVT_CODE_MASK) >>
             SH4_INTEVT_CODE_SHIFT) != SH4_EXCP_TMU0_TUNI0) {
            fprintf(stderr, "bad intevt code value (interrupt reason is not "
                    "TUNI0)!\n");
            fprintf(stderr, "intevt value is %x\n",
                    (unsigned)test_state.sh4.reg[SH4_REG_INTEVT]);
            fprintf(stderr, "spc is 0x%08x\n",
                    (unsigned)test_state.sh4.reg[SH4_REG_SPC]);
            ret_code = 1;
        }
    }

    if (ret_code)
        printf("TEST FAILURE\n");
    else
        printf("TEST SUCCESS\n");

    bios_file_cleanup(&test_state.bios);
    sh4_cleanup(&test_state.sh4);

    return ret_code;
}
