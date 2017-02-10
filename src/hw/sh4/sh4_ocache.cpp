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

#include <cassert>
#include <cstring>
#include <boost/cstdint.hpp>

#include "sh4.hpp"
#include "BaseException.hpp"

#include "sh4_ocache.hpp"

static const size_t SH4_OCACHE_LONGS_PER_CACHE_LINE = 8;
static const size_t SH4_OCACHE_ENTRY_COUNT = 512;
static const size_t SH4_OCACHE_LINE_SHIFT = 5;
static const size_t SH4_OCACHE_LINE_SIZE = SH4_OCACHE_LONGS_PER_CACHE_LINE * 4;
static const size_t SH4_OCACHE_SIZE = SH4_OCACHE_ENTRY_COUNT *
    SH4_OCACHE_LINE_SIZE;

// The valid flag
static const unsigned SH4_OCACHE_KEY_VALID_SHIFT = 0;
static const unsigned SH4_OCACHE_KEY_VALID_MASK = 1 << SH4_OCACHE_KEY_VALID_SHIFT;

// the dirty flag
static const unsigned SH4_OCACHE_KEY_DIRTY_SHIFT = 1;
static const unsigned SH4_OCACHE_KEY_DIRTY_MASK = 1 << SH4_OCACHE_KEY_DIRTY_SHIFT;

// the tag represents bits 28:10 (inclusive) of a 29-bit address.
static const unsigned SH4_OCACHE_KEY_TAG_SHIFT = 2;
static const unsigned SH4_OCACHE_KEY_TAG_MASK = 0x7ffff << SH4_OCACHE_KEY_TAG_SHIFT;

void sh4_ocache_init(struct sh4_ocache *ocache) {
    ocache->oc_ram_area = new uint8_t[SH4_OC_RAM_AREA_SIZE];

    sh4_ocache_clear(ocache);
}

void sh4_ocache_cleanup(struct sh4_ocache *ocache) {
    free(ocache->oc_ram_area);
}

void sh4_ocache_clear(struct sh4_ocache *ocache) {
    memset(ocache->oc_ram_area, 0, sizeof(uint8_t) * SH4_OC_RAM_AREA_SIZE);
}

void sh4_ocache_do_write_ora(Sh4 *sh4, void const *dat,
                             addr32_t paddr, unsigned len) {
    void *addr = sh4_ocache_get_ora_ram_addr(sh4, paddr);
    memcpy(addr, dat, len);
}

void sh4_ocache_do_read_ora(Sh4 *sh4, void *dat, addr32_t paddr, unsigned len) {
    void *addr = sh4_ocache_get_ora_ram_addr(sh4, paddr);
    memcpy(dat, addr, len);
}

void *sh4_ocache_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr) {
    addr32_t area_offset = paddr & 0xfff;
    addr32_t area_start;
    addr32_t mask;
    if (sh4->reg[SH4_REG_CCR] & SH4_CCR_OIX_MASK)
        mask = 1 << 25;
    else
        mask = 1 << 13;
    if (paddr & mask)
        area_start = SH4_OC_RAM_AREA_SIZE >> 1;
    else
        area_start = 0;
    return sh4->ocache.oc_ram_area + area_start + area_offset;
}
