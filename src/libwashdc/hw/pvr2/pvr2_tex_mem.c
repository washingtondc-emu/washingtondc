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

/*
 * texture memory.
 *
 * VRAM can be accessed through one of two areas: the 64-bit area or
 * the 32-bit area. Both areas are backed by the same physical
 * memory.  Physical VRAM consists of two separate 4MB modules, and
 * the difference between the 32-bit and 64-bit areas is in how those
 * two modules are mapped to addresses. The 32-bit area is used to
 * store the framebuffer, and the 64-bit area is used to store textures.
 *
 * Although these two areas are referred to as "32-bit" and "64-bit"
 * areas, there is no restriction on what sizes may be used for read and
 * write operations. The names reflect the fact that the 64-bit area's
 * interleaving allows it to be accessed faster than the 32-bit area
 * since each consecutive set of four bytes comes from alternating RAM modules.
 *
 * The 32-bit area allows for sequential access across all 8MB of VRAM,
 * with the entirety of the second 4MB module placed after the first 4MB
 * module. The 64-bit memory interleaves the first 4MB module with the
 * second 4MB module, alternating between the two modules every four
 * bytes. So, every second set of four bytes in 64-bit area is offset
 * by 4MB in the 32-bit area as illustrated in the table below.
 */

#include <string.h>

#include "pvr2.h"
#include "washdc/error.h"
#include "mem_code.h"
#include "washdc/MemoryMap.h"
#include "pvr2_reg.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"

static inline void
pvr2_tex_mem_sync_fb(struct pvr2 *pvr2, uint32_t addr_32bit, size_t n_bytes) {
    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    uint32_t sof1 = get_fb_w_sof1(pvr2);
    uint32_t sof2 = get_fb_w_sof2(pvr2);
    if ((addr_32bit + ADDR_TEX32_FIRST + n_bytes) >= sof1 ||
        (addr_32bit + ADDR_TEX32_FIRST + n_bytes) >= sof2)
        framebuffer_sync_from_host_maybe();
}

static inline void
pvr2_tex_mem_notify_writes(struct pvr2 *pvr2,
                           uint32_t addr_32bit, size_t n_bytes) {
    pvr2_tex_mem_sync_fb(pvr2, addr_32bit, n_bytes);
    pvr2_framebuffer_notify_write(pvr2, addr_32bit, n_bytes);

    /*
     * TODO: calling pvr2_tex_mem_addr_32_to_64 is suboptimal because if this
     * function is being called from a 64-bit write handler then we already
     * know the 64-bit address.
     */
    pvr2_tex_cache_notify_write(pvr2,
                                pvr2_tex_mem_addr_32_to_64(addr_32bit),
                                n_bytes);
}

double
pvr2_tex_mem_32bit_read_double(struct pvr2 *pvr2, unsigned addr) {
    double ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    memcpy(&ret, pvr2->mem.tex32 + addr, sizeof(ret));
    return ret;
}

float
pvr2_tex_mem_32bit_read_float(struct pvr2 *pvr2, unsigned addr) {
    float ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_sync_fb(pvr2, addr, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + addr, sizeof(ret));
    return ret;
}

uint32_t
pvr2_tex_mem_32bit_read32(struct pvr2 *pvr2, unsigned addr) {
    uint32_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_sync_fb(pvr2, addr, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + addr, sizeof(ret));
    return ret;
}

uint16_t
pvr2_tex_mem_32bit_read16(struct pvr2 *pvr2, unsigned addr) {
    uint16_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_sync_fb(pvr2, addr, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + addr, sizeof(ret));
    return ret;
}

uint8_t
pvr2_tex_mem_32bit_read8(struct pvr2 *pvr2, unsigned addr) {
    uint8_t ret;

    if (addr > PVR2_TEX32_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_sync_fb(pvr2, addr, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + addr, sizeof(ret));
    return ret;
}

void pvr2_tex_mem_32bit_read_raw(struct pvr2 *pvr2,
                                 void *dstp, uint32_t addr,
                                 unsigned n_bytes) {
    void *srcp = pvr2->mem.tex32 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX32_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_sync_fb(pvr2, addr, n_bytes);
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
}

static uint8_t pvr2_tex_mem_area32_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    return pvr2_tex_mem_32bit_read8(pvr2, addr);
}

