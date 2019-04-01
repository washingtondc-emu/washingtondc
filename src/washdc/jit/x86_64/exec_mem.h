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
