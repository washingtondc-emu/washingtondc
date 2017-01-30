/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#ifndef OCACHE_HPP_
#define OCACHE_HPP_

#ifndef ENABLE_SH4_OCACHE
#error this file cannot be built with the sh4 operand cache disabled!
#endif

#include <boost/cstdint.hpp>

#include "types.hpp"
#include "MemoryMap.hpp"

typedef size_t sh4_ocache_line_t;     // index of cache-line (32-bytes/incrment)
typedef uint32_t sh4_ocache_key_t;

// SH4 16 KB Operand Cache
struct Sh4Ocache {
    // 16 KB ("Operand Cache" in the hardware manual)
    uint8_t *op_cache;
    sh4_ocache_key_t *op_cache_keys;
};

/* Returns: zero on success, nonzero on failure */
int sh4_ocache_read(Sh4Ocache *state, MemoryMap *mem, void *out,
                    unsigned len, addr32_t paddr, bool index_enable,
                    bool cache_as_ram);

/*
 * Write the n-byte value pointed to by data to memory through the cache in
 * copy-back mode.
 * Returns: zero on success, nonzero on failure.
 */
int sh4_ocache_write_cb(Sh4Ocache *state, MemoryMap *mem, void const *data,
                        unsigned len, addr32_t paddr, bool index_enable,
                        bool cache_as_ram);

/*
 * Write the n-byte value pointed to by data to memory through the cache in
 * write-through mode.
 * Returns: zero on success, nonzero on failure.
 */
int sh4_ocache_write_wt(Sh4Ocache *state, MemoryMap *mem, void const *data,
                        unsigned len, addr32_t paddr, bool index_enable,
                        bool cache_as_ram);

/* reset the cache to its default (empty) state */
void sh4_ocache_reset(Sh4Ocache *sh4_ocache);

/* initialize sh4_ocache */
void sh4_ocache_init(Sh4Ocache *sh4_ocache);

/* cleanup sh4_ocache and release resources */
void sh4_ocache_cleanup(Sh4Ocache *sh4_ocache);

/*
 * prefetch the cache-line containing paddr.
 * This is the backend of the PREF instruction.
 */
void sh4_ocache_pref(Sh4Ocache *sh4_ocache, MemoryMap *mem, addr32_t paddr,
                     bool index_enable, bool cache_as_ram);

/*
 * if paddr is in the cache, paddr's entire cache line will be written back
 * and invalidated.  This is part of the OCBP instruction's implementation
 */
int sh4_ocache_purge(Sh4Ocache *sh4_ocache, MemoryMap *mem,
                     addr32_t paddr, bool index_enable,
                     bool cache_as_ram);

/*
 * if paddr is in the cache, paddr's entire cache line will be invalidated
 * no data will be written back.  This is part of the OCBI instruction's
 * implementation
 */
void sh4_ocache_invalidate(Sh4Ocache *sh4_ocache, addr32_t paddr,
                           bool index_enable, bool cache_as_ram);

int sh4_ocache_alloc(Sh4Ocache *sh4_ocache, MemoryMap *mem,
                     addr32_t paddr, bool index_enable,
                     bool cache_as_ram);

#endif
