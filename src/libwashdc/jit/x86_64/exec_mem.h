/*******************************************************************************
 *
 * Copyright 2018 snickerbockers
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

#ifndef EXEC_MEM_H_
#define EXEC_MEM_H_

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <stddef.h>

void exec_mem_init(void);
void exec_mem_cleanup(void);

void *exec_mem_alloc(size_t len_req);
void exec_mem_free(void *ptr);

/*
 * attempt to grow the given allocation to the given size.  This funciton
 * returns zero on success and nonzero on failure.
 *
 * This function will never attempt to relocate your allocation because this
 * allocator is designed for executable code, and moving an allocation could
 * damage existing pointers and offsets; that is why this function can call.
 *
 * For best results, only attempt to grow the latest allocation.  The allocator
 * used by exec_mem_alloc will always pick an allocation at the beginning of the
 * largest contiguous area of memory; this means that growing the most recent
 * allocation will have a high probability of success, but growing older
 * allocations will have a high probability of failure.  I don't think the JIT
 * will ever have a good reason to grow an old allocation, anyways.
 */
int exec_mem_grow(void *ptr, size_t len_req);

struct exec_mem_stats {
    size_t free_bytes;
    size_t total_bytes;
    unsigned n_allocations;
    unsigned n_free_chunks;
};

void exec_mem_get_stats(struct exec_mem_stats *stats);
void exec_mem_print_stats(struct exec_mem_stats const *stats);

#ifdef INVARIANTS
/*
 * This checks to make sure no blocks overlap each other, and there is no
 * needless fragmentation.  It cannot check for "dangling pointer" situations
 * where some memory allocation that another component thinks is not free
 * actually is.  It also cannot prove there are no memory leaks.
 */
void exec_mem_check_integrity(void);
#endif

#endif
