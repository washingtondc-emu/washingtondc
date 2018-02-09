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

#ifndef CODE_BLOCK_H_
#define CODE_BLOCK_H_

#include "jit_il.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/code_block_x86_64.h"
#endif

#include "jit_intp/code_block_intp.h"

struct il_slot {
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

struct il_code_block {
    struct jit_inst *inst_list;
    unsigned inst_count;
    unsigned inst_alloc;
    unsigned cycle_count;
    unsigned last_inst_type;

    // this is a counter of how many slots the code block uses
    unsigned n_slots;

    struct il_slot *slots;
};

union jit_code_block {
#ifdef ENABLE_JIT_X86_64
    struct code_block_x86_64 x86_64;
#endif
    struct code_block_intp intp;
};

void il_code_block_init(struct il_code_block *block);
void il_code_block_cleanup(struct il_code_block *block);

void il_code_block_push_inst(struct il_code_block *block,
                              struct jit_inst const *inst);

/*
 * fill out block based on the SH4 basic-block which begins at guest-address
 * "addr".
 */
void il_code_block_compile(struct il_code_block *block, addr32_t addr);

void il_code_block_add_slot(struct il_code_block *block);

#endif