void
pvr2_tex_mem_32bit_write_double(struct pvr2 *pvr2,
                                unsigned addr, double val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_notify_writes(pvr2, addr, sizeof(val));
    memcpy(pvr2->mem.tex32 + addr, &val, sizeof(val));
}

void
pvr2_tex_mem_32bit_write_float(struct pvr2 *pvr2,
                               unsigned addr, float val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_notify_writes(pvr2, addr, sizeof(val));
    memcpy(pvr2->mem.tex32 + addr, &val, sizeof(val));
}

void
pvr2_tex_mem_32bit_write32(struct pvr2 *pvr2,
                           unsigned addr, uint32_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_notify_writes(pvr2, addr, sizeof(val));
    memcpy(pvr2->mem.tex32 + addr, &val, sizeof(val));
}

void
pvr2_tex_mem_32bit_write16(struct pvr2 *pvr2,
                           unsigned addr, uint16_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_notify_writes(pvr2, addr, sizeof(val));
    memcpy(pvr2->mem.tex32 + addr, &val, sizeof(val));
}

void
pvr2_tex_mem_32bit_write8(struct pvr2 *pvr2,
                          unsigned addr, uint8_t val) {
    if (addr > PVR2_TEX32_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    pvr2_tex_mem_notify_writes(pvr2, addr, sizeof(val));
    memcpy(pvr2->mem.tex32 + addr, &val, sizeof(val));
}

void pvr2_tex_mem_32bit_write_raw(struct pvr2 *pvr2,
                                  uint32_t addr, void const *srcp,
                                  unsigned n_bytes) {
    void *dstp = pvr2->mem.tex32 + addr;
    if (addr + (n_bytes - 1) >= PVR2_TEX32_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    pvr2_tex_mem_notify_writes(pvr2, addr, n_bytes);
    memcpy(dstp, srcp, n_bytes * sizeof(uint8_t));
}

double
pvr2_tex_mem_64bit_read_double(struct pvr2 *pvr2, unsigned addr) {
    double ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs1 = pvr2_tex_mem_addr_64_to_32(addr);
    unsigned offs2 = pvr2_tex_mem_addr_64_to_32(addr + 4);

    /*
     * TODO: calling pvr2_tex_mem_sync_fb twice is suboptimal
     * because we might end up syncing the framebuffe twice
     */
    pvr2_tex_mem_sync_fb(pvr2, offs1, sizeof(ret)/2);
    pvr2_tex_mem_sync_fb(pvr2, offs2, sizeof(ret)/2);
    memcpy(&ret, pvr2->mem.tex32 + offs1, sizeof(ret)/2);
    memcpy(((char*)&ret) + sizeof(ret)/2, pvr2->mem.tex32 + offs2, sizeof(ret)/2);
    return ret;
}

float
pvr2_tex_mem_64bit_read_float(struct pvr2 *pvr2, unsigned addr) {
    float ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_sync_fb(pvr2, offs, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + offs, sizeof(ret));
    return ret;
}

uint32_t
pvr2_tex_mem_64bit_read32(struct pvr2 *pvr2, unsigned addr) {
    uint32_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_sync_fb(pvr2, offs, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + offs, sizeof(ret));
    return ret;
}

uint16_t
pvr2_tex_mem_64bit_read16(struct pvr2 *pvr2, unsigned addr) {
    uint16_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_sync_fb(pvr2, offs, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + offs, sizeof(ret));
    return ret;
}

uint8_t
pvr2_tex_mem_64bit_read8(struct pvr2 *pvr2, unsigned addr) {
    uint8_t ret;

    if (addr > PVR2_TEX64_MEM_LEN - sizeof(ret)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(ret));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_sync_fb(pvr2, offs, sizeof(ret));
    memcpy(&ret, pvr2->mem.tex32 + offs, sizeof(ret));
    return ret;
}

void pvr2_tex_mem_64bit_read_raw(struct pvr2 *pvr2,
                                 void *dstp, uint32_t addr,
                                 unsigned n_bytes) {
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    char *dst = (char*)dstp;
    while (n_bytes) {
        uint8_t val = pvr2_tex_mem_64bit_read8(pvr2, addr);
        memcpy(dst, &val, sizeof(uint8_t));

        --n_bytes;
        ++addr;
        dst += sizeof(uint8_t);
    }
}

void
pvr2_tex_mem_64bit_read_dwords(struct pvr2 *pvr2,
                               uint32_t *dstp, uint32_t addr,
                               unsigned n_dwords) {
    unsigned n_bytes = 4 * n_dwords;
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    while (n_dwords--) {
        *dstp++ = pvr2_tex_mem_64bit_read32(pvr2, addr);
        addr += 4;
    }
}


void pvr2_tex_mem_64bit_write_raw(struct pvr2 *pvr2,
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
        pvr2_tex_mem_64bit_write8(pvr2, addr, val);

        --n_bytes;
        ++addr;
        ++src;
    }
}

void pvr2_tex_mem_64bit_write_dwords(struct pvr2 *pvr2,
                                     uint32_t addr, uint32_t const *srcp,
                                     unsigned n_dwords) {
    unsigned n_bytes = 4 * n_dwords;
    if (addr + (n_bytes - 1) >= PVR2_TEX64_MEM_LEN) {
        error_set_address(addr);
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    while (n_dwords--) {
        pvr2_tex_mem_64bit_write32(pvr2, addr, *srcp++);
        addr += 4;
    }
}

void
pvr2_tex_mem_64bit_write_double(struct pvr2 *pvr2,
                                unsigned addr, double val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs1 = pvr2_tex_mem_addr_64_to_32(addr);
    unsigned offs2 = pvr2_tex_mem_addr_64_to_32(addr + 4);

    /*
     * TODO: this is suboptimal because it could call
     * framebuffer_sync_from_host_maybe twice
     */
    pvr2_tex_mem_notify_writes(pvr2, offs1, sizeof(val));
    pvr2_tex_mem_notify_writes(pvr2, offs2, sizeof(val));

    memcpy(pvr2->mem.tex32 + offs1, &val, sizeof(val)/2);
    memcpy(pvr2->mem.tex32 + offs2,
           ((char*)&val) + sizeof(val)/2, sizeof(val)/2);
}

void
pvr2_tex_mem_64bit_write_float(struct pvr2 *pvr2,
                               unsigned addr, float val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_notify_writes(pvr2, offs, sizeof(val));
    memcpy(pvr2->mem.tex32 + offs, &val, sizeof(val));
}

void
pvr2_tex_mem_64bit_write32(struct pvr2 *pvr2,
                           unsigned addr, uint32_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_notify_writes(pvr2, offs, sizeof(val));
    memcpy(pvr2->mem.tex32 + offs, &val, sizeof(val));
}

void
pvr2_tex_mem_64bit_write16(struct pvr2 *pvr2,
                           unsigned addr, uint16_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_notify_writes(pvr2, offs, sizeof(val));
    memcpy(pvr2->mem.tex32 + offs, &val, sizeof(val));
}

void
pvr2_tex_mem_64bit_write8(struct pvr2 *pvr2,
                          unsigned addr, uint8_t val) {
    if (addr > PVR2_TEX64_MEM_LEN - sizeof(val)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned offs = pvr2_tex_mem_addr_64_to_32(addr);
    pvr2_tex_mem_notify_writes(pvr2, offs, sizeof(val));
    memcpy(pvr2->mem.tex32 + offs, &val, sizeof(val));
}

static void pvr2_tex_mem_area32_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_32bit_write8(pvr2, addr, val);
}

static uint16_t pvr2_tex_mem_area32_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_32bit_read16(pvr2, addr);
}

static void pvr2_tex_mem_area32_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_32bit_write16(pvr2, addr, val);
}

