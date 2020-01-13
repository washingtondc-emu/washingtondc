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

#include <string.h>

#include "pvr2.h"
#include "washdc/error.h"
#include "mem_code.h"
#include "washdc/MemoryMap.h"
#include "pvr2_reg.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"

static uint8_t pvr2_tex_mem_area32_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX32_FIRST + sizeof(uint8_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX32_FIRST + sizeof(uint8_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_32bit_read8(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area32_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX32_FIRST, sizeof(val));
    pvr2_tex_mem_32bit_write8(&pvr2->mem, addr, val);
}

static uint16_t pvr2_tex_mem_area32_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX32_FIRST + sizeof(uint16_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX32_FIRST + sizeof(uint16_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_32bit_read16(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area32_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX32_FIRST, sizeof(val));
    pvr2_tex_mem_32bit_write16(&pvr2->mem, addr, val);
}

static uint32_t pvr2_tex_mem_area32_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX32_FIRST + sizeof(uint32_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX32_FIRST + sizeof(uint32_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_32bit_read32(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area32_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX32_FIRST, sizeof(val));
    pvr2_tex_mem_32bit_write32(&pvr2->mem, addr, val);
}

static float pvr2_tex_mem_area32_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = pvr2_tex_mem_area32_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

static void pvr2_tex_mem_area32_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_tex_mem_area32_write_32(addr, tmp, ctxt);
}

static double pvr2_tex_mem_area32_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void pvr2_tex_mem_area32_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX32_FIRST, sizeof(val));
    pvr2_tex_mem_32bit_write_double(&pvr2->mem, addr, val);
}

static uint8_t pvr2_tex_mem_area64_read_8(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX64_FIRST + sizeof(uint8_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX64_FIRST + sizeof(uint8_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_64bit_read8(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area64_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));

    pvr2_tex_mem_64bit_write8(&pvr2->mem, addr, val);
}

static uint16_t pvr2_tex_mem_area64_read_16(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX64_FIRST + sizeof(uint16_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX64_FIRST + sizeof(uint16_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_64bit_read16(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area64_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));

    pvr2_tex_mem_64bit_write16(&pvr2->mem, addr, val);
}

static uint32_t pvr2_tex_mem_area64_read_32(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    /*
     * TODO: don't call framebuffer_sync_from_host_maybe if addr is beyond the
     * end of the framebuffer
     */
    if ((addr + ADDR_TEX64_FIRST + sizeof(uint32_t)) >= get_fb_w_sof1(pvr2) ||
        (addr + ADDR_TEX64_FIRST + sizeof(uint32_t)) >= get_fb_w_sof2(pvr2))
        framebuffer_sync_from_host_maybe();

    return pvr2_tex_mem_64bit_read32(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area64_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));

    pvr2_tex_mem_64bit_write32(&pvr2->mem, addr, val);
}

static float pvr2_tex_mem_area64_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = pvr2_tex_mem_area64_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

static void pvr2_tex_mem_area64_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_tex_mem_area64_write_32(addr, tmp, ctxt);
}

static double pvr2_tex_mem_area64_read_double(addr32_t addr, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    return pvr2_tex_mem_64bit_read_double(&pvr2->mem, addr);
}

static void pvr2_tex_mem_area64_write_double(addr32_t addr, double val, void *ctxt) {
    struct pvr2 *pvr2 = (struct pvr2*)ctxt;

    pvr2_framebuffer_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));
    pvr2_tex_cache_notify_write(pvr2, addr + ADDR_TEX64_FIRST, sizeof(val));

    pvr2_tex_mem_64bit_write_double(&pvr2->mem, addr, val);
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
