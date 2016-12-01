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

#ifndef OCACHE_HPP_
#define OCACHE_HPP_

#ifndef ENABLE_SH4_OCACHE
#error this file cannot be built with the sh4 operand cache disabled!
#endif

#include <boost/cstdint.hpp>

#include "types.hpp"

// SH4 Operand Cache
class Ocache {
public:
    static const unsigned LONGS_PER_CACHE_LINE = 8;
    static const unsigned ENTRY_COUNT = 512;

    // The valid flag
    static const unsigned KEY_VALID_SHIFT = 0;
    static const unsigned KEY_VALID_MASK = 1 << KEY_VALID_SHIFT;

    // the dirty flag
    static const unsigned KEY_DIRTY_SHIFT = 1;
    static const unsigned KEY_DIRTY_MASK = 1 << KEY_DIRTY_SHIFT;

    // the tag represents bits 28:10 (inclusive) of a 29-bit address.
    static const unsigned KEY_TAG_SHIFT = 2;
    static const unsigned KEY_TAG_MASK = 0x7ffff << KEY_TAG_SHIFT;

    struct cache_line {
        // contains the tag, dirty bit and valid bit
        boost::uint32_t key;

        // cache line data array
        union {
            boost::uint8_t byte[LONGS_PER_CACHE_LINE * 4];
            boost::uint16_t sw[LONGS_PER_CACHE_LINE * 2];
            boost::uint32_t lw[LONGS_PER_CACHE_LINE];
            boost::uint64_t qw[LONGS_PER_CACHE_LINE / 2];
        };
    };

    // this class does not take ownership of sh4 or mem, so they will not be
    // deleted by the destructor function
    Ocache(Sh4 *sh4, Memory *mem);
    ~Ocache();

    // Returns: zero on success, nonzero on failure.
    int cache_read(basic_val_t *out, unsigned len, addr32_t paddr,
                   bool index_enable, bool cache_as_ram);
    /*
     * Write the n-byte value pointed to by data to memory through the cache in
     * copy-back mode.
     * Returns: zero on success, nonzero on failure.
     */
    int cache_write_cb(basic_val_t data, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram);

    /*
     * Write the n-byte value pointed to by data to memory through the cache in
     * write-through mode.
     * Returns: zero on success, nonzero on failure.
     */
    int cache_write_wt(basic_val_t data, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram);

    // reset the cache to its default (empty) state
    void reset(void);

private:
    Sh4 *sh4;
    Memory *mem;

    // 16 KB ("Operand Cache" in the hardware manual)
    struct cache_line *op_cache;

    template<int N_BYTES>
    int do_cache_read(basic_val_t *out, addr32_t paddr, bool index_enable,
                      bool cache_as_ram);

    template<int N_BYTES>
    int do_cache_write_cb(basic_val_t data, addr32_t paddr,
                          bool index_enable, bool cache_as_ram);

    template<int N_BYTES>
    int do_cache_write_wt(basic_val_t data, addr32_t paddr,
                          bool index_enable, bool cache_as_ram);

    /*
     * Return true if line matches paddr; else return false.
     *
     * This function does not verify that the cache is enabled; nor does it
     * verify that paddr is even in an area which can be cached.  The callee
     * should do that before calling this function.
     *
     * This function does not check the valid bit.
     */
    bool cache_check(struct cache_line const *line, addr32_t paddr);

    /*
     * returns the index into the op-cache where paddr
     * would go if it had an entry.
     */
    addr32_t cache_selector(addr32_t paddr, bool index_enable,
                            bool cache_as_ram) const;

    /*
     * Load the cache-line corresponding to paddr into line.
     * Returns non-zero on failure.
     */
    int cache_load(struct cache_line *line, addr32_t paddr);

    /*
     * Write the cache-line into memory and clear its dirty-bit.
     * returns non-zero on failure.
     */
    int cache_write_back(struct cache_line *line);

    static addr32_t
    cache_line_get_tag(struct cache_line const *line);

    // sets the line's tag to tag.
    void cache_line_set_tag(struct cache_line *line,
                            addr32_t tag);

    // extract the tag from the upper 19 bits of the lower 29 bits of paddr
    static addr32_t tag_from_paddr(addr32_t paddr);
};

inline addr32_t
Ocache::cache_line_get_tag(struct cache_line const *line) {
    return (KEY_TAG_MASK & line->key) >> KEY_TAG_SHIFT;
}

inline addr32_t Ocache::tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

inline void Ocache::cache_line_set_tag(struct cache_line *line,
                                       addr32_t tag) {
    line->key &= ~KEY_TAG_MASK;
    line->key |= tag << KEY_TAG_SHIFT;
}

#endif