static uint32_t pvr2_tex_mem_area32_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_32bit_read32(pvr2, addr);
}

static void pvr2_tex_mem_area32_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_32bit_write32(pvr2, addr, val);
}

static float pvr2_tex_mem_area32_read_float(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_32bit_read_float(pvr2, addr);
}

static void pvr2_tex_mem_area32_write_float(addr32_t addr, float val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_32bit_write_float(pvr2, addr, val);
}

static double pvr2_tex_mem_area32_read_double(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_32bit_read_double(pvr2, addr);
}

static void pvr2_tex_mem_area32_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_32bit_write_double(pvr2, addr, val);
}

static uint8_t pvr2_tex_mem_area64_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_64bit_read8(pvr2, addr);
}

static void pvr2_tex_mem_area64_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_64bit_write8(pvr2, addr, val);
}

static uint16_t pvr2_tex_mem_area64_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_64bit_read16(pvr2, addr);
}

static void pvr2_tex_mem_area64_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_64bit_write16(pvr2, addr, val);
}

static uint32_t pvr2_tex_mem_area64_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_64bit_read32(pvr2, addr);
}

static void pvr2_tex_mem_area64_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_64bit_write32(pvr2, addr, val);
}

static float pvr2_tex_mem_area64_read_float(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_64bit_read_float(pvr2, addr);
}

