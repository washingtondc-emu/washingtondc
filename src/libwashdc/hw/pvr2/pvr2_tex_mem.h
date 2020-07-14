/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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
    unsigned offs64;

    if (offs32 >= PVR2_TEX_MEM_BANK_SIZE)
        offs64 = (offs32 - PVR2_TEX_MEM_BANK_SIZE) * 2 + 4;
    else
        offs64 = offs32 * 2;

    return offs64 | (offs & 3);
}

static inline unsigned
pvr2_tex_mem_addr_64_to_32(unsigned offs) {
    unsigned offs64 = offs & 0x007ffffc;
    unsigned offs32;

    if (offs64 % 8)
        offs32 = ((offs64 - 4) / 2) + PVR2_TEX_MEM_BANK_SIZE;
    else
        offs32 = offs64 / 2;

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
