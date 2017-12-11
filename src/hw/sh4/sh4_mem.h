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

#ifndef SH4_MEM_H_
#define SH4_MEM_H_

#include <assert.h>

struct Sh4;

enum VirtMemArea {
    SH4_AREA_P0 = 0,
    SH4_AREA_P1,
    SH4_AREA_P2,
    SH4_AREA_P3,
    SH4_AREA_P4
};

// Physical memory aread boundaries
#define SH4_AREA_P0_FIRST  0x00000000
#define SH4_AREA_P0_LAST   0x7fffffff
#define SH4_AREA_P1_FIRST  0x80000000
#define SH4_AREA_P1_LAST   0x9fffffff
#define SH4_AREA_P2_FIRST  0xa0000000
#define SH4_AREA_P2_LAST   0xbfffffff
#define SH4_AREA_P3_FIRST  0xc0000000
#define SH4_AREA_P3_LAST   0xdfffffff
#define SH4_AREA_P4_FIRST  0xe0000000
#define SH4_AREA_P4_LAST   0xffffffff

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
#define SH4_P4_REGSTART    0xff000000
#define SH4_P4_REGEND      0xfff00008
#define SH4_AREA7_REGSTART 0x1f000000
#define SH4_AREA7_REGEND   0x1ff00008
static_assert((SH4_P4_REGEND - SH4_P4_REGSTART) ==
              (SH4_AREA7_REGEND - SH4_AREA7_REGSTART),
              "AREA7 is not the same size as the P4 area");

/* constants needed for opcache as ram */
#define SH4_LONGS_PER_OP_CACHE_LINE 8
#define SH4_OP_CACHE_LINE_SIZE (SH4_LONGS_PER_OP_CACHE_LINE * 4)
#define SH4_OC_RAM_AREA_SIZE (8 * 1024)

/*
 * From within the CPU, these functions should be called instead of
 * the memory's read/write functions because these implement the MMU
 * functionality.  In the event of a CPU exception, these functions will set the
 * appropriate CPU flags for an exception and return non-zero.  On success
 * they will return zero.
 */
int sh4_write_mem(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len);
int sh4_read_mem(Sh4 *sh4, void *dat, addr32_t addr, unsigned len);


/*
 * same as sh4_write_mem/sh4_read_mem, except they don't automatically raise
 * pending errors and they don't check for watchpoints
 */
int sh4_do_write_mem(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len);
int sh4_do_read_mem(Sh4 *sh4, void *dat, addr32_t addr, unsigned len);

inst_t sh4_read_inst(Sh4 *sh4, addr32_t addr);

/*
 * generally you'll call these functions through do_read_mem/do_write_mem
 * instead of calling these functions directly
 */
int sh4_do_read_p4(Sh4 *sh4, void *dat, addr32_t addr, unsigned len);
int sh4_do_write_p4(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len);

#endif
