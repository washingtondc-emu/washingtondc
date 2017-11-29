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

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "BiosFile.h"
#include "memory.h"
#include "sh4asm_neo/sh4asm_neo.h"
#include "MemoryMap.h"

#define INST_MAX 256
uint16_t inst_list[INST_MAX];
unsigned inst_count;

const static unsigned N_TEST_ITERATIONS = 2048;

struct div_test_state {
    BiosFile bios;
    struct Memory mem;
    Sh4 sh4;
};

struct div_test;

static unsigned pick_rand32(void);
static unsigned pick_rand16(void);
static void emit(uint16_t inst);
static void run_until(Sh4 *sh4, addr32_t addr);
static void bios_load_binary(BiosFile *bios, addr32_t where);
static int
run_div_test(addr32_t stop_addr, struct div_test_state *state,
             char const *test_name, reg32_t dividend, reg32_t divisor,
             reg32_t quotient);
static int
unsigned_div_test_32_16(struct div_test *test, struct div_test_state *state);
static int
signed_div_test_16_16(struct div_test *test, struct div_test_state *state);
static int
signed_div_test_32_32(struct div_test *test, struct div_test_state *state);
static int
unsigned_div_test_64_32(struct div_test *test, struct div_test_state *state);

static void clear_program(void);

typedef int(*div_test_func_t)(struct div_test*, struct div_test_state*);

struct div_test {
    char const *test_name;
    div_test_func_t test_func;
} div_tests[] = {
    { "32-by-16 unsigned integer division", unsigned_div_test_32_16 },
    { "16-by-16 signed integer division", signed_div_test_16_16 },
    { "32-by-32 signed integer division", signed_div_test_32_32 },
    { "64-by-32 unsigned integer division", unsigned_div_test_64_32 },
    { NULL, NULL }
};

static unsigned pick_rand32(void) {
    return (uint32_t)rand();
}

static unsigned pick_rand16(void) {
    return (uint16_t)rand();
}

static void emit(uint16_t inst) {
    if (inst_count < INST_MAX)
        inst_list[inst_count++] = inst;
}

static void clear_program(void) {
    inst_count = 0;
}

static void run_until(Sh4 *sh4, addr32_t addr) {
    while (sh4->reg[SH4_REG_PC] != addr)
        dc_single_step(sh4);
}

/*
 * loads a program into the given address.  the InputIterator's
 * indirect method (overload*) should return a data_tp.
 */
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

static int
run_div_test(addr32_t stop_addr, struct div_test_state *state,
             char const *test_name, reg32_t dividend, reg32_t divisor,
             reg32_t quotient) {
    bios_load_binary(&state->bios, 0);

    sh4_on_hard_reset(&state->sh4);

    *sh4_gen_reg(&state->sh4, 1) = divisor;
    *sh4_gen_reg(&state->sh4, 2) = dividend;
    run_until(&state->sh4, stop_addr);

    reg32_t quotient_actual;
    quotient_actual = *sh4_gen_reg(&state->sh4, 2);

    if (quotient != quotient_actual) {
        printf("FAILURE while Running integer division test \"%s\"\n",
               test_name);
        printf("input operation was %x / %x\n",
               (unsigned)dividend, (unsigned)divisor);
        printf("expected result was %u\n", (unsigned)quotient);
        printf("actual result was %u\n", (unsigned)quotient_actual);

        return 1;
    } else {
        return 0;
    }
}

static int
unsigned_div_test_32_16(struct div_test *test, struct div_test_state *state) {
    /*
     * pick a random 32-bit dividend and a random 16-bit divisor,
     * being careful to ensure that there is no overflow
     */
    uint32_t dividend, divisor, quotient;

    static char const *prog_asm =
        "shll16 r1\n"
        "mov #16, r0\n"
        "div0u\n"

        /*
         * looping is untenable here because we don't want to touch the T flag
         * it *is* possible to save/restore the T flag on every iteration, but
         * it's easier to just copy/paste the same instruction 16 times.
         */
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"

        "rotcl r2\n"
        "extu.w r2, r2\n";
    // final address should be 0x2a

    sh4asm_neo_input_string(prog_asm);

    do {
        dividend = pick_rand32();
        divisor = pick_rand16();
    } while ((!divisor) || (dividend >= (divisor << 16)));

    quotient = dividend / divisor;

    return run_div_test(0xa000002a, state, test->test_name,
                        dividend, divisor, quotient);
}

