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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "washdc/error.h"
#include "code_block.h"

#include "jit_profile.h"

#ifndef JIT_PROFILE
#error this file should only be built for -DJIT_PROFILE=On builds
#endif

void jit_profile_ctxt_init(struct jit_profile_ctxt *ctxt, unsigned bytes_per_inst) {
    memset(ctxt, 0, sizeof(*ctxt));
    ctxt->bytes_per_inst = bytes_per_inst;
}

void jit_profile_ctxt_cleanup(struct jit_profile_ctxt *ctxt) {
    unsigned idx;
    for (idx = 0; idx < JIT_PROFILE_N_BLOCKS; idx++) {
        if (ctxt->high_score[idx])
            jit_profile_free_block(ctxt->high_score[idx]);
    }
}

void jit_profile_notify(struct jit_profile_ctxt *ctxt,
                        struct jit_profile_per_block *blk) {
    jit_profile_freq hit_count = ++blk->hit_count;

    int idx;
    struct jit_profile_per_block **high_score = ctxt->high_score;

    for (idx = 0; idx < JIT_PROFILE_N_BLOCKS; idx++)
        if (high_score[idx] == blk) {
            int idx2;
            for (idx2 = idx - 1; idx2 >= 0; idx2--) {
                if (high_score[idx2]->hit_count <= hit_count) {
                    struct jit_profile_per_block *tmp = high_score[idx2];
                    high_score[idx2] = high_score[idx];
                    high_score[idx] = tmp;
                    idx = idx2;
                } else {
                    break;
                }
            }
            return;
        }

    for (idx = 0; idx < JIT_PROFILE_N_BLOCKS; idx++) {
        if (!high_score[idx] ||
            hit_count >= high_score[idx]->hit_count) {

            /*
             * shift the score at idx and all scores beneath it down by 1.  This
             * also necessitates releasing the reference to the high_score at
             * JIT_PROFILE_N_BLOCKS-1
             */
            if (high_score[JIT_PROFILE_N_BLOCKS - 1])
                jit_profile_free_block(high_score[JIT_PROFILE_N_BLOCKS - 1]);
            if (idx < (JIT_PROFILE_N_BLOCKS - 1)) {
                memmove(high_score + idx + 1, high_score + idx,
                        sizeof(struct jit_profile_per_block*) *
                        (JIT_PROFILE_N_BLOCKS - 1 - idx));
            }

            high_score[idx] = blk;
            blk->refcount++;
            break;
        }
    }
}

struct jit_profile_per_block *jit_profile_create_block(uint32_t addr_first) {
    struct jit_profile_per_block *blk = (struct jit_profile_per_block*)
        calloc(1, sizeof(struct jit_profile_per_block));

    blk->first_addr = addr_first;
    blk->refcount = 1;

    return blk;
}

void jit_profile_free_block(struct jit_profile_per_block *blk) {
    if (--blk->refcount == 0) {
        free(blk->instructions);
        free(blk);
    }
}

void jit_profile_push_inst(struct jit_profile_ctxt *ctxt,
                           struct jit_profile_per_block *blk,
                           void *inst) {
    void *newinst = realloc(blk->instructions,
                            ++blk->inst_count * ctxt->bytes_per_inst);
    if (!newinst)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    memcpy(newinst + (blk->inst_count - 1) * ctxt->bytes_per_inst,
           inst, ctxt->bytes_per_inst);

    blk->instructions = newinst;
}

void jit_profile_print(struct jit_profile_ctxt *ctxt, FILE *fout) {
    unsigned n_blocks = 0;
    unsigned idx;
    for (idx = 0; idx < JIT_PROFILE_N_BLOCKS; idx++)
        if (ctxt->high_score[idx])
            n_blocks++;
    fprintf(fout, "showing the top %u code-blocks\n", n_blocks);

    unsigned rank = 0;
    for (idx = 0; idx < JIT_PROFILE_N_BLOCKS; idx++) {
        struct jit_profile_per_block *profile = ctxt->high_score[idx];
        if (!profile)
            continue;
        fprintf(fout, "rank %u\n", ++rank);
        fprintf(fout, "\taddress: 0x%08x\n", (unsigned)profile->first_addr);
        fprintf(fout, "\tinstruction count: %u\n", profile->inst_count);
        fprintf(fout, "\taccess count: %llu\n",
                (unsigned long long)profile->hit_count);
        fputs("\n", fout);

        if (ctxt->disas) {
            fputs("Disassembly:\n", fout);
            unsigned inst_no;
            unsigned bytes_per_inst = ctxt->bytes_per_inst;
            for (inst_no = 0; inst_no < profile->inst_count; inst_no++) {
                size_t byte_offs = inst_no * bytes_per_inst;
                uint32_t addr = profile->first_addr + byte_offs;
                fprintf(fout, "\t0x%08x: ", (unsigned)addr);
                ctxt->disas(fout, addr,
                            ((uint8_t*)profile->instructions) + byte_offs);
                fputs("\n", fout);
            }
            fputs("\n", fout);
        }
    }
}
