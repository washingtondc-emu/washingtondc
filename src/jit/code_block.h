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

struct il_code_block {
    struct jit_inst *inst_list;
    unsigned inst_count;
    unsigned inst_alloc;
    unsigned cycle_count;
    unsigned last_inst_type;
};

void il_code_block_init(struct il_code_block *block);
void il_code_block_cleanup(struct il_code_block *block);

// restore a previously-init'd il_code_block to its initial (post-init) state.
void clode_block_clear(struct il_code_block *block);

void il_code_block_exec(struct il_code_block const *block);

void il_code_block_push_inst(struct il_code_block *block,
                              struct jit_inst const *inst);

void il_code_block_compile(struct il_code_block *block, addr32_t addr);

#endif
