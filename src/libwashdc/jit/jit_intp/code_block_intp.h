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

#ifndef CODE_BLOCK_INTP_H_
#define CODE_BLOCK_INTP_H_

struct il_code_block;

union slot_val {
    uint32_t as_u32;
    float as_float;
    void *as_host_ptr;
};

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
    union slot_val *slots;
};

void code_block_intp_init(struct code_block_intp *block);
void code_block_intp_cleanup(struct code_block_intp *block);

void code_block_intp_compile(void *cpu,
                             struct code_block_intp *out,
                             struct il_code_block const *il_blk,
                             unsigned cycle_count);

reg32_t code_block_intp_exec(void *cpu, struct code_block_intp const *block);

#endif
