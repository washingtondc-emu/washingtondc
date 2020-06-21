/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
