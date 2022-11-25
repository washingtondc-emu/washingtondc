/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020, 2022 snickerbockers
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

#ifndef PVR2_TEX_MEM_H_
#define PVR2_TEX_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

struct pvr2;

struct pvr2_tex_mem {
    uint8_t tex32[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
};

#define PVR2_TEX32_MEM_LEN (ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1)
#define PVR2_TEX64_MEM_LEN (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1)

#define PVR2_TEX_MEM_BANK_SIZE (PVR2_TEX32_MEM_LEN / 2)

static inline unsigned
pvr2_tex_mem_addr_32_to_64(unsigned offs) {
    unsigned offs32 = offs & 0x007ffffc;
    unsigned bank_idx = !!(offs32 >= PVR2_TEX_MEM_BANK_SIZE);
    unsigned offs64 = 2 * (offs32 - PVR2_TEX_MEM_BANK_SIZE * bank_idx +
                           2 * bank_idx);
    return offs64 | (offs & 3);
}

static inline unsigned
pvr2_tex_mem_addr_64_to_32(unsigned offs) {
    unsigned offs64 = offs & 0x007ffffc;
    unsigned bank_idx = !!(offs64 % 8);
    unsigned offs32 = (offs64 - 4 * bank_idx) / 2 +
        bank_idx * PVR2_TEX_MEM_BANK_SIZE;

    return offs32 | (offs & 3);
}

// generic read/write functions for emulator code to use (ie not part of memory map
double
pvr2_tex_mem_32bit_read_double(struct pvr2 *pvr2, unsigned addr);
float
pvr2_tex_mem_32bit_read_float(struct pvr2 *pvr2, unsigned addr);
uint32_t
pvr2_tex_mem_32bit_read32(struct pvr2 *pvr2, unsigned addr);
uint16_t
pvr2_tex_mem_32bit_read16(struct pvr2 *pvr2, unsigned addr);
uint8_t
pvr2_tex_mem_32bit_read8(struct pvr2 *pvr2, unsigned addr);
void
pvr2_tex_mem_32bit_read_raw(struct pvr2 *pvr2,
                            void *dstp, uint32_t addr,
                            unsigned n_bytes);
void
pvr2_tex_mem_32bit_write_float(struct pvr2 *pvr2,
                               unsigned addr, float val);
void
pvr2_tex_mem_32bit_write32(struct pvr2 *pvr2,
                           unsigned addr, uint32_t val);
void
pvr2_tex_mem_32bit_write16(struct pvr2 *pvr2,
                           unsigned addr, uint16_t val);
void
pvr2_tex_mem_32bit_write8(struct pvr2 *pvr2,
                          unsigned addr, uint8_t val);

void pvr2_tex_mem_32bit_write_raw(struct pvr2 *pvr2,
                                  uint32_t addr, void const *srcp,
                                  unsigned n_bytes);

double
pvr2_tex_mem_64bit_read_double(struct pvr2 *pvr2, unsigned addr);
float
pvr2_tex_mem_64bit_read_float(struct pvr2 *pvr2, unsigned addr);
uint32_t
pvr2_tex_mem_64bit_read32(struct pvr2 *pvr2, unsigned addr);
uint16_t
pvr2_tex_mem_64bit_read16(struct pvr2 *pvr2, unsigned addr);
uint8_t
pvr2_tex_mem_64bit_read8(struct pvr2 *pvr2, unsigned addr);

void pvr2_tex_mem_64bit_read_raw(struct pvr2 *pvr2,
                                 void *dstp, uint32_t addr,
                                 unsigned n_bytes);
void
pvr2_tex_mem_64bit_read_dwords(struct pvr2 *pvr2,
                               uint32_t *dstp, uint32_t addr,
                               unsigned n_dwords);

void pvr2_tex_mem_64bit_write_raw(struct pvr2 *pvr2,
                                  uint32_t addr, void const *srcp,
                                  unsigned n_bytes);
void pvr2_tex_mem_64bit_write_dwords(struct pvr2 *pvr2,
                                     uint32_t addr, uint32_t const *srcp,
                                     unsigned n_dwords);
void
pvr2_tex_mem_64bit_write_double(struct pvr2 *pvr2,
                                unsigned addr, double val);
void
pvr2_tex_mem_64bit_write_float(struct pvr2 *pvr2,
                               unsigned addr, float val);
void
pvr2_tex_mem_64bit_write32(struct pvr2 *pvr2,
                           unsigned addr, uint32_t val);
void
pvr2_tex_mem_64bit_write16(struct pvr2 *pvr2,
                           unsigned addr, uint16_t val);
void
pvr2_tex_mem_64bit_write8(struct pvr2 *pvr2,
                          unsigned addr, uint8_t val);

extern struct memory_interface pvr2_tex_mem_area32_intf,
    pvr2_tex_mem_area64_intf, pvr2_tex_mem_unused_intf;

#endif