static int
signed_div_test_16_16(struct div_test *test, struct div_test_state *state) {

    static char const *prog_asm =
        "shll16 r1\n"
        "exts.w r2, r2\n"
        "xor r0, r0\n"
        "mov r2, r3\n"
        "rotcl r3\n"
        "subc r0, r2\n"

        "div0s r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"
        "div1 r1, r2\n"

        "exts.w r2, r2\n"
        "rotcl r2\n"
        "addc r0, r2\n"
        "exts.w r2, r2\n";
    // exit at pc=0x34

    sh4asm_neo_input_string(prog_asm);

    /*
     * pick random 16-bit signed integers.
     * this is less complicated than it looks.
     */
    uint32_t dividend, divisor, quotient;

    do {
        dividend = pick_rand32();
        divisor = pick_rand32();

        uint32_t dividend_sign = dividend & 0x8000;
        if (dividend_sign)
            dividend |= ~0xffff;
        else
            dividend &= 0xffff;
        uint32_t divisor_sign = divisor & 0x8000;
        if (divisor_sign)
            divisor |= ~0xffff;
        else
            divisor &= 0xffff;
    } while (!divisor);


    quotient = (int32_t)dividend / (int32_t)divisor;

    return run_div_test(0xa0000034, state, test->test_name,
                        dividend, divisor, quotient);
}

static int
signed_div_test_32_32(struct div_test *test, struct div_test_state *state) {
    int32_t dividend, divisor, quotient;

    static char const *prog_asm =
        "mov r2, r3\n"
        "rotcl r3\n"
        "subc r0, r0\n"
        "xor r3, r3\n"
        "subc r3, r2\n"

        // at this point the dividend is in one's-complement
        "div0s r1, r0\n"

        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"
        "rotcl r2\n"
        "div1 r1, r0\n"

        "rotcl r2\n"
        "addc r3, r2\n";
    // should end at PC=0x90

    sh4asm_neo_input_string(prog_asm);

    do {
        dividend = pick_rand32();
        divisor = pick_rand32();
    } while (!divisor);

    quotient = dividend / divisor;
    return run_div_test(0xa0000090, state, test->test_name,
                        dividend, divisor, quotient);
}

static int
unsigned_div_test_64_32(struct div_test *test, struct div_test_state *state) {
    uint32_t dividend_high, dividend_low, divisor, quotient;
    uint64_t dividend64;

    /*
     * This test doesn't follow the same format as the other three.
     *
     * It expects the dividend to be a 64-bit int with the upper 4 bytes in R1,
     * and the lower 4 bytes in R2.  The divisor goes in R3.  The quotient will be
     * left in R2.
     */
    static char const *prog_asm =
        "div0u\n"

        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"
        "rotcl r2\n"
        "div1 r3, r1\n"

        "rotcl r2\n";

    sh4asm_neo_input_string(prog_asm);

    do {
        dividend_high = pick_rand32();
        dividend_low = pick_rand32();
        divisor = pick_rand32();
    } while ((!divisor) || (dividend_high >= divisor));

    /*
     * TODO: This will break on big-endian systems
     *
     * Although in general I probably have a lot of code that won't work on
     * bit endian systems because I never bothered to take that into account.
     */
    memcpy(&dividend64, &dividend_low, sizeof(dividend_low));
    memcpy(((uint32_t*)&dividend64) + 1, &dividend_high, sizeof(dividend_high));

    quotient = dividend64 / divisor;

    /*
     * we can't use run_div_test for this test case because it has a slightly
     * different format compared to the other three test cases.
     */
    bios_load_binary(&state->bios, 0);

    sh4_on_hard_reset(&state->sh4);

    *sh4_gen_reg(&state->sh4, 1) = dividend_high;
    *sh4_gen_reg(&state->sh4, 2) = dividend_low;
    *sh4_gen_reg(&state->sh4, 3) = divisor;
    run_until(&state->sh4, 0xa0000084);

    reg32_t quotient_actual;
    quotient_actual = *sh4_gen_reg(&state->sh4, 2);

    if (quotient != quotient_actual) {
        printf("FAILURE while running integer division test \"%s\"\n",
               test->test_name);
        printf("input operation was %llx / %x\n",
               (long long)dividend64, (unsigned)divisor);
        printf("expected result was %u\n", (unsigned)quotient);
        printf("actual result was %u\n", (unsigned)quotient_actual);

        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char **argv) {
    int n_tests = 0;
    int n_success = 0;
    unsigned seed = time(NULL);
    int opt;

    while ((opt = getopt(argc, argv, "s:")) > 0) {
        if (opt == 's')
            seed = atoi(optarg);
    }

    srand(seed);

    struct div_test_state test_state;
    bios_file_init_empty(&test_state.bios);

    memory_init(&test_state.mem);
    memory_map_init(&test_state.bios, &test_state.mem);
    sh4_init(&test_state.sh4);

    sh4asm_neo_set_emitter(emit);

    unsigned iteration;
    for (iteration = 0; iteration < N_TEST_ITERATIONS; iteration++) {
        struct div_test *curs = div_tests;

        while (curs->test_name) {
            clear_program();
            if (curs->test_func(curs, &test_state) == 0)
                n_success++;

            n_tests++;
            curs++;
        }
    }

    sh4_cleanup(&test_state.sh4);

    printf("%d tests run -- %d successes.\n", n_tests, n_success);

    bios_file_cleanup(&test_state.bios);

    return (n_tests == n_success) ? 0 : 1;
}
