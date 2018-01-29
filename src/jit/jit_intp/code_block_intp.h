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

#ifndef CODE_BLOCK_INTP_H_
#define CODE_BLOCK_INTP_H_

struct il_code_block;

/*
 * this is mostly identical to the il_code_block, but it's been prepared for
 * the interpreter
 */
struct code_block_intp {
    struct jit_inst *inst_list;
    unsigned cycle_count, inst_count;

    /*
     * number of JIT (NOT SH-4) registers.
     * The JIT_OP_LOAD_REG and JIT_OP_STORE_REG il instructions will handle
     * moving values between the sh4 registers and these il registers.
     */
    unsigned n_slots;
    uint32_t *slots;
};

void code_block_intp_init(struct code_block_intp *block);
void code_block_intp_cleanup(struct code_block_intp *block);

void code_block_intp_compile(struct code_block_intp *out,
                             struct il_code_block const *il_blk);

reg32_t code_block_intp_exec(struct code_block_intp const *block);

#endif
