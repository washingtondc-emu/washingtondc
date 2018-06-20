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

#include <stdbool.h>
#include <stdio.h>

#include "hw/arm7/arm7.h"

#include "aica.h"

#define AICA_ARM7_RST 0x2c00

static float aica_sys_read_float(addr32_t addr, void *ctxt);
static void aica_sys_write_float(addr32_t addr, float val, void *ctxt);
static double aica_sys_read_double(addr32_t addr, void *ctxt);
static void aica_sys_write_double(addr32_t addr, double val, void *ctxt);
static uint32_t aica_sys_read_32(addr32_t addr, void *ctxt);
static void aica_sys_write_32(addr32_t addr, uint32_t val, void *ctxt);
static uint16_t aica_sys_read_16(addr32_t addr, void *ctxt);
static void aica_sys_write_16(addr32_t addr, uint16_t val, void *ctxt);
static uint8_t aica_sys_read_8(addr32_t addr, void *ctxt);
static void aica_sys_write_8(addr32_t addr, uint8_t val, void *ctxt);

struct memory_interface aica_sys_intf = {
    .read32 = aica_sys_read_32,
    .read16 = aica_sys_read_16,
    .read8 = aica_sys_read_8,
    .readfloat = aica_sys_read_float,
    .readdouble = aica_sys_read_double,

    .write32 = aica_sys_write_32,
    .write16 = aica_sys_write_16,
    .write8 = aica_sys_write_8,
    .writefloat = aica_sys_write_float,
    .writedouble = aica_sys_write_double
};

void aica_init(struct aica *aica, struct arm7 *arm7) {
    aica->arm7 = arm7;
    aica_wave_mem_init(&aica->mem);
}

void aica_cleanup(struct aica *aica) {
    aica_wave_mem_cleanup(&aica->mem);
}

static float aica_sys_read_float(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    return ((float*)aica->sys_reg)[addr / 4];
}

static void aica_sys_write_float(addr32_t addr, float val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    ((float*)aica->sys_reg)[addr / 4] = val;
}

static double aica_sys_read_double(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    return ((double*)aica->sys_reg)[addr / 8];
}

static void aica_sys_write_double(addr32_t addr, double val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    ((double*)aica->sys_reg)[addr / 8] = val;
}

static uint32_t aica_sys_read_32(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    return aica->sys_reg[addr / 4];
}

static void aica_sys_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    switch (addr) {
    case AICA_ARM7_RST:
        if (from_sh4) {
            arm7_reset(aica->arm7, !(val & 1));
        } else {
            printf("ARM7 suicide unimplemented\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    default:
        aica->sys_reg[addr / 4] = val;
    }
}

static uint16_t aica_sys_read_16(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    return ((uint16_t*)aica->sys_reg)[addr / 2];
}

static void aica_sys_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    ((uint16_t*)aica->sys_reg)[addr / 2] = val;
}

static uint8_t aica_sys_read_8(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    return ((uint8_t*)aica->sys_reg)[addr];
}

static void aica_sys_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    addr &= AICA_SYS_MASK;

    ((uint8_t*)aica->sys_reg)[addr] = val;
}
