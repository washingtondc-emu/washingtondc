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

#include <limits>
#include <string>
#include <iostream>

#include "hw/sh4/sh4.hpp"
#include "hw/sh4/sh4_excp.hpp"
#include "tool/sh4asm/sh4asm.hpp"
#include "BaseException.hpp"
#include "memory.h"
#include "RandGenerator.hpp"
#include "BiosFile.h"
#include "MemoryMap.hpp"

typedef RandGenerator<uint32_t> RandGen32;

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

/*
 * sh4 program for unsigned division of a 32-bit dividend by a 16-bit divisor
 *
 * this gets loaded in at 0x00000000
 *
 * divisor should be placed in R1, dividend should be placed in R2.
 * This does not check for overflow or division by zero
 */
char const div_unsigned_32_16_asm[] =
    "SHLL16 R1\n"
    "MOV #16, R0\n"
    "DIV0U\n"

    /*
     * looping is untenable here because we don't want to touch the T flag
     * it *is* possible to save/restore the T flag on every iteration, but
     * it's easier to just copy/paste the same instruction 16 times.
     */
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"

    "ROTCL R2\n"
    "EXTU.W R2, R2\n";
// final address should be 0x2a

char const div_signed_16_16_asm[] =
    "SHLL16 R1\n"
    "EXTS.W R2, R2\n"
    "XOR R0, R0\n"
    "MOV R2, R3\n"
    "ROTCL R3\n"
    "SUBC R0, R2\n"

    "DIV0S R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"
    "DIV1 R1, R2\n"

    "EXTS.W R2, R2\n"
    "ROTCL R2\n"
    "ADDC R0, R2\n"
    "EXTS.W R2, R2\n";
// exit at pc=0x34


char const div_signed_32_32_asm[] =
    /*
     * R1 is the divisor, R2 is the lower 32-bits of the dividend and
     * R0 is the upper 32-bits of the dividend.
     */
    "MOV R2, R3\n"
    "ROTCL R3\n"
    "SUBC R0, R0\n"
    "XOR R3, R3\n"
    "SUBC R3, R2\n"

    // at this point the dividend is in one's-complement
    "DIV0S R1, R0\n"

    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"
    "ROTCL R2\n"
    "DIV1 R1, R0\n"

    "ROTCL R2\n"
    "ADDC R3, R2\n";
// should end at PC=0x90

/*
 * This test doesn't follow the same format as the other three.
 *
 * It expects the dividend to be a 64-bit int with the upper 4 bytes in R1,
 * and the lower 4 bytes in R2.  The divisor goes in R3.  The quotient will be
 * left in R2.
 */
char const div_unsigned_64_32[] =
    "DIV0U\n"

    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"
    "ROTCL R2\n"
    "DIV1 R3, R1\n"

    "ROTCL R2\n";

struct div_test_state {
    BiosFile bios;
    struct Memory mem;
    Sh4 sh4;

    RandGen32 randgen32;
};

struct div_test;

static int
run_div_test(addr32_t run_until, struct div_test_state *state,
             char const *test_name, char const *prog_asm,
             reg32_t dividend, reg32_t divisor, reg32_t quotient);
static int
unsigned_div_test_32_16(struct div_test *test, struct div_test_state *state);
static int
signed_div_test_16_16(struct div_test *test, struct div_test_state *state);
static int
signed_div_test_32_32(struct div_test *test, struct div_test_state *state);
static int
unsigned_div_test_64_32(struct div_test *test, struct div_test_state *state);

const static unsigned N_TEST_ITERATIONS = 2048;
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

