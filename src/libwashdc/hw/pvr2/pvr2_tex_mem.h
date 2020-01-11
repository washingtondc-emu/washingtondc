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

/*
 * I don't yet understand the 32-bit/64-bit access area dichotomy, so I'm
 * keeping them separated for now.  They might both map th the same memory, I'm
 * just not sure yet.
 */
struct pvr2_tex_mem {
    uint8_t tex32[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
    uint8_t tex64[ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1];
};

#define PVR2_TEX32_MEM_LEN (ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1)
#define PVR2_TEX64_MEM_LEN (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1)

// generic read/write functions for emulator code to use (ie not part of memory map
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

    memcpy(&ret, mem->tex64 + addr, sizeof(ret));
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

    memcpy(&ret, mem->tex64 + addr, sizeof(ret));
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

    memcpy(&ret, mem->tex64 + addr, sizeof(ret));
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

    memcpy(&ret, mem->tex64 + addr, sizeof(ret));
    return ret;
}

inline static void pvr2_tex_mem_64bit_read_raw(struct pvr2_tex_mem *mem,
                                               void *dstp, uint32_t addr,
                                               unsigned n_bytes) {
    void *srcp = mem->tex64 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
}

inline static void pvr2_tex_mem_64bit_write_raw(struct pvr2_tex_mem *mem,
                                                uint32_t addr, void const *srcp,
                                                unsigned n_bytes) {
    void *dstp = mem->tex64 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
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

    memcpy(mem->tex64 + addr, &val, sizeof(val));
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

    memcpy(mem->tex64 + addr, &val, sizeof(val));
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

    memcpy(mem->tex64 + addr, &val, sizeof(val));
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

    memcpy(mem->tex64 + addr, &val, sizeof(val));
}

// 32-bit access area
uint8_t pvr2_tex_mem_area32_read_8(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t pvr2_tex_mem_area32_read_16(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t pvr2_tex_mem_area32_read_32(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_32(addr32_t addr, uint32_t val, void *ctxt);
float pvr2_tex_mem_area32_read_float(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_float(addr32_t addr, float val, void *ctxt);
double pvr2_tex_mem_area32_read_double(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_double(addr32_t addr, double val, void *ctxt);

// 64-bit access area
uint8_t pvr2_tex_mem_area64_read_8(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t pvr2_tex_mem_area64_read_16(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t pvr2_tex_mem_area64_read_32(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_32(addr32_t addr, uint32_t val, void *ctxt);
float pvr2_tex_mem_area64_read_float(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_float(addr32_t addr, float val, void *ctxt);
double pvr2_tex_mem_area64_read_double(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_double(addr32_t addr, double val, void *ctxt);

extern struct memory_interface pvr2_tex_mem_area32_intf,
    pvr2_tex_mem_area64_intf;

#endif
