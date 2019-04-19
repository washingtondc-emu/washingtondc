/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <string.h>

#include "pvr2.h"
#include "washdc/error.h"
#include "mem_code.h"
#include "washdc/MemoryMap.h"
#include "pvr2_reg.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"

uint8_t pvr2_tex_mem_area32_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(uint8_t)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(uint8_t)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint8_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint8_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2->mem.tex32[addr - ADDR_TEX32_FIRST];
}

void pvr2_tex_mem_area32_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if ((addr < ADDR_TEX32_FIRST) || (addr > ADDR_TEX32_LAST) ||
        (addr > ADDR_TEX32_LAST) || (addr < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));

    pvr2->mem.tex32[addr - ADDR_TEX32_FIRST] = val;
}

uint16_t pvr2_tex_mem_area32_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(uint16_t)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(uint16_t)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint16_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint16_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return ((uint16_t*)pvr2->mem.tex32)[(addr - ADDR_TEX32_FIRST) / 2];
}

void pvr2_tex_mem_area32_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(uint16_t)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(uint16_t)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));

    ((uint16_t*)pvr2->mem.tex32)[(addr - ADDR_TEX32_FIRST) / 2] = val;
}

uint32_t pvr2_tex_mem_area32_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(uint32_t)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(uint32_t)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint32_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint32_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return ((uint32_t*)pvr2->mem.tex32)[(addr - ADDR_TEX32_FIRST) / 4];
}

void pvr2_tex_mem_area32_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(uint32_t)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(uint32_t)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));

    ((uint32_t*)pvr2->mem.tex32)[(addr - ADDR_TEX32_FIRST) / 4] = val;
}

float pvr2_tex_mem_area32_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = pvr2_tex_mem_area32_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_tex_mem_area32_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_tex_mem_area32_write_32(addr, tmp, ctxt);
}

double pvr2_tex_mem_area32_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_tex_mem_area32_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + sizeof(val)) > ADDR_TEX32_LAST) ||
        ((addr - 1 + sizeof(val)) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));
    ((double*)pvr2->mem.tex32)[(addr - ADDR_TEX32_FIRST) / sizeof(val)] = val;
}

uint8_t pvr2_tex_mem_area64_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        (addr > ADDR_TEX64_LAST) || (addr < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint8_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint8_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2->mem.tex64[addr - ADDR_TEX64_FIRST];
}

void pvr2_tex_mem_area64_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        (addr > ADDR_TEX64_LAST) || (addr < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr, sizeof(val));

    ((uint8_t*)pvr2->mem.tex64)[addr - ADDR_TEX64_FIRST] = val;
}

uint16_t pvr2_tex_mem_area64_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(uint16_t)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(uint16_t)) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint16_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint16_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return ((uint16_t*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / 2];
}

void pvr2_tex_mem_area64_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(uint16_t)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(uint16_t)) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr, sizeof(val));

    ((uint16_t*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / 2] = val;
}

uint32_t pvr2_tex_mem_area64_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(uint32_t)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(uint32_t)) < ADDR_TEX64_FIRST)) {
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        error_set_feature("out-of-bounds PVR2 texture memory read");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + sizeof(uint32_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + sizeof(uint32_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return ((uint32_t*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / 4];
}

void pvr2_tex_mem_area64_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(uint32_t)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(uint32_t)) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr, sizeof(val));

    ((uint32_t*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / 4] = val;
}

float pvr2_tex_mem_area64_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = pvr2_tex_mem_area64_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_tex_mem_area64_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_tex_mem_area64_write_32(addr, tmp, ctxt);
}

double pvr2_tex_mem_area64_read_double(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(double)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(double)) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        error_set_address(addr);
        error_set_length(sizeof(double));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    return ((double*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / sizeof(double)];
}

void pvr2_tex_mem_area64_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + sizeof(val)) > ADDR_TEX64_LAST) ||
        ((addr - 1 + sizeof(val)) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory write");
        error_set_address(addr);
        error_set_length(sizeof(val));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    pvr2_framebuffer_notify_write(pvr2, addr, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr, sizeof(val));

    ((double*)pvr2->mem.tex64)[(addr - ADDR_TEX64_FIRST) / sizeof(val)] = val;
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
