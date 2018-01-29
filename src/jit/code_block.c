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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "sh4_inst.h"
#include "code_block.h"
#include "dreamcast.h"
#include "hw/sh4/sh4_reg.h"
#include "hw/sh4/sh4_disas.h"

#define DEFAULT_BLOCK_LEN 32
#define BLOCK_GROW_LEN 1

void il_code_block_init(struct il_code_block *block) {
    memset(block, 0, sizeof(*block));
    block->inst_count = 0;
    block->inst_alloc = DEFAULT_BLOCK_LEN;
    block->inst_list = (struct jit_inst*)malloc(DEFAULT_BLOCK_LEN *
                                                sizeof(struct jit_inst));
    block->last_inst_type = SH4_GROUP_NONE;
}

void il_code_block_cleanup(struct il_code_block *block) {
    free(block->inst_list);
    memset(block, 0, sizeof(*block));
}

void il_code_block_push_inst(struct il_code_block *block,
                              struct jit_inst const *inst) {
    if (block->inst_count >= block->inst_alloc) {
        unsigned new_alloc = block->inst_alloc + BLOCK_GROW_LEN;
        struct jit_inst *new_list =
            (struct jit_inst*)realloc(block->inst_list,
                                      new_alloc * sizeof(struct jit_inst));
        if (!new_list)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        block->inst_list = new_list;
        block->inst_alloc = new_alloc;
    }

    block->inst_list[block->inst_count++] = *inst;
}

void il_code_block_compile(struct il_code_block *block, addr32_t addr) {
    bool do_continue;

    sh4_disas_new_block();

    do {
        do_continue = sh4_disas_inst(block, addr);
        addr += 2;
    } while (do_continue);
}
