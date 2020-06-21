/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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

#ifndef WASHDC_JIT_PROFILE_H_
#define WASHDC_JIT_PROFILE_H_

#include <stdint.h>

#include "jit_il.h"
#include "washdc/hostfile.h"

#ifndef JIT_PROFILE
#error this file should only be included in -DJIT_PROFILE=On builds
#endif

struct jit_code_block;

/*
 * the profiler tracks the top N blocks (where N is JIT_PROFILE_N_BLOCKS) based
 * on how many times each block is jumped to.  Because execution time is not
 * taken into account, this does not necessarily show us which blocks are
 * bottlenecks.  It does give an indication of what the most common code-paths
 * are in a given game so that those paths can be optimized.
 */
#define JIT_PROFILE_N_BLOCKS 128

typedef unsigned long long jit_profile_freq;

struct jit_profile_per_block {
    jit_profile_freq hit_count;

    uint32_t first_addr;
    unsigned inst_count;
    void *instructions;

    unsigned il_inst_count;
    struct jit_inst *il_insts;

    unsigned refcount;

    unsigned native_bytes;
    void *native_dat;
};

typedef void(*jit_profile_disas_fn)(washdc_hostfile out, uint32_t addr, void const *instp);

struct jit_profile_ctxt {
    /*
     * list of the top N blocks in terms of hit_count.  The highest is at index
     * 0, lowest is at JIT_PROFILE_N_BLOCKS-1.
     */
    struct jit_profile_per_block *high_score[JIT_PROFILE_N_BLOCKS];
    unsigned bytes_per_inst;

    jit_profile_disas_fn disas;
};

void jit_profile_ctxt_init(struct jit_profile_ctxt *ctxt,
                           unsigned bytes_per_inst);
void jit_profile_ctxt_cleanup(struct jit_profile_ctxt *ctxt);

/*
 * The JIT calls this function to notify the profiler that it is jumping to a
 * code block.
 */
void jit_profile_notify(struct jit_profile_ctxt *ctxt,
                        struct jit_profile_per_block *blk);

// allocate a profile information for a new code block.
struct jit_profile_per_block *jit_profile_create_block(uint32_t addr_first);

// release a reference to a code block
void jit_profile_free_block(struct jit_profile_per_block *blk);

// called when the jit pushes a new CPU instruction onto a block it's compiling.
void jit_profile_push_inst(struct jit_profile_ctxt *ctxt,
                           struct jit_profile_per_block *blk,
                           void *inst);

// called when the jit pushes a new IL instruction onto a block it's compiling.
void jit_profile_push_il_inst(struct jit_profile_ctxt *ctxt,
                              struct jit_profile_per_block *blk,
                              struct jit_inst const *inst);

void jit_profile_print(struct jit_profile_ctxt *ctxt, washdc_hostfile fout);

void jit_profile_set_native_insts(struct jit_profile_ctxt *ctxt,
                                  struct jit_profile_per_block *blk,
                                  unsigned n_bytes,
                                  void const *dat);
#endif
