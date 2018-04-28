/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <stdio.h>
#include <string.h>

#include "mem_code.h"
#include "MemoryMap.h"
#include "log.h"
#include "mem_areas.h"

#include "aica_common.h"

#define ARM7RST_ADDR 0x00702c00

static float aica_common_read_float(addr32_t addr, void *ctxt);
static void aica_common_write_float(addr32_t addr, float val, void *ctxt);
static double aica_common_read_double(addr32_t addr, void *ctxt);
static void aica_common_write_double(addr32_t addr, double val, void *ctxt);
static uint32_t aica_common_read_32(addr32_t addr, void *ctxt);
static void aica_common_write_32(addr32_t addr, uint32_t val, void *ctxt);
static uint16_t aica_common_read_16(addr32_t addr, void *ctxt);
static void aica_common_write_16(addr32_t addr, uint16_t val, void *ctxt);
static uint8_t aica_common_read_8(addr32_t addr, void *ctxt);
static void aica_common_write_8(addr32_t addr, uint8_t val, void *ctxt);

static inline uint32_t mask_addr(uint32_t addr);

static void arm7_reset(uint32_t val);

void aica_common_init(struct aica_common *cmn) {
    memset(cmn->backing, 0, sizeof(cmn->backing));
}

void aica_common_cleanup(struct aica_common *cmn) {
}

static float aica_common_read_float(addr32_t addr, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    return ((float*)cmn->backing)[mask_addr(addr) / sizeof(float)];
}

static void aica_common_write_float(addr32_t addr, float val, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    ((float*)cmn->backing)[mask_addr(addr) / sizeof(float)] = val;
}

static double aica_common_read_double(addr32_t addr, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    return ((double*)cmn->backing)[mask_addr(addr) / sizeof(double)];
}

static void aica_common_write_double(addr32_t addr, double val, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    ((double*)cmn->backing)[mask_addr(addr) / sizeof(double)] = val;
}

static uint32_t aica_common_read_32(addr32_t addr, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    return ((uint32_t*)cmn->backing)[mask_addr(addr) / sizeof(uint32_t)];
}

static void aica_common_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    ((uint32_t*)cmn->backing)[mask_addr(addr) / sizeof(uint32_t)] = val;

    switch (addr) {
    case ARM7RST_ADDR:
        arm7_reset(val);
        break;
    default:
        break;
    }
}

static uint16_t aica_common_read_16(addr32_t addr, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    return ((uint16_t*)cmn->backing)[mask_addr(addr) / sizeof(uint16_t)];
}

static void aica_common_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    ((uint16_t*)cmn->backing)[mask_addr(addr) / sizeof(uint16_t)] = val;
}

static uint8_t aica_common_read_8(addr32_t addr, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    return ((uint8_t*)cmn->backing)[mask_addr(addr) / sizeof(uint8_t)];
}

static void aica_common_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica_common *cmn = (struct aica_common*)ctxt;
    ((uint8_t*)cmn->backing)[mask_addr(addr) / sizeof(uint8_t)] = val;
}

static inline uint32_t mask_addr(uint32_t addr) {
    return addr & 0x7ff;
}

static void arm7_reset(uint32_t val) {
    printf("%s(%u)\n", __func__, (unsigned)val);
}

struct memory_interface aica_common_intf = {
    .read32 = aica_common_read_32,
    .read16 = aica_common_read_16,
    .read8 = aica_common_read_8,
    .readfloat = aica_common_read_float,
    .readdouble = aica_common_read_double,

    .write32 = aica_common_write_32,
    .write16 = aica_common_write_16,
    .write8 = aica_common_write_8,
    .writefloat = aica_common_write_float,
    .writedouble = aica_common_write_double
};
