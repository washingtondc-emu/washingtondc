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

#ifndef CODE_BLOCK_X86_64_H_
#define CODE_BLOCK_X86_64_H_

#include <stdint.h>
#include <stdbool.h>

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

struct il_code_block;
struct native_dispatch_meta;

struct code_block_x86_64 {
    /*
     * native points to the function where this code block is implemented.
     * exec_mem_alloc_start points to the beginning of the actual allocation
     * (which might include local variables or other non-executable data)
     */
    void *native; // void(*native)(void);
    void *exec_mem_alloc_start;

    uint32_t cycle_count;
    unsigned bytes_used;

    bool dirty_stack;
};

void jit_x86_64_backend_init(void);
void jit_x86_64_backend_cleanup(void);

void code_block_x86_64_init(struct code_block_x86_64 *blk);
void code_block_x86_64_cleanup(struct code_block_x86_64 *blk);

void code_block_x86_64_compile(void *cpu, struct code_block_x86_64 *out,
                               struct il_code_block const *il_blk,
                               struct native_dispatch_meta const *dispatch_meta,
                               unsigned cycle_count);

/*
 * if the stack is not 16-byte aligned, make it 16-byte aligned.
 * This way, when the CALL instruction is issued the stack will be off from
 * 16-byte alignment by 8 bytes; this is what GCC's calling convention requires.
 */
void x86_64_align_stack(struct code_block_x86_64 *blk);

/*
 * Microsoft's ABI requires 32 bytes to be allocated on the stack when calling
 * a function.
 */
void ms_shadow_open(struct code_block_x86_64 *blk);
void ms_shadow_close(void);

#endif
