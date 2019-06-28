/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#ifndef WASHDC_JIT_PROFILE_H_
#define WASHDC_JIT_PROFILE_H_

#include <stdio.h>
#include <stdint.h>

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

    unsigned refcount;
};

typedef void(*jit_profile_disas_fn)(FILE *out, uint32_t addr, void const *instp);

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

// called when the jit pushes a new instruction onto a block it's compiling.
void jit_profile_push_inst(struct jit_profile_ctxt *ctxt,
                           struct jit_profile_per_block *blk,
                           void *inst);

void jit_profile_print(struct jit_profile_ctxt *ctxt, FILE *fout);

#endif
