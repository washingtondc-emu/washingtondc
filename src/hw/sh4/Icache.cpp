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

#ifndef ENABLE_SH4_ICACHE
#error this file cannot be built with the sh4 instruction cache disabled!
#endif

#include <cassert>
#include <cstring>

#include "BaseException.hpp"

#include "Icache.hpp"

static addr32_t sh4_icache_selector(addr32_t paddr, bool index_enable);
static addr32_t sh4_icache_tag_from_paddr(addr32_t paddr);
static addr32_t sh4_icache_line_get_tag(Sh4Icache *icache,
                                        sh4_icache_line_t line_idx);
static void sh4_icache_line_set_tag(Sh4Icache *icache,
                                    sh4_icache_line_t line_idx, addr32_t tag);
static int sh4_icache_load(Sh4Icache *icache, MemoryMap *mem,
                           sh4_icache_line_t line_idx, addr32_t paddr);
static bool sh4_icache_check(Sh4Icache *icache, sh4_icache_line_t line_idx,
                             addr32_t paddr);
static int sh4_icache_read1(Sh4Icache *icache, MemoryMap *mem,
                            uint32_t *out, addr32_t paddr,
                            bool index_enable);
static int sh4_icache_read2(Sh4Icache *icache, MemoryMap *mem, uint32_t *out,
                            addr32_t paddr, bool index_enable);

static const unsigned SH4_ICACHE_LONGS_PER_CACHE_LINE = 8;
static const unsigned SH4_ICACHE_ENTRY_COUNT = 256;
static const size_t SH4_ICACHE_LINE_SIZE = SH4_ICACHE_LONGS_PER_CACHE_LINE * 4;
static const size_t SH4_ICACHE_SIZE =
    SH4_ICACHE_ENTRY_COUNT * SH4_ICACHE_LINE_SIZE;

// the valid bit of the instruction cache keys
static const unsigned SH4_ICACHE_KEY_VALID_SHIFT = 0;
static const unsigned SH4_ICACHE_KEY_VALID_MASK =
    1 << SH4_ICACHE_KEY_VALID_SHIFT;

// 19-bit tag of the instruction cache keys
static const unsigned SH4_ICACHE_KEY_TAG_SHIFT = 1;
static const unsigned SH4_ICACHE_KEY_TAG_MASK =
         0x7ffff << SH4_ICACHE_KEY_TAG_SHIFT;

void sh4_icache_init(Sh4Icache *icache) {
    icache->inst_cache = new uint8_t[SH4_ICACHE_SIZE];
    icache->inst_cache_keys = new sh4_icache_key_t[SH4_ICACHE_ENTRY_COUNT];

    sh4_icache_reset(icache);
}

void sh4_icache_cleanup(Sh4Icache *icache) {
    delete[] icache->inst_cache_keys;
    delete[] icache->inst_cache;
}

void sh4_icache_reset(Sh4Icache *icache) {
    memset(icache->inst_cache, 0,
           sizeof(icache->inst_cache[0]) * SH4_ICACHE_SIZE);
    memset(icache->inst_cache_keys, 0,
           sizeof(icache->inst_cache_keys[0]) * SH4_ICACHE_ENTRY_COUNT);
}

/*
 * returns the index into the inst-cache where paddr
 * would go if it had an entry.
 */
static addr32_t sh4_icache_selector(addr32_t paddr, bool index_enable) {
    addr32_t ent_sel = paddr & 0xfe0;
    if (index_enable)
        ent_sel |= (paddr & (1 << 25)) >> 13;
    else
        ent_sel |= paddr & (1 << 12);
    ent_sel >>= 5;

    assert(ent_sel < SH4_ICACHE_ENTRY_COUNT);

    return ent_sel;
}

/* extract the tag from the upper 19 bits of the lower 29 bits of paddr */
static addr32_t sh4_icache_tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

static addr32_t sh4_icache_line_get_tag(Sh4Icache *icache,
                                        sh4_icache_line_t line_idx) {
    return (SH4_ICACHE_KEY_TAG_MASK & icache->inst_cache_keys[line_idx]) >>
        SH4_ICACHE_KEY_TAG_SHIFT;
}

