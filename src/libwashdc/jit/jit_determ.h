/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef JIT_DETERM_H_
#define JIT_DETERM_H_

#include "jit_il.h"

#ifndef JIT_OPTIMIZE
#error this file should not be built without -DJIT_OPTIMIZE
#endif

/*
 * The purpose of the determinism pass is to go through jit_il code and
 * determine which bits of which slots are known at compile-time.  Later passes
 * can then use this information to perform compile-time optimizations.
 *
 * We do this by allocating an array for each slot which tracks the known bits
 * of that slot and the values of those bits for every instruction in the block.
 */

struct jit_determ_slot {
    /*
     * known_bits is a bitmask which tracks the bits whose values are known at
     * compile-time.  known_val contains the values of those bits.
     *
     * For example, consider the following:
     *     A = B & C;
     * If the value of B is not known but the value of C is known, then I know
     * that any bit in A which corresponds to a 0-bit in C will itself be zero.
     * Ergo, A's known_bits would be ~C, and A's known_val would be 0.
     *
     * In a more general case, I might know some bits of B but not all of them,
     * and I might know some bits of C but not all of them.  In that case, this
     * expression would yield A's known bits for the above AND expression:
     *    A.known_bits = (~B) | (~C)
     *    A.known_val = 0
     *
     * The upshot of all this is that if the JIT knows what specific bits in a
     * slot are at compile-time, it can use that information to make
     * optimizations.  For example, if a bits 27:24 in a slot are 0xc, 0xd, 0xe
     * or 0xf and bit 28 in that slot is 0, then I know that slot contains a
     * valid pointer to system memory.  If that slot is used as an address for a
     * memory read or write, then I can bypass the memory map and read/write
     * directly to system memory.
     */
    uint32_t known_bits;
    uint32_t known_val;
};

/*
 * there is one of these for every instruction in the block
 */
struct jit_determ_state {
    struct jit_determ_slot slots[MAX_SLOTS];
};

struct il_code_block;

/*
 * initialize the given jit_determ_state to its default value.
 * This function is called by the code_block code whenever a new
 * jit_determ_state is created.  Unlike the rest of the code in jit_determ,
 * it is not called during the determination pass.
 */
void jit_determ_default(struct jit_determ_state *new_state);

/*
 * perform the determ pass on the given code_block and fill out its determinism
 * table.
 */
void jit_determ_pass(struct il_code_block *block);

#endif
