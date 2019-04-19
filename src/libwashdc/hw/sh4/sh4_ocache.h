/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#ifndef SH4_OCACHE_H_
#define SH4_OCACHE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "washdc/types.h"

typedef size_t sh4_ocache_line_t;     // index of cache-line (32-bytes/incrment)
typedef uint32_t sh4_ocache_key_t;

// SH4 16 KB Operand Cache
struct sh4_ocache {
    /*
     * without an operand cache, we need to supply some other area
     * to serve as RAM when the ORA bit is enabled.
     */
    uint8_t *oc_ram_area;

    /*
     * sq[0] through sq[7] correspond to store queue 0
     * sq[8] through sq[15] correspond to store queue 1
     */
    uint32_t sq[16];
};

void sh4_ocache_init(struct sh4_ocache *ocache);
void sh4_ocache_cleanup(struct sh4_ocache *ocache);

void sh4_ocache_clear(struct sh4_ocache *ocache);

/*
 * if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL), then the address is a
 * store queue address.
 */
#define SH4_SQ_AREA_MASK 0xfc000000
#define SH4_SQ_AREA_VAL  0xe0000000

// it is not a mistake that this overlaps with SH4_SQ_SELECT_MASK by 1 bit
#define SH4_SQ_ADDR_MASK 0x03ffffe0

// bit 5 in a store-queue address decides between SQ0 and SQ1
#define SH4_SQ_SELECT_SHIFT 5
#define SH4_SQ_SELECT_MASK (1 << SH4_SQ_SELECT_SHIFT)


void sh4_sq_write(Sh4 *sh4, void const *buf, addr32_t addr, unsigned len);

// read from a store-queue
double sh4_sq_read_double(Sh4 *sh4, addr32_t addr);
float sh4_sq_read_float(Sh4 *sh4, addr32_t addr);
uint32_t sh4_sq_read_32(Sh4 *sh4, addr32_t addr);
uint16_t sh4_sq_read_16(Sh4 *sh4, addr32_t addr);
uint8_t sh4_sq_read_8(Sh4 *sh4, addr32_t addr);

// write to a store-queue
void sh4_sq_write_double(Sh4 *sh4, addr32_t addr, double val);
void sh4_sq_write_float(Sh4 *sh4, addr32_t addr, float val);
void sh4_sq_write_32(Sh4 *sh4, addr32_t addr, uint32_t val);
void sh4_sq_write_16(Sh4 *sh4, addr32_t addr, uint16_t val);
void sh4_sq_write_8(Sh4 *sh4, addr32_t addr, uint8_t val);

// implement the store queues' version of the pref instruction
int sh4_sq_pref(Sh4 *sh4, addr32_t addr);

/*
 * if ((addr & OC_RAM_AREA_MASK) == OC_RAM_AREA_VAL) and the ORA bit is set
 * in CCR, then addr is part of the Operand Cache's RAM area
 */
#define SH4_OC_RAM_AREA_MASK  0xfc000000
#define SH4_OC_RAM_AREA_VAL   0x7c000000

#define SH4_OC_RAM_AREA_FIRST 0x7c000000
#define SH4_OC_RAM_AREA_LAST  0x7fffffff

static inline bool sh4_ocache_in_ram_area(addr32_t addr) {
    return (addr & SH4_OC_RAM_AREA_MASK) == SH4_OC_RAM_AREA_VAL;
}

#define SH4_OC_ADDR_ARRAY_FIRST 0xf4000000
#define SH4_OC_ADDR_ARRAY_LAST  0xf4ffffff

void sh4_ocache_write_addr_array_float(Sh4 *sh4, addr32_t paddr, float val);
void sh4_ocache_write_addr_array_double(Sh4 *sh4, addr32_t paddr, double val);
void sh4_ocache_write_addr_array_32(Sh4 *sh4, addr32_t paddr, uint32_t val);
void sh4_ocache_write_addr_array_16(Sh4 *sh4, addr32_t paddr, uint16_t val);
void sh4_ocache_write_addr_array_8(Sh4 *sh4, addr32_t paddr, uint8_t val);

float sh4_ocache_read_addr_array_float(Sh4 *sh4, addr32_t paddr);
double sh4_ocache_read_addr_array_double(Sh4 *sh4, addr32_t paddr);
uint32_t sh4_ocache_read_addr_array_32(Sh4 *sh4, addr32_t paddr);
uint16_t sh4_ocache_read_addr_array_16(Sh4 *sh4, addr32_t paddr);
uint8_t sh4_ocache_read_addr_array_8(Sh4 *sh4, addr32_t paddr);

struct memory_interface;
extern struct memory_interface sh4_ora_intf;

#endif
