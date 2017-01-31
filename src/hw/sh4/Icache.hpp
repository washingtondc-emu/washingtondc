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

#ifndef ICACHE_HPP_
#define ICACHE_HPP_

#ifndef ENABLE_SH4_ICACHE
#error this file cannot be built with the sh4 instruction cache disabled!
#endif

#include <boost/cstdint.hpp>

#include "types.hpp"
#include "MemoryMap.hpp"

typedef size_t sh4_icache_line_t;     // index of cache-line (32-bytes/incrment)
typedef uint32_t sh4_icache_key_t;

struct Sh4Icache {
    // 8 KB ("Instruction Cache" in the hardware manual)
    uint8_t *inst_cache;
    sh4_icache_key_t *inst_cache_keys;
};

int sh4_icache_read(Sh4Icache *icache, uint32_t *out,
                    addr32_t paddr, bool index_enable);

/* reset the cache to its default (empty) state */
void sh4_icache_reset(Sh4Icache *icache);

/* initialize Sh4Icache */
void sh4_icache_init(Sh4Icache *icache);

/* cleanup icache and release resources */
void sh4_icache_cleanup(Sh4Icache *icache);

#endif