/* sets the line's tag to tag. */
static void sh4_icache_line_set_tag(Sh4Icache *icache,
                                    sh4_icache_line_t line_idx, addr32_t tag) {
    sh4_icache_key_t *keyp = icache->inst_cache_keys + line_idx;
    *keyp = (*keyp & ~SH4_ICACHE_KEY_TAG_MASK) |
        (tag << SH4_ICACHE_KEY_TAG_SHIFT);
}

static int sh4_icache_load(Sh4Icache *icache, MemoryMap *mem,
                           sh4_icache_line_t line_idx, addr32_t paddr) {
    size_t n_bytes = sizeof(uint32_t) * SH4_ICACHE_LONGS_PER_CACHE_LINE;

    void *cache_ptr = icache->inst_cache + line_idx * SH4_ICACHE_LINE_SIZE;
    int err_code = mem->read(cache_ptr, paddr & ~31 & 0x1fffffff, n_bytes);
    if (err_code != 0)
        return err_code;

    sh4_icache_line_set_tag(icache, line_idx, sh4_icache_tag_from_paddr(paddr));
    icache->inst_cache_keys[line_idx] |= SH4_ICACHE_KEY_VALID_MASK;

    return 0;
}

/*
 * Return true if line matches paddr; else return false.
 *
 * This function does not verify that the cache is enabled; nor does it
 * verify that paddr is even in an area which can be cached.  The callee
 * should do that before calling this function.
 *
 * This function does not check the valid bit.
 */
static bool sh4_icache_check(Sh4Icache *icache, sh4_icache_line_t line_idx,
                             addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = sh4_icache_tag_from_paddr(paddr);

    addr32_t line_tag = sh4_icache_line_get_tag(icache, line_idx);
    if (line_tag == paddr_tag)
        return true;
    return false;
}

static int sh4_icache_read1(Sh4Icache *icache, MemoryMap *mem,
                            uint32_t *out, addr32_t paddr,
                            bool index_enable) {
    int err = 0;

    sh4_icache_line_t line_idx = sh4_icache_selector(paddr, index_enable);
    uint8_t *line = line_idx * SH4_ICACHE_LINE_SIZE + icache->inst_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (sh4_icache_check(icache, line_idx, paddr) &&
        (icache->inst_cache_keys[line_idx] & SH4_ICACHE_KEY_VALID_MASK)) {
        // cache hit
        *out = line[byte_idx];
        return 0;
    } else {
        if ((err = sh4_icache_load(icache, mem, line_idx, paddr)) != 0)
            return err;
        *out = line[byte_idx];
        return 0;
    }
}

static int sh4_icache_read2(Sh4Icache *icache, MemoryMap *mem, uint32_t *out,
                            addr32_t paddr, bool index_enable) {
    int err = 0;

    if (paddr & 0x1) {
        // do it 1 byte at a time
        uint32_t out_buf = 0;
        for (int i = 0; i < 2; i++) {
            uint32_t tmp;
            int err;

            err = sh4_icache_read1(icache, mem, &tmp, paddr + i, index_enable);
            if (err)
                return err;

            out_buf |= tmp << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    sh4_icache_line_t line_idx = sh4_icache_selector(paddr, index_enable);
    uint8_t *line = line_idx * SH4_ICACHE_LINE_SIZE + icache->inst_cache;
    unsigned sw_idx = (paddr & 0x1f) >> 1;

    if (sh4_icache_check(icache, line_idx, paddr) &&
        (icache->inst_cache_keys[line_idx] & SH4_ICACHE_KEY_VALID_MASK)) {
        // cache hit
        *out = ((uint16_t*)line)[sw_idx];
        return 0;
    } else {
        if ((err = sh4_icache_load(icache, mem, line_idx, paddr)) != 0)
            return err;
        *out = ((uint16_t*)line)[sw_idx];
        return 0;
    }
}

int sh4_icache_read(Sh4Icache *icache, MemoryMap *mem, uint32_t *out,
                    addr32_t paddr, bool index_enable) {
    return sh4_icache_read2(icache, mem, out, paddr, index_enable);
}
