/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <stdbool.h>

#include "jit_il.h"

#ifdef JIT_OPTIMIZE
#include "jit_determ.h"
#endif

#ifdef ENABLE_JIT_X86_64
#include "x86_64/code_block_x86_64.h"
#endif

#include "jit_intp/code_block_intp.h"

// this tells whether a given slot is in use
bool slot_status(struct il_code_block *block, unsigned slot_no);

unsigned alloc_slot(struct il_code_block *block);
void free_slot(struct il_code_block *block, unsigned slot_no);

struct il_slot {
    bool in_use;
};

struct il_code_block {
    struct jit_inst *inst_list;
    unsigned inst_count;
    unsigned inst_alloc;
    unsigned cycle_count;

    // this is a counter of how many slots the code block uses
    unsigned n_slots;

    struct il_slot slots[MAX_SLOTS];

#ifdef JIT_OPTIMIZE
    // The length of this array is inst_count, but only if it is non-NULL
    struct jit_determ_state *determ;
#endif
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

struct Sh4;

/*
 * fill out block based on the SH4 basic-block which begins at guest-address
 * "addr".
 */
void
il_code_block_compile(struct Sh4 *sh4,
                      struct il_code_block *block, addr32_t addr);

#endif
