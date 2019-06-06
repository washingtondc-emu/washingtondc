/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018. 2019 snickerbockers
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

#ifndef INTMATH_H_
#define INTMATH_H_

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

// Set all bits up to but not including bit_no:
#define SET_TO_BIT(bit_no)   ((uint32_t)((((uint64_t)1) << (bit_no)) - 1))

// set all bits between first and last (inclusive)
#define BIT_RANGE(first, last) (SET_TO_BIT(last + 1) & ~SET_TO_BIT(first))

static inline uint32_t add_flags(uint32_t lhs, uint32_t rhs, bool carry_in,
                                 bool *carry_out, bool *overflow_out) {
    uint64_t c_in = carry_in ? 1 : 0;

    // detect carry by doing 64-bit math
    uint64_t lhs64 = lhs;
    uint64_t rhs64 = rhs;
    uint64_t res64 = lhs64 + rhs64 + c_in;

    if (carry_out)
        *carry_out = (res64 & (((uint64_t)1) << 32)) ? true : false;

    if (overflow_out) {
        int32_t lhs_signed = lhs;
        int32_t rhs_signed = rhs;
        bool overflow_bit;
        overflow_bit =
            ((rhs_signed > 0) && (lhs_signed > INT_MAX - rhs_signed)) ||
            ((rhs_signed < 0) && (lhs_signed < INT_MIN - rhs_signed));
        if (!overflow_bit && c_in) {
            int32_t res_signed = rhs_signed + lhs_signed;
            overflow_bit = ((res_signed > 0) && (1 > INT_MAX - res_signed)) ||
                ((res_signed < 0) && (1 < INT_MIN - res_signed));
        }
        *overflow_out = overflow_bit;
    }

    return res64;
}

/*
 * XXX This function has confusing notation:
 * lhs and rhs refer to the left and right-hand sides of an SH4 asm instruction,
 * respectively.  This is the opposite of what would be considered the left and
 * right sides in standard mathematical notations.
 *
 * Thus, the expression this function implements is rhs - lhs.
 */
static inline int32_t sub_flags(int32_t lhs, int32_t rhs, bool carry_in,
                                bool *carry_out, bool *overflow_out) {
    uint64_t c_in = carry_in ? 1 : 0;

    // detect carry by doing 64-bit math
    uint64_t lhs64 = (uint32_t)lhs;
    uint64_t rhs64 = (uint32_t)rhs;
    uint64_t res64 = rhs64 - lhs64 - c_in;

    if (carry_out)
        *carry_out = (res64 & (((uint64_t)1) << 32)) ? true : false;

    if (overflow_out) {
        int64_t lhs64_signed = lhs;
        int64_t rhs64_signed = rhs;
        int64_t res64_signed = rhs64_signed - lhs64_signed;
        bool overflow_bit;
        overflow_bit = (res64_signed > INT32_MAX) || (res64_signed < INT32_MIN);
        if (!overflow_bit && c_in) {
            res64_signed++;
            overflow_bit = (res64_signed > INT32_MAX) ||
                (res64_signed < INT32_MIN);
        }
        *overflow_out = overflow_bit;
    }

    return res64;
}

// left-shift by n-bits and saturate to INT32_MAX or INT32_MIN if necessary
static inline int32_t sat_shift(int32_t in, unsigned n_bits) {
    // outbits includes all bits shifted out AND the sign-bit
    int32_t outbits = in >> (31 - n_bits);
    if (outbits == 0 || outbits == -1)
        return in << n_bits;
    if (in < 0)
        return INT32_MIN;
    return INT32_MAX;
}

#endif
