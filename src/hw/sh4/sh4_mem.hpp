/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef SH4_MEM_HPP_
#define SH4_MEM_HPP_

#include <boost/static_assert.hpp>

enum VirtMemArea {
    SH4_AREA_P0 = 0,
    SH4_AREA_P1,
    SH4_AREA_P2,
    SH4_AREA_P3,
    SH4_AREA_P4
};

// Physical memory aread boundaries
static const size_t SH4_AREA_P0_FIRST = 0x00000000;
static const size_t SH4_AREA_P0_LAST  = 0x7fffffff;
static const size_t SH4_AREA_P1_FIRST = 0x80000000;
static const size_t SH4_AREA_P1_LAST  = 0x9fffffff;
static const size_t SH4_AREA_P2_FIRST = 0xa0000000;
static const size_t SH4_AREA_P2_LAST  = 0xbfffffff;
static const size_t SH4_AREA_P3_FIRST = 0xc0000000;
static const size_t SH4_AREA_P3_LAST  = 0xdfffffff;
static const size_t SH4_AREA_P4_FIRST = 0xe0000000;
static const size_t SH4_AREA_P4_LAST  = 0xffffffff;

/*
 * SH4_P4_REGSTART is the addr of the first memory-mapped
 *     register in area 7
 * SH4_P4_REGEND is the first addr *after* the last memory-mapped
 *     register in the p4 area.
 * SH4_AREA7_REGSTART is the addr of the first memory-mapped
 *     register in area 7
 * SH4_AREA7_REGEND is the first addr *after* the last memory-mapped
 *     register in area 7
 */
static const size_t SH4_P4_REGSTART = 0xff000000;
static const size_t SH4_P4_REGEND = 0xfff00008;
static const size_t SH4_AREA7_REGSTART = 0x1f000000;
static const size_t SH4_AREA7_REGEND = 0x1ff00008;
BOOST_STATIC_ASSERT((SH4_P4_REGEND - SH4_P4_REGSTART) ==
                    (SH4_AREA7_REGEND - SH4_AREA7_REGSTART));

#ifndef ENABLE_SH4_OCACHE
/* constants needed for opcache as ram */
static const size_t SH4_LONGS_PER_OP_CACHE_LINE = 8;
static const size_t SH4_OP_CACHE_LINE_SIZE = SH4_LONGS_PER_OP_CACHE_LINE * 4;
static const size_t SH4_OC_RAM_AREA_SIZE = 8 * 1024;
#endif

/*
 * From within the CPU, these functions should be called instead of
 * the memory's read/write functions because these implement the MMU
 * functionality.  In the event of a failure, these functions will set the
 * appropriate CPU flags for an exception and return non-zero.  On success
 * they will return zero.
 */
int sh4_write_mem(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len);
int sh4_read_mem(Sh4 *sh4, void *dat, addr32_t addr, unsigned len);

int sh4_read_inst(Sh4 *sh4, inst_t *out, addr32_t addr);

enum VirtMemArea sh4_get_mem_area(addr32_t addr);

void *sh4_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr);

/*
 * read to/write from the operand cache's RAM-space in situations where we
 * don't actually have a real operand cache available.  It is up to the
 * caller to make sure that the operand cache is enabled (OCE in the CCR),
 * that the Operand Cache's RAM switch is enabled (ORA in the CCR) and that
 * paddr lies within the Operand Cache RAM mapping (in_oc_ram_area returns
 * true).
 */
void sh4_do_write_ora(Sh4 *sh4, void const *dat, addr32_t paddr, unsigned len);
void sh4_do_read_ora(Sh4 *sh4, void *dat, addr32_t paddr, unsigned len);

/*
 * generally you'll call these functions through do_read_mem/do_write_mem
 * instead of calling these functions directly
 */
int sh4_do_read_p4(Sh4 *sh4, void *dat, addr32_t addr, unsigned len);
int sh4_do_write_p4(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len);

#endif
