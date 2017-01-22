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

#include <cstring>

#include "sh4.hpp"
#include "BaseException.hpp"

#include "Icache.hpp"

Icache::Icache(Sh4 *sh4, MemoryMap *mem) {
    this->sh4 = sh4;
    this->mem = mem;

    inst_cache = new uint8_t[INST_CACHE_SIZE];
    inst_cache_keys = new cache_key_t[ENTRY_COUNT];

    reset();
}

Icache::~Icache() {
    delete[] inst_cache_keys;
    delete[] inst_cache;
}

void Icache::reset() {
    memset(inst_cache, 0, sizeof(inst_cache[0]) * INST_CACHE_SIZE);
    memset(inst_cache_keys, 0, sizeof(inst_cache_keys[0]) * ENTRY_COUNT);
}

addr32_t Icache::cache_selector(addr32_t paddr, bool index_enable) const {
    addr32_t ent_sel = paddr & 0xfe0;
    if (index_enable)
        ent_sel |= (paddr & (1 << 25)) >> 13;
    else
        ent_sel |= paddr & (1 << 12);
    ent_sel >>= 5;

    return ent_sel;
}

bool Icache::cache_check(cache_line_t line_idx, addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = tag_from_paddr(paddr);

    addr32_t line_tag = cache_line_get_tag(line_idx);
    if (line_tag == paddr_tag)
        return true;
    return false;
}

int Icache::cache_load(cache_line_t line_idx, addr32_t paddr) {
    int err_code;
    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_CACHE_LINE;

    if ((err_code = mem->read(inst_cache + line_idx * CACHE_LINE_SIZE,
                              paddr & ~31 & 0x1fffffff, n_bytes)) != 0)
        return err_code;

    cache_line_set_tag(line_idx, tag_from_paddr(paddr));
    inst_cache_keys[line_idx] |= KEY_VALID_MASK;

    return 0;
}

int Icache::read1(boost::uint32_t *out, addr32_t paddr, bool index_enable) {
    int err = 0;

    cache_line_t line_idx = cache_selector(paddr, index_enable);
    uint8_t *line = line_idx * CACHE_LINE_SIZE + inst_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (cache_check(line_idx, paddr) && (inst_cache_keys[line_idx] & KEY_VALID_MASK)) {
        // cache hit
        *out = line[byte_idx];
        return 0;
    } else {
        if ((err = cache_load(line_idx, paddr)) != 0)
            return err;
        *out = line[byte_idx];
        return 0;
    }
}

int Icache::read2(boost::uint32_t *out, addr32_t paddr, bool index_enable) {
    int err = 0;

    if (paddr & 0x1) {
        // do it 1 byte at a time
        uint32_t out_buf = 0;
        for (int i = 0; i < 2; i++) {
            uint32_t tmp;
            int err;

            err = read1(&tmp, paddr + i, index_enable);
            if (err)
                return err;

            out_buf |= tmp << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    cache_line_t line_idx = cache_selector(paddr, index_enable);
    uint8_t *line = line_idx * CACHE_LINE_SIZE + inst_cache;
    unsigned sw_idx = (paddr & 0x1f) >> 1;

    if (cache_check(line_idx, paddr) &&
        (inst_cache_keys[line_idx] & KEY_VALID_MASK)) {
        // cache hit
        *out = ((uint16_t*)line)[sw_idx];
        return 0;
    } else {
        if ((err = cache_load(line_idx, paddr)) != 0)
            return err;
        *out = ((uint16_t*)line)[sw_idx];
        return 0;
    }
}
