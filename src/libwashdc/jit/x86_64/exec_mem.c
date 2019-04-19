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

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "log.h"
#include "washdc/error.h"

#include "exec_mem.h"

#define X86_64_ALLOC_SIZE (512 * 1024 * 1024)

static void *native;

#define FREE_CHUNK_MAGIC  0xca55e77e
#define ALLOC_CHUNK_MAGIC 0xfeedface

#define MIN_FREE_CHUNK_SIZE sizeof(struct free_chunk)

// this list should always be sorted from small addrs to large addrs
static struct free_chunk {
#ifdef INVARIANTS
    unsigned magic;
#endif
    struct free_chunk *next;
    struct free_chunk **pprev;
    size_t len;
} *free_mem;

struct alloc_chunk {
#ifdef INVARIANTS
    unsigned magic;
#endif
    size_t len, len_req;
};

static size_t n_allocations;
unsigned largest_alloc, smallest_alloc;

/*
 * This returns a pointer to the true start of the allocation, which is its
 * struct alloc_chunk.
 */
static void *get_alloc_start(void *alloc_ptr);

void exec_mem_init(void) {
    native = mmap(NULL, X86_64_ALLOC_SIZE, PROT_WRITE | PROT_EXEC | PROT_READ,
                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (native == MAP_FAILED)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    free_mem = native;
    free_mem->next = NULL;
    free_mem->len = X86_64_ALLOC_SIZE;
    free_mem->pprev = &free_mem;

#ifdef INVARIANTS
    free_mem->magic = FREE_CHUNK_MAGIC;
#endif
}

void exec_mem_cleanup(void) {
    munmap(native, X86_64_ALLOC_SIZE);
    native = NULL;
}

/*
 * This function always returns memory that is aligned to an 8-byte boundary
 * due to autism.  Strictly speaking, alignment is not needed on x86 but I like
 * it.
 */
void* exec_mem_alloc(size_t len_req) {
    struct free_chunk *curs;
    struct free_chunk *candidate = NULL;
    size_t len = len_req;

    // add in metadata plus room for padding
    len += sizeof(struct alloc_chunk) + 8;

    // make sure the next chunk after the one we return is 8-byte aligned
    while (len % 8)
        len++;

    /*
     * pull off of the beginning of the largest available allocation.
     * this way, reallocations are more likely to succeed.
     */
    for (curs = free_mem; curs; curs = curs->next) {
#ifdef INVARIANTS
        if (curs->magic != FREE_CHUNK_MAGIC) {
            LOG_ERROR("%s - memory corruption detected at %p\n",
                      __func__, curs);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
#endif

        if (curs->len >= len) {
            if (!candidate || curs->len > candidate->len)
                candidate = curs;
        }
    }

    if (!candidate) {
        struct exec_mem_stats stats;
        LOG_ERROR("%s - failed alloc of size %llu\n",
                  __func__, (unsigned long long)len);
        LOG_ERROR("exec_mem stats dump follows\n");
        exec_mem_get_stats(&stats);
        exec_mem_print_stats(&stats);
        return NULL;
    }

    if (candidate->len == len) {
        // easy case - just remove candidate from the pool
        if (candidate->next)
            candidate->next->pprev = candidate->pprev;
        *candidate->pprev = candidate->next;
    } else if (candidate->len - len < MIN_FREE_CHUNK_SIZE) {
        /*
         * remove the candidate from the pool and increase length because we
         * can't possibly store another chunk after this.
         */
        len = candidate->len;
        if (candidate->next)
            candidate->next->pprev = candidate->pprev;
        *candidate->pprev = candidate->next;
    } else {
        // split candidate allocation
        struct free_chunk *new_chunk =
            (struct free_chunk*)(((uint8_t*)candidate) + len/* sizeof(candidate) */);
        new_chunk->next = candidate->next;
        if (new_chunk->next)
            new_chunk->next->pprev = &new_chunk->next;
        new_chunk->pprev = candidate->pprev;
        *new_chunk->pprev = new_chunk;
        new_chunk->len = candidate->len - len;
#ifdef INVARIANTS
        new_chunk->magic = FREE_CHUNK_MAGIC;
#endif
    }

    struct alloc_chunk *chunk = (struct alloc_chunk*)(void*)candidate;
    chunk->len = len;
    chunk->len_req = len_req;

#ifdef INVARIANTS
    chunk->magic = ALLOC_CHUNK_MAGIC;
#endif

    /*
     * align the output.  This is safe to do because we reserved an extra
     * 8 bytes earlier when we were determing the allocation length.
     */
    uintptr_t ret_ptr = (uintptr_t)(void*)candidate + sizeof(*chunk);
    while (ret_ptr % 8)
        ret_ptr++;

    n_allocations++;

    void *ret = (void*)ret_ptr;
    memset(ret, 0, len_req);
    return ret;
}

void exec_mem_free(void *ptr) {
    // match behavior of the libc free function by ignoring NULL
    if (!ptr)
        return;

    void *alloc_start = get_alloc_start(ptr);
    struct alloc_chunk *alloc = (struct alloc_chunk*)alloc_start;
    struct free_chunk *free_chunk = (struct free_chunk*)alloc_start;

    uintptr_t as_int = (uintptr_t)alloc_start;

#ifdef INVARIANTS
    if (as_int % 8) {
        LOG_ERROR("0x%08llx is not 8-byte aligned!\n", (long long)as_int);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

#ifdef INVARIANTS
    if (alloc->magic != ALLOC_CHUNK_MAGIC) {
        LOG_ERROR("Corrupted alloc_chunk at %p\n", alloc);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    size_t len = alloc->len;

    memset(free_chunk, 0, sizeof(*free_chunk));

    if (!free_mem) {
        // Oh wow, this is the only chunk.
#ifdef INVARIANTS
        free_chunk->magic = FREE_CHUNK_MAGIC;
#endif
        free_chunk->len = len;
        free_chunk->pprev = &free_mem;
        free_mem = free_chunk;
        goto successful_free;
    }

    uintptr_t first_addr = as_int;
    uintptr_t last_addr = first_addr + (len - 1);

    uintptr_t free_mem_first = (uintptr_t)(void*)free_mem;
    if ((free_mem_first - 1) > last_addr) {
        // this is the new first chunk
#ifdef INVARIANTS
        free_chunk->magic = FREE_CHUNK_MAGIC;
#endif
        free_chunk->len = len;
        free_chunk->pprev = &free_mem;
        free_chunk->next = free_mem;
        free_chunk->next->pprev = &free_chunk->next;
        free_mem = free_chunk;
        goto successful_free;
    } else if ((free_mem_first - 1) == last_addr) {
        // absorb free_mem into this chunk and make it the new free_mem
#ifdef INVARIANTS
        free_chunk->magic = FREE_CHUNK_MAGIC;
#endif
        free_chunk->len = len + free_mem->len;
        free_chunk->next = free_mem->next;
        if (free_chunk->next)
            free_chunk->next->pprev = &free_chunk->next;
        free_chunk->pprev = &free_mem;
        free_mem = free_chunk;
        goto successful_free;
    }

    struct free_chunk *curs, *pre = NULL, *post = NULL;
    for (curs = free_mem; curs; curs = curs->next) {

#ifdef INVARIANTS
        if (curs->magic != FREE_CHUNK_MAGIC)
            RAISE_ERROR(ERROR_INTEGRITY);
#endif

        struct free_chunk *next = curs->next;
        if (next) {
            uintptr_t next_first = (uintptr_t)(void*)next;
            if (next_first > last_addr) {
                pre = curs;
                post = next;
                break;
            }
        } else {
            pre = curs;
            post = NULL;
            break;
        }
    }

    if (!pre) {
        RAISE_ERROR(ERROR_INTEGRITY);
    }

#ifdef INVARIANTS
    if (pre->magic != FREE_CHUNK_MAGIC)
        RAISE_ERROR(ERROR_INTEGRITY);
    if (post && post->magic != FREE_CHUNK_MAGIC)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    uintptr_t pre_first = (uintptr_t)(void*)pre;
    uintptr_t pre_last = pre_first + (pre->len - 1);

    if (post) {
#ifdef INVARIANTS
        if (post->pprev != &pre->next)
            RAISE_ERROR(ERROR_INTEGRITY);
#endif

        // add the new free chunk inbetween pre and post
        uintptr_t post_first = (uintptr_t)(void*)post;
#ifdef INVARIANTS
        uintptr_t post_last = post_first + (post->len - 1);

        if (post_first <= last_addr) {
            LOG_ERROR("pre range: 0x%08llx through 0x%08llx\n",
                      (unsigned long long)pre_first,
                      (unsigned long long)pre_last);
            LOG_ERROR("new chunk: 0x%08llx through 0x%08llx\n",
                      (unsigned long long)first_addr,
                      (unsigned long long)last_addr);
            LOG_ERROR("post range: 0x%08llx through 0x%08llx\n",
                      (unsigned long long)post_first,
                      (unsigned long long)post_last);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
#endif

#ifdef INVARIANTS
        free_chunk->magic = FREE_CHUNK_MAGIC;
#endif
        free_chunk->len = len;

        if ((first_addr - 1) == pre_last) {
            // absorb free_chunk into pre
            pre->len += len;
            free_chunk = pre;
        } else {
            // build a link from pre to free_chunk
            pre->next = free_chunk;
            free_chunk->pprev = &pre->next;
        }

        if ((post_first - 1) == last_addr) {
            // absorb post into free_chunk
            free_chunk->next = post->next;
            if (free_chunk->next)
                free_chunk->next->pprev = &free_chunk->next;
            free_chunk->len += post->len;
        } else {
            // build a link from free_chunk to post
            free_chunk->next = post;
            post->pprev = &free_chunk->next;
        }

        goto successful_free;
    } else if ((first_addr - 1) == pre_last) {
        // this is easy, abosrb free_chunk into pre
        // pre is the last chunk, and it will continue to be the last chunk
        pre->len += len;
        goto successful_free;
    } else {
        // free_chunk is the new last chunk
#ifdef INVARIANTS
        free_chunk->magic = FREE_CHUNK_MAGIC;
#endif
        free_chunk->len = len;
        free_chunk->next = NULL;
        free_chunk->pprev = &pre->next;
        pre->next = free_chunk;
        goto successful_free;
    }

    RAISE_ERROR(ERROR_INTEGRITY);

successful_free:
    n_allocations--;
}

int exec_mem_grow(void *ptr, size_t len_req) {
    struct alloc_chunk *alloc = (struct alloc_chunk*)get_alloc_start(ptr);
    uintptr_t alloc_first = (uintptr_t)alloc;
    uintptr_t alloc_last = alloc_first + (alloc->len - 1);

    /*
     * len_req_aligned represents the size of the allocation post-growth
     * including the size of the struct alloc_chunk and any necessary padding.
     */
    size_t grow_amt = len_req - alloc->len_req;
    size_t len_req_aligned = alloc->len + grow_amt;
    while (len_req_aligned % 8)
        len_req_aligned++;
    uintptr_t alloc_goal = alloc_first + len_req_aligned - 1;

#ifdef INVARIANTS
    if (alloc->magic != ALLOC_CHUNK_MAGIC)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    if (alloc->len_req >= len_req)
        return 0; // nothing to do here, i suppose

    struct free_chunk *curs;
    for (curs = free_mem; curs; curs = curs->next) {
        uintptr_t curs_first = (uintptr_t)curs;
        uintptr_t curs_last = curs_first + (curs->len - 1);

#ifdef INVARIANTS
        // make sure the allocation doesn't overlap with the free chunk
        if ((alloc_first >= curs_first && alloc_first <= curs_last) ||
            (alloc_last >= curs_first && alloc_last <= curs_last))
            RAISE_ERROR(ERROR_INTEGRITY);
#endif

#ifdef INVARIANTS
        if (curs->magic != FREE_CHUNK_MAGIC)
            RAISE_ERROR(ERROR_INTEGRITY);
#endif

        if ((curs_first - 1) == alloc_last) {
            if (alloc_goal <= curs_last) {
#ifdef INVARIANTS
                if (*curs->pprev != curs)
                    RAISE_ERROR(ERROR_INTEGRITY);
#endif

                // we can do it!
                if (alloc->len - len_req_aligned + curs->len <
                    MIN_FREE_CHUNK_SIZE) {
                    // take the entire chunk
                    if (curs->next)
                        curs->next->pprev = curs->pprev;
                    *curs->pprev = curs->next;
                    alloc->len += curs->len;
                    memset(curs, 0, sizeof(*curs));
                    alloc->len_req = len_req;
#ifdef INVARIANTS
                    if (curs == free_mem)
                        RAISE_ERROR(ERROR_INTEGRITY);
#endif
                    return 0;
                } else {
                    // split curs
                    struct free_chunk *new_chunk =
                        (struct free_chunk*)(void*)(alloc_goal + 1);
                    memset(new_chunk, 0xaa, sizeof(*new_chunk));
#ifdef INVARIANTS
                    new_chunk->magic = FREE_CHUNK_MAGIC;
#endif
                    new_chunk->pprev = curs->pprev;
                    *new_chunk->pprev = new_chunk;
                    new_chunk->next = curs->next;
                    if (new_chunk->next)
                        new_chunk->next->pprev = &new_chunk->next;
                    new_chunk->len = curs_last - (alloc_goal + 1) + 1;
                    alloc->len_req = len_req;
                    alloc->len = alloc_goal - alloc_first + 1;
#ifdef INVARIANTS
                    if (curs == free_mem)
                        RAISE_ERROR(ERROR_INTEGRITY);
#endif
                    return 0;
                }
            } else {
                /*
                 * exec_mem_free always merges adjacent regions when it is
                 * possible to do so.  This means that if curs isn't big enough
                 * to grow the allocation, then it cannot be done.
                 */
                return -1;
            }
        } else if (curs_first > alloc_last) {
            /*
             * memory is too fragmented, the byte immediately following the
             * allocation is already taken.
             */
            return -1;
        }
    }

    return -1;
}

static void *get_alloc_start(void *alloc_ptr) {
    uintptr_t as_int = (uintptr_t)alloc_ptr;

    /*
     * as_int will be aligned to eight bytes.
     * the alloc_chunk will begin before that at the first 8-byte boundary
     * which has enough space between it and as_int to hold alloc_chunk.
     */
    size_t disp = sizeof(struct alloc_chunk);
    while (disp % 8)
        disp++;
    as_int -= disp;

    return (void*)as_int;
}

void exec_mem_get_stats(struct exec_mem_stats *stats) {
    size_t n_bytes = 0;
    unsigned n_free_chunks = 0;
    struct free_chunk *curs;
    for (curs = free_mem; curs; curs = curs->next) {
        n_bytes += curs->len;
        n_free_chunks++;
    }

    stats->total_bytes = X86_64_ALLOC_SIZE;
    stats->free_bytes = n_bytes;
    stats->n_allocations = n_allocations;
    stats->n_free_chunks = n_free_chunks;
}

void exec_mem_print_stats(struct exec_mem_stats const *stats) {
    double percent =
        100.0 * (double)stats->free_bytes / (double)stats->total_bytes;

    LOG_INFO("exec_mem: %llu free bytes out of %llu total (%f%%)\n",
             (unsigned long long)stats->free_bytes,
             (unsigned long long)stats->total_bytes,
             percent);
    LOG_INFO("exec_mem: There are %u active allocations\n",
             stats->n_allocations);
    LOG_INFO("exec_mem: There are %u total free chunks\n", stats->n_free_chunks);
}

#ifdef INVARIANTS
void exec_mem_check_integrity(void) {
    struct free_chunk *curs;
    for (curs = free_mem; curs->next; curs = curs->next) {
        struct free_chunk *next = curs->next;
        uintptr_t curs_first = (uintptr_t)curs;
        uintptr_t curs_last = curs_first + (curs->len - 1);
        uintptr_t next_first = (uintptr_t)next;

        if (next_first - 1 == curs_last) {
            LOG_ERROR("exec_mem: needless memory fragmentation\n");
            RAISE_ERROR(ERROR_INTEGRITY);
        }

        struct free_chunk *curs2;
        for (curs2 = next; curs2; curs2 = curs2->next) {
            uintptr_t curs2_first = (uintptr_t)curs2;
            uintptr_t curs2_last = curs2_first + (curs2->len - 1);

            if (curs2_first <= curs_last || curs2_last <= curs_last) {
                LOG_ERROR("exec_mem: out-of-order memory chunks\n");
                LOG_ERROR("[0x%08llx-0x%08llx] should not come after "
                          "[0x%08llx-0x%08llx]\n",
                          (long long)curs_first, (long long)curs_last,
                          (long long)curs2_first, (long long)curs2_last);
                RAISE_ERROR(ERROR_INTEGRITY);
            }

            if ((curs_first >= curs2_first && curs_first <= curs2_last) ||
                (curs_last >= curs2_first && curs_last <= curs2_first)) {
                LOG_ERROR("exec_mem: memory overlap\n");
                LOG_ERROR("[0x%08llx-0x%08llx] overlaps with "
                          "[0x%08llx-0x%08llx]\n",
                          (long long)curs_first, (long long)curs_last,
                          (long long)curs2_first, (long long)curs2_last);
                RAISE_ERROR(ERROR_INTEGRITY);
            }
        }
    }
}
#endif
