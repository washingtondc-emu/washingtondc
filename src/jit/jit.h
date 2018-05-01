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

#ifndef JIT_H_
#define JIT_H_

#include <stdint.h>

#include "hw/sh4/sh4.h"

void jit_init(void);
void jit_cleanup(void);

static inline void
jit_compile_native(Sh4 *sh4, struct code_block_x86_64 *blk, uint32_t pc) {
    struct il_code_block il_blk;
    il_code_block_init(&il_blk);
    il_code_block_compile(sh4, &il_blk, pc);
#ifdef JIT_OPTIMIZE
    jit_determ_pass(&il_blk);
#endif
    code_block_x86_64_compile(blk, &il_blk);
    il_code_block_cleanup(&il_blk);
}

static inline void
jit_compile_intp(Sh4 *sh4, struct code_block_intp *blk, uint32_t pc) {
    struct il_code_block il_blk;
    il_code_block_init(&il_blk);
    il_code_block_compile(sh4, &il_blk, pc);
#ifdef JIT_OPTIMIZE
    jit_determ_pass(&il_blk);
#endif
    code_block_intp_compile(blk, &il_blk);
    il_code_block_cleanup(&il_blk);
}

#endif
