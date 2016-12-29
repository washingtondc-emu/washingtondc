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

// SH4 16 KB Operand Cache
class Ocache {
public:
    static const size_t LONGS_PER_CACHE_LINE = 8;
    static const size_t ENTRY_COUNT = 512;
    static const size_t CACHE_LINE_SHIFT = 5;
    static const size_t CACHE_LINE_SIZE = LONGS_PER_CACHE_LINE * 4;
    static const size_t OP_CACHE_SIZE = ENTRY_COUNT * CACHE_LINE_SIZE;

    // The valid flag
    static const unsigned KEY_VALID_SHIFT = 0;
    static const unsigned KEY_VALID_MASK = 1 << KEY_VALID_SHIFT;

    // the dirty flag
    static const unsigned KEY_DIRTY_SHIFT = 1;
    static const unsigned KEY_DIRTY_MASK = 1 << KEY_DIRTY_SHIFT;

    // the tag represents bits 28:10 (inclusive) of a 29-bit address.
    static const unsigned KEY_TAG_SHIFT = 2;
    static const unsigned KEY_TAG_MASK = 0x7ffff << KEY_TAG_SHIFT;

    typedef size_t cache_line_t;     // index of cache-line (32-bytes/incrment)
    typedef boost::uint32_t cache_key_t;

    // this class does not take ownership of sh4 or mem, so they will not be
    // deleted by the destructor function
    Ocache(Sh4 *sh4, Memory *mem);
    ~Ocache();

    // Returns: zero on success, nonzero on failure.
    int cache_read(void *out, unsigned len, addr32_t paddr,
                   bool index_enable, bool cache_as_ram);
    /*
     * Write the n-byte value pointed to by data to memory through the cache in
     * copy-back mode.
     * Returns: zero on success, nonzero on failure.
     */
    int cache_write_cb(void const *data, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram);

    /*
     * Write the n-byte value pointed to by data to memory through the cache in
     * write-through mode.
     * Returns: zero on success, nonzero on failure.
     */
    int cache_write_wt(void const *data, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram);

    // reset the cache to its default (empty) state
    void reset(void);

    int cache_alloc(addr32_t paddr, bool index_enable, bool cache_as_ram);

    // if paddr is in the cache, paddr's entire cache line will be invalidated
    // no data will be written back.  This is part of the OCBI instruction's
    // implementation
    void invalidate(addr32_t paddr, bool index_enable, bool cache_as_ram);

    // if paddr is in the cache, paddr's entire cache line will be written back
    // and invalidated.  This is part of the OCBP instruction's implementation
    int purge(addr32_t paddr, bool index_enable, bool cache_as_ram);

    /*
     * prefetch the cache-line containing paddr.
     * This is the backend of the PREF instruction.
     */
    void pref(addr32_t paddr, bool index_enable, bool cache_as_ram);

private:
    Sh4 *sh4;
    Memory *mem;

    // 16 KB ("Operand Cache" in the hardware manual)
    uint8_t *op_cache;
    cache_key_t *op_cache_keys;

    template<typename buf_t>
    int do_cache_read(buf_t *out, addr32_t paddr, bool index_enable,
                      bool cache_as_ram);

    template<typename buf_t>
    int do_cache_write_cb(buf_t const *data, addr32_t paddr,
                          bool index_enable, bool cache_as_ram);

    template<typename buf_t>
    int do_cache_write_wt(buf_t const *data, addr32_t paddr,
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
    bool cache_check(cache_line_t line_no, addr32_t paddr);

    /*
     * returns the index into the op-cache where paddr
     * would go if it had an entry.
     */
    cache_line_t cache_selector(addr32_t paddr, bool index_enable,
                                bool cache_as_ram) const;

    /*
     * Load the cache-line corresponding to paddr into line.
     * Returns non-zero on failure.
     */
    int cache_load(cache_line_t line_no, addr32_t paddr);

    /*
     * Write the cache-line into memory and clear its dirty-bit.
     * returns non-zero on failure.
     */
    int cache_write_back(cache_line_t line_no);

    addr32_t cache_line_get_tag(cache_line_t line);

    // sets the line's tag to tag.
    void cache_line_set_tag(cache_line_t line, addr32_t tag);

    /*
     * returns a pointer to where paddr ought to go if ORA ram is enabled.
     *
     * This function does not verify that paddr is actually a valid address,
     * nor does it handle the possibility that paddr might straddle the border
     * between RAM area 1 and RAM area 2 (which are not adjacent in the
     * operand cache).  The caller must make sure that paddr is actually a RAM
     * address and that the entire length of the read/write operation can be
     * safely executed.
     */
    void *get_ram_addr(addr32_t paddr, bool index_enable);

    // extract the tag from the upper 19 bits of the lower 29 bits of paddr
    static addr32_t tag_from_paddr(addr32_t paddr);
};

inline addr32_t Ocache::cache_line_get_tag(cache_line_t line) {
    return (KEY_TAG_MASK & op_cache_keys[line]) >> KEY_TAG_SHIFT;
}

inline addr32_t Ocache::tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

inline void Ocache::cache_line_set_tag(cache_line_t line, addr32_t tag) {
    cache_key_t key = op_cache_keys[line];
    op_cache_keys[line] = (key & ~KEY_TAG_MASK) | (tag << KEY_TAG_SHIFT);
}

#endif
