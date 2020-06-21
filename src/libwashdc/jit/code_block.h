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

#ifndef CODE_BLOCK_H_
#define CODE_BLOCK_H_

#include <stddef.h>
#include <stdbool.h>

#include "jit_il.h"
#include "washdc/error.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/code_block_x86_64.h"
#endif

#ifdef JIT_PROFILE
#include "jit_profile.h"
#endif

#include "jit_intp/code_block_intp.h"

enum washdc_jit_slot_tp {
    // general-purpose slot
    WASHDC_JIT_SLOT_GEN,

    // floating-point slot
    WASHDC_JIT_SLOT_FLOAT,

    // pointer to something on the host CPU
    WASHDC_JIT_SLOT_HOST_PTR
};

unsigned alloc_slot(struct il_code_block *block, enum washdc_jit_slot_tp tp);
void free_slot(struct il_code_block *block, unsigned slot_no);

struct il_slot {
    enum washdc_jit_slot_tp tp;
};

struct il_code_block {
    struct jit_inst *inst_list;
    unsigned inst_count;
    unsigned inst_alloc;

    // this is a counter of how many slots the code block uses
    unsigned n_slots;

    struct il_slot slots[MAX_SLOTS];

#ifdef JIT_PROFILE
    struct jit_profile_per_block *profile;
#endif
};

static inline void
check_slot(struct il_code_block *block, unsigned slot_no,
           enum washdc_jit_slot_tp tp) {
    if (slot_no >= block->n_slots)
        RAISE_ERROR(ERROR_INTEGRITY);
    if (block->slots[slot_no].tp != tp)
        RAISE_ERROR(ERROR_INTEGRITY);
}

struct jit_code_block {
    union {
#ifdef ENABLE_JIT_X86_64
        struct code_block_x86_64 x86_64;
#endif
        struct code_block_intp intp;
    };

#ifdef JIT_PROFILE
    struct jit_profile_per_block *profile;
#endif
};

void il_code_block_init(struct il_code_block *block);
void il_code_block_cleanup(struct il_code_block *block);

void il_code_block_push_inst(struct il_code_block *block,
                              struct jit_inst const *inst);
void il_code_block_strike_inst(struct il_code_block *blk, unsigned inst_idx);
void il_code_block_insert_inst(struct il_code_block *blk,
                               struct jit_inst const *inst, unsigned idx);

static inline void
jit_code_block_init(struct jit_code_block *blk, uint32_t addr_first,
                    bool native_mode) {
#ifdef ENABLE_JIT_X86_64
    if (native_mode)
        code_block_x86_64_init(&blk->x86_64);
    else
#endif
        code_block_intp_init(&blk->intp);

#ifdef JIT_PROFILE
    blk->profile = jit_profile_create_block(addr_first);
#endif
}

static inline void
jit_code_block_cleanup(struct jit_code_block *blk, bool native_mode) {
#ifdef JIT_PROFILE
    jit_profile_free_block(blk->profile);
#endif

#ifdef ENABLE_JIT_X86_64
    if (native_mode)
        code_block_x86_64_cleanup(&blk->x86_64);
    else
#endif
        code_block_intp_cleanup(&blk->intp);
}

/*
 * starting from base, return the index of the last slot instruction which
 * references a given slot.
 */
unsigned
jit_code_block_slot_lifespan(struct il_code_block const *blk,
                             unsigned slot_no, unsigned base);

#endif