static int
run_div_test(addr32_t run_until, struct div_test_state *state,
             char const *test_name, char const *prog_asm,
             reg32_t dividend, reg32_t divisor, reg32_t quotient) {
    Sh4Prog test_prog;

    test_prog.add_txt(std::string(prog_asm));
    const Sh4Prog::ByteList& inst = test_prog.get_prog();
    bios_load_binary(&state->bios, 0, inst.begin(), inst.end());

    sh4_on_hard_reset(&state->sh4);
    sh4_enter(&state->sh4);

    *sh4_gen_reg(&state->sh4, 1) = divisor;
    *sh4_gen_reg(&state->sh4, 2) = dividend;
    sh4_run_until(&state->sh4, run_until);

    reg32_t quotient_actual;
    quotient_actual = *sh4_gen_reg(&state->sh4, 2);

    if (quotient != quotient_actual) {
        std::cout << "FAILURE while " << "Running integer division test \"" <<
            test_name << "\"" << std::endl;
        std::cout << "input operation was " << std::hex << dividend << " / " <<
            divisor << std::endl;
        std::cout << "expected result was " << quotient << std::endl;
        std::cout << "actual result was " << quotient_actual << std::endl;

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

    do {
        dividend = state->randgen32.pick_val(0);
        divisor = state->randgen32.pick_val(0) & 0xffff;
    } while ((!divisor) || (dividend >= (divisor << 16)));

    quotient = dividend / divisor;
    return run_div_test(0xa000002a, state, test->test_name,
                        div_unsigned_32_16_asm, dividend, divisor, quotient);
}

static int
signed_div_test_16_16(struct div_test *test, struct div_test_state *state) {
    /*
     * pick random 16-bit signed integers.
     * this is less complicated than it looks.
     */
    uint32_t dividend, divisor, quotient;

    do {
        dividend = state->randgen32.pick_val(0);
        divisor = state->randgen32.pick_val(0);

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

    quotient = int32_t(dividend) / int32_t(divisor);
    return run_div_test(0xa0000034, state, test->test_name,
                        div_signed_16_16_asm, dividend, divisor, quotient);
}

static int
signed_div_test_32_32(struct div_test *test, struct div_test_state *state) {
    int32_t dividend, divisor, quotient;

    do {
        dividend = state->randgen32.pick_val(0);
        divisor = state->randgen32.pick_val(0);
    } while (!divisor);

    quotient = dividend / divisor;
    return run_div_test(0xa0000090, state, test->test_name,
                        div_signed_32_32_asm, dividend, divisor, quotient);
}

static int
unsigned_div_test_64_32(struct div_test *test, struct div_test_state *state) {
    uint32_t dividend_high, dividend_low, divisor, quotient;
    uint64_t dividend64;

    do {
        dividend_high = state->randgen32.pick_val(0);
        dividend_low = state->randgen32.pick_val(0);
        divisor = state->randgen32.pick_val(0);
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
    Sh4Prog test_prog;

    test_prog.add_txt(std::string(div_unsigned_64_32));
    const Sh4Prog::ByteList& inst = test_prog.get_prog();
    bios_load_binary(&state->bios, 0, inst.begin(), inst.end());

    sh4_on_hard_reset(&state->sh4);
    sh4_enter(&state->sh4);

    *sh4_gen_reg(&state->sh4, 1) = dividend_high;
    *sh4_gen_reg(&state->sh4, 2) = dividend_low;
    *sh4_gen_reg(&state->sh4, 3) = divisor;
    sh4_run_until(&state->sh4, 0xa0000084);

    reg32_t quotient_actual;
    quotient_actual = *sh4_gen_reg(&state->sh4, 2);

    if (quotient != quotient_actual) {
        std::cout << "FAILURE while running integer division test \"" <<
            test->test_name << "\"" << std::endl;
        std::cout << "input operation was " << std::hex << dividend64
                  << " / " << divisor << std::endl;
        std::cout << "expected result was " << quotient << std::endl;
        std::cout << "actual result was " << quotient_actual << std::endl;

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

    struct div_test_state test_state;
    bios_file_init_empty(&test_state.bios);

    try {
        memory_init(&test_state.mem, 16 * 1024 * 1024);
        memory_map_init(&test_state.bios, &test_state.mem);
        sh4_init(&test_state.sh4);

        test_state.randgen32 = RandGen32(seed);
        test_state.randgen32.reset();

        unsigned iteration;
        for (iteration = 0; iteration < N_TEST_ITERATIONS; iteration++) {
            struct div_test *curs = div_tests;

            while (curs->test_name) {
                if (curs->test_func(curs, &test_state) == 0)
                    n_success++;

                n_tests++;
                curs++;
            }
        }

        sh4_cleanup(&test_state.sh4);
    } catch (BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
    }

    std::cout << std::dec << n_tests << " run -- " << n_success <<
        " successes." << std::endl;

    bios_file_cleanup(&test_state.bios);

    return (n_tests == n_success) ? 0 : 1;
}
