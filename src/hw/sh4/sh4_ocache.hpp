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

/*
 * Ocache - we don't actually emulate the Sh4's operand cache,
 * but we do need to implement the mode wherein the operand
 * cache is used as RAM.
 */

#ifndef SH4_OCACHE_HPP_
#define SH4_OCACHE_HPP_

#include <boost/cstdint.hpp>

#include "types.hpp"
#include "MemoryMap.hpp"

typedef size_t sh4_ocache_line_t;     // index of cache-line (32-bytes/incrment)
typedef uint32_t sh4_ocache_key_t;

// SH4 16 KB Operand Cache
struct sh4_ocache {
    /*
     * without an operand cache, we need to supply some other area
     * to serve as RAM when the ORA bit is enabled.
     */
    uint8_t *oc_ram_area;
};

void sh4_ocache_init(struct sh4_ocache *ocache);
void sh4_ocache_cleanup(struct sh4_ocache *ocache);

void sh4_ocache_clear(struct sh4_ocache *ocache);

void *sh4_ocache_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr);

/*
 * read to/write from the operand cache's RAM-space in situations where we
 * don't actually have a real operand cache available.  It is up to the
 * caller to make sure that the operand cache is enabled (OCE in the CCR),
 * that the Operand Cache's RAM switch is enabled (ORA in the CCR) and that
 * paddr lies within the Operand Cache RAM mapping (in_oc_ram_area returns
 * true).
 */
void sh4_ocache_do_write_ora(Sh4 *sh4, void const *dat, addr32_t paddr, unsigned len);
void sh4_ocache_do_read_ora(Sh4 *sh4, void *dat, addr32_t paddr, unsigned len);

/*
 * if ((addr & OC_RAM_AREA_MASK) == OC_RAM_AREA_VAL) and the ORA bit is set
 * in CCR, then addr is part of the Operand Cache's RAM area
 */
static const addr32_t SH4_OC_RAM_AREA_MASK = 0xfc000000;
static const addr32_t SH4_OC_RAM_AREA_VAL = 0x7c000000;
static inline bool sh4_ocache_in_ram_area(addr32_t addr) {
    return (addr & SH4_OC_RAM_AREA_MASK) == SH4_OC_RAM_AREA_VAL;
}

#endif
