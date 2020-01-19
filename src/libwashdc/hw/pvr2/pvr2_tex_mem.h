/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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

struct pvr2_tex_mem {
    uint8_t tex32[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
};

#define PVR2_TEX32_MEM_LEN (ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1)
#define PVR2_TEX64_MEM_LEN (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1)

#define PVR2_TEX_MEM_BANK_SIZE (PVR2_TEX32_MEM_LEN / 2)

/* static inline unsigned */
/* pvr2_tex_mem_addr_32_to_64(unsigned offs) { */
/*     unsigned offs32 = offs & 0x007ffffc; */
/*     unsigned offs64; */

/*     if (offs32 >= PVR2_TEX_MEM_BANK_SIZE) */
/*         offs64 = (offs32 - PVR2_TEX_MEM_BANK_SIZE) * 2 + 4; */
/*     else */
/*         offs64 = offs32 * 2; */

/*     return offs64 | (offs & 3); */
/* } */

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

inline static void
pvr2_tex_mem_64bit_write8(struct pvr2_tex_mem *mem,
                          unsigned addr, uint8_t val);

// generic read/write functions for emulator code to use (ie not part of memory map
inline static double
pvr2_tex_mem_32bit_read_double(struct pvr2_tex_mem *mem, unsigned addr) {
    double ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, mem->tex32 + addr, sizeof(ret));
    return ret;
}

inline static float
pvr2_tex_mem_32bit_read_float(struct pvr2_tex_mem *mem, unsigned addr) {
    float ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, mem->tex32 + addr, sizeof(ret));
    return ret;
}

inline static uint32_t
pvr2_tex_mem_32bit_read32(struct pvr2_tex_mem *mem, unsigned addr) {
    uint32_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, mem->tex32 + addr, sizeof(ret));
    return ret;
}

inline static uint16_t
pvr2_tex_mem_32bit_read16(struct pvr2_tex_mem *mem, unsigned addr) {
    uint16_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, mem->tex32 + addr, sizeof(ret));
    return ret;
}

inline static uint8_t
pvr2_tex_mem_32bit_read8(struct pvr2_tex_mem *mem, unsigned addr) {
    uint8_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, mem->tex32 + addr, sizeof(ret));
    return ret;
}

inline static void pvr2_tex_mem_32bit_read_raw(struct pvr2_tex_mem *mem,
                                               void *dstp, uint32_t addr,
                                               unsigned n_bytes) {
    void *srcp = mem->tex32 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX32_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
}

inline static void
pvr2_tex_mem_32bit_write_double(struct pvr2_tex_mem *mem,
                                unsigned addr, double val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(mem->tex32 + addr, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_32bit_write_float(struct pvr2_tex_mem *mem,
                               unsigned addr, float val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(mem->tex32 + addr, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_32bit_write32(struct pvr2_tex_mem *mem,
                           unsigned addr, uint32_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(mem->tex32 + addr, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_32bit_write16(struct pvr2_tex_mem *mem,
                           unsigned addr, uint16_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(mem->tex32 + addr, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_32bit_write8(struct pvr2_tex_mem *mem,
                          unsigned addr, uint8_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(mem->tex32 + addr, &val, sizeof(val));
}

inline static void pvr2_tex_mem_32bit_write_raw(struct pvr2_tex_mem *mem,
                                                uint32_t addr, void const *srcp,
                                                unsigned n_bytes) {
    void *dstp = mem->tex32 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX32_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
}

inline static double
pvr2_tex_mem_64bit_read_double(struct pvr2_tex_mem *mem, unsigned addr) {
    double ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs1 = pvr2_tex_mem_addr_64_to_32(addr);
    unsigned offs2 = pvr2_tex_mem_addr_64_to_32(addr + 4);

    memcpy(&ret, mem->tex32 + offs1, sizeof(ret)/2);
    memcpy(((char*)&ret) + sizeof(ret)/2, mem->tex32 + offs2, sizeof(ret)/2);
    return ret;
}

inline static float
pvr2_tex_mem_64bit_read_float(struct pvr2_tex_mem *mem, unsigned addr) {
    float ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(&ret, mem->tex32 + offs, sizeof(ret));
    return ret;
}

inline static uint32_t
pvr2_tex_mem_64bit_read32(struct pvr2_tex_mem *mem, unsigned addr) {
    uint32_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(&ret, mem->tex32 + offs, sizeof(ret));
    return ret;
}

inline static uint16_t
pvr2_tex_mem_64bit_read16(struct pvr2_tex_mem *mem, unsigned addr) {
    uint16_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(&ret, mem->tex32 + offs, sizeof(ret));
    return ret;
}

inline static uint8_t
pvr2_tex_mem_64bit_read8(struct pvr2_tex_mem *mem, unsigned addr) {
    uint8_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(&ret, mem->tex32 + offs, sizeof(ret));
    return ret;
}

inline static void pvr2_tex_mem_64bit_read_raw(struct pvr2_tex_mem *mem,
                                               void *dstp, uint32_t addr,
                                               unsigned n_bytes) {
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    uint8_t *dst = (uint8_t*)dstp;
    while (n_bytes) {
        uint8_t val = pvr2_tex_mem_64bit_read8(mem, addr);
        memcpy(dst, &val, sizeof(uint8_t));

        --n_bytes;
        ++addr;
        ++dst;
    }
}

inline static void pvr2_tex_mem_64bit_write_raw(struct pvr2_tex_mem *mem,
                                                uint32_t addr, void const *srcp,
                                                unsigned n_bytes) {
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    uint8_t const *src = (uint8_t*)srcp;
    while (n_bytes) {
        uint8_t val;
        memcpy(&val, src, sizeof(val));
        pvr2_tex_mem_64bit_write8(mem, addr, val);

        --n_bytes;
        ++addr;
        ++src;
    }
}

inline static void
pvr2_tex_mem_64bit_write_double(struct pvr2_tex_mem *mem,
                                unsigned addr, double val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs1 = pvr2_tex_mem_addr_64_to_32(addr);
    unsigned offs2 = pvr2_tex_mem_addr_64_to_32(addr + 4);

    memcpy(mem->tex32 + offs1, &val, sizeof(val)/2);
    memcpy(mem->tex32 + offs2,
           ((char*)&val) + sizeof(val)/2, sizeof(val)/2);
}

inline static void
pvr2_tex_mem_64bit_write_float(struct pvr2_tex_mem *mem,
                               unsigned addr, float val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(mem->tex32 + offs, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_64bit_write32(struct pvr2_tex_mem *mem,
                           unsigned addr, uint32_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(mem->tex32 + offs, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_64bit_write16(struct pvr2_tex_mem *mem,
                           unsigned addr, uint16_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(mem->tex32 + offs, &val, sizeof(val));
}

inline static void
pvr2_tex_mem_64bit_write8(struct pvr2_tex_mem *mem,
                          unsigned addr, uint8_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    memcpy(mem->tex32 + offs, &val, sizeof(val));
}

extern struct memory_interface pvr2_tex_mem_area32_intf,
    pvr2_tex_mem_area64_intf, pvr2_tex_mem_unused_intf;

#endif
