/*******************************************************************************
 *
 * Copyright 2016-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

static inline bool sh4_addr_in_sq_area(uint32_t addr) {
    return (addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL;
}

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