static void pvr2_tex_mem_area64_write_float(addr32_t addr, float val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_64bit_write_float(pvr2, addr, val);
}

static double pvr2_tex_mem_area64_read_double(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    return pvr2_tex_mem_64bit_read_double(pvr2, addr);
}

static void pvr2_tex_mem_area64_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;
    pvr2_tex_mem_64bit_write_double(pvr2, addr, val);
}

static uint8_t pvr2_tex_mem_unused_read_8(addr32_t addr, void *ctxt) {
    return ~0;
}
static void
pvr2_tex_mem_unused_write_8(addr32_t addr, uint8_t val, void *ctxt) {
}
static uint16_t pvr2_tex_mem_unused_read_16(addr32_t addr, void *ctxt) {
    return ~0;
}
static void
pvr2_tex_mem_unused_write_16(addr32_t addr, uint16_t val, void *ctxt) {
}
static uint32_t pvr2_tex_mem_unused_read_32(addr32_t addr, void *ctxt) {
    return ~0;
}
static void
pvr2_tex_mem_unused_write_32(addr32_t addr, uint32_t val, void *ctxt) {
}
static float pvr2_tex_mem_unused_read_float(addr32_t addr, void *ctxt) {
    float retval;
    memset(&retval, 0xff, sizeof(retval));
    return retval;
}
static void
pvr2_tex_mem_unused_write_float(addr32_t addr, float val, void *ctxt) {
}
static double pvr2_tex_mem_unused_read_double(addr32_t addr, void *ctxt) {
    double retval;
    memset(&retval, 0xff, sizeof(retval));
    return retval;
}
static void
pvr2_tex_mem_unused_write_double(addr32_t addr, double val, void *ctxt) {
}

struct memory_interface pvr2_tex_mem_area32_intf = {
    .readdouble = pvr2_tex_mem_area32_read_double,
    .readfloat = pvr2_tex_mem_area32_read_float,
    .read32 = pvr2_tex_mem_area32_read_32,
    .read16 = pvr2_tex_mem_area32_read_16,
    .read8 = pvr2_tex_mem_area32_read_8,

    .writedouble = pvr2_tex_mem_area32_write_double,
    .writefloat = pvr2_tex_mem_area32_write_float,
    .write32 = pvr2_tex_mem_area32_write_32,
    .write16 = pvr2_tex_mem_area32_write_16,
    .write8 = pvr2_tex_mem_area32_write_8
};

struct memory_interface pvr2_tex_mem_area64_intf = {
    .readdouble = pvr2_tex_mem_area64_read_double,
    .readfloat = pvr2_tex_mem_area64_read_float,
    .read32 = pvr2_tex_mem_area64_read_32,
    .read16 = pvr2_tex_mem_area64_read_16,
    .read8 = pvr2_tex_mem_area64_read_8,

    .writedouble = pvr2_tex_mem_area64_write_double,
    .writefloat = pvr2_tex_mem_area64_write_float,
    .write32 = pvr2_tex_mem_area64_write_32,
    .write16 = pvr2_tex_mem_area64_write_16,
    .write8 = pvr2_tex_mem_area64_write_8
};

struct memory_interface pvr2_tex_mem_unused_intf = {
    .readdouble = pvr2_tex_mem_unused_read_double,
    .readfloat = pvr2_tex_mem_unused_read_float,
    .read32 = pvr2_tex_mem_unused_read_32,
    .read16 = pvr2_tex_mem_unused_read_16,
    .read8 = pvr2_tex_mem_unused_read_8,

    .writedouble = pvr2_tex_mem_unused_write_double,
    .writefloat = pvr2_tex_mem_unused_write_float,
    .write32 = pvr2_tex_mem_unused_write_32,
    .write16 = pvr2_tex_mem_unused_write_16,
    .write8 = pvr2_tex_mem_unused_write_8,
};
