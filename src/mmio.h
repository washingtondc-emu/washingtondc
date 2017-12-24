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

#ifndef MMIO_H_
#define MMIO_H_

#include <stdint.h>
#include <stddef.h>

#include "types.h"

struct mmio_cell;

struct mmio_region {
    // first and last addresses, in terms of bytes
    addr32_t beg;
    size_t len; // length (in terms of 32-bit integers, not bytes)

    /*
     * this points to memory used by the default read/write handlers.
     * custom handlers can use this too, but they don't need to.
     *
     * The length of this array is (end - beg + 1) / sizeof(uint32_t)
     */
    uint32_t * backing;

    /*
     * this array has a length of (end - beg + 1) so that lookups can be done
     * in real-time.
     */
    struct mmio_cell * cells;
};

typedef uint32_t(*mmio_read_handler)(struct mmio_region*,unsigned);
typedef void(*mmio_write_handler)(struct mmio_region*,unsigned,uint32_t);

// read/write handlers that raise an ERROR_UNIMPLEMENTED
uint32_t mmio_read_error(struct mmio_region *region, unsigned idx);
void mmio_write_error(struct mmio_region *region, unsigned idx, uint32_t val);

void mmio_readonly_write_error(struct mmio_region *region,
                               unsigned idx, uint32_t val);
uint32_t mmio_writeonly_read_handler(struct mmio_region *region,
                                     unsigned idx);

struct mmio_cell {
    char const *name;

    mmio_read_handler on_read;
    mmio_write_handler on_write;
};

static inline uint32_t
mmio_read_32(struct mmio_region *region, addr32_t addr) {
    unsigned idx = (addr - region->beg) / sizeof(uint32_t);
    struct mmio_cell *cell = region->cells + idx;
    return cell->on_read(region, idx);
}

static inline void
mmio_write_32(struct mmio_region *region, addr32_t addr, uint32_t val) {
    unsigned idx = (addr - region->beg) / sizeof(uint32_t);
    struct mmio_cell *cell = region->cells + idx;
    cell->on_write(region, idx, val);
}

// idx here is in terms of uint32_t, not uint8_t
uint32_t mmio_warn_read_handler(struct mmio_region *region, unsigned idx);
void mmio_warn_write_handler(struct mmio_region *region,
                             unsigned idx, uint32_t val);

void init_mmio_region(struct mmio_region *region,
                      addr32_t first, addr32_t last);
void init_mmio_cell(struct mmio_region *region, char const *name,
                    addr32_t addr, mmio_read_handler on_read,
                    mmio_write_handler on_write);

void cleanup_mmio_region(struct mmio_region *region);

#endif
