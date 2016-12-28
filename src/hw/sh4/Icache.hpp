/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

// SH4 8KB instruction cache
class Icache {
public:
    static const unsigned LONGS_PER_CACHE_LINE = 8;
    static const unsigned ENTRY_COUNT = 256;
    static const size_t CACHE_LINE_SIZE = LONGS_PER_CACHE_LINE * 4;
    static const size_t INST_CACHE_SIZE = ENTRY_COUNT * CACHE_LINE_SIZE;

    // the valid bit of the instruction cache keys
    static const unsigned KEY_VALID_SHIFT = 0;
    static const unsigned KEY_VALID_MASK = 1 << KEY_VALID_SHIFT;

    // 19-bit tag of the instruction cache keys
    static const unsigned KEY_TAG_SHIFT = 1;
    static const unsigned KEY_TAG_MASK = 0x7ffff << KEY_TAG_SHIFT;

    typedef size_t cache_line_t;     // index of cache-line (32-bytes/incrment)
    typedef boost::uint32_t cache_key_t;

    // this class does not take ownership of sh4 or mem, so they will not be
    // deleted by the destructor function
    Icache(Sh4 *sh4, Memory *mem);
    ~Icache();

    // Returns: zero on success, nonzero on failure
    int read(boost::uint32_t *out, addr32_t paddr, bool index_enable);

    // reset the cache to its default (empty) state
    void reset(void);
private:
    Sh4 *sh4;
    Memory *mem;

    // 8 KB ("Instruction Cache" in the hardware manual)
    uint8_t *inst_cache;
    cache_key_t *inst_cache_keys;

    int read1(boost::uint32_t *out, addr32_t paddr, bool index_enable);
    int read2(boost::uint32_t *out, addr32_t paddr, bool index_enable);

    /*
     * returns the index into the inst-cache where paddr
     * would go if it had an entry.
     */
    addr32_t cache_selector(addr32_t paddr, bool index_enable) const;

    /*
     * Return true if line matches paddr; else return false.
     *
     * This function does not verify that the cache is enabled; nor does it
     * verify that paddr is even in an area which can be cached.  The callee
     * should do that before calling this function.
     *
     * This function does not check the valid bit.
     */
    bool cache_check(cache_line_t line_idx, addr32_t paddr);

    /*
     * Load the cache-line corresponding to paddr into line.
     * Returns non-zero on failure.
     */
    int cache_load(cache_line_t line_idx, addr32_t paddr);

    addr32_t cache_line_get_tag(cache_line_t line_idx);

    // sets the line's tag to tag.
    void cache_line_set_tag(cache_line_t line_idx, addr32_t tag);

    // extract the tag from the upper 19 bits of the lower 29 bits of paddr
    static addr32_t tag_from_paddr(addr32_t paddr);
};

inline int
Icache::read(boost::uint32_t *out, addr32_t paddr, bool index_enable) {
    return read2(out, paddr, index_enable);
}

inline addr32_t
Icache::cache_line_get_tag(cache_line_t line_idx) {
    return (KEY_TAG_MASK & inst_cache_keys[line_idx]) >> KEY_TAG_SHIFT;
}

inline addr32_t Icache::tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

inline void Icache::cache_line_set_tag(cache_line_t line_idx, addr32_t tag) {
    cache_key_t *keyp = inst_cache_keys + line_idx;
    *keyp = (*keyp & ~KEY_TAG_MASK) | (tag << KEY_TAG_SHIFT);
}

#endif
