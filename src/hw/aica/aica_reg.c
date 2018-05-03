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

#include "mem_code.h"
#include "MemoryMap.h"
#include "log.h"
#include "mem_areas.h"
#include "mmio.h"

#include "aica_reg.h"

#define N_AICA_REGS (ADDR_AICA_LAST - ADDR_AICA_FIRST + 1)

DECL_MMIO_REGION(aica_reg, N_AICA_REGS, ADDR_AICA_FIRST, uint32_t)
DEF_MMIO_REGION(aica_reg, N_AICA_REGS, ADDR_AICA_FIRST, uint32_t)

static uint8_t reg_backing[N_AICA_REGS];

void aica_reg_init(void) {
    unsigned idx;

    init_mmio_region_aica_reg(&mmio_region_aica_reg, (void*)reg_backing);

    /*
     * two-byte register containing VREG and some other weird unrelated stuff
     * that is part of AICA for reasons which I cannot fathom
     */
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700000", 0x00700000,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700004", 0x00700004,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700008", 0x00700008,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_0070000c", 0x0070000c,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700010", 0x00700010,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700014", 0x00700014,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700018", 0x00700018,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_0070001c", 0x0070001c,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700020", 0x00700020,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700024", 0x00700024,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700028", 0x00700028,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_FLV0", 0x0070002c,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_FLV1", 0x00700030,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_FLV2", 0x00700034,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_FLV3", 0x00700038,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_FLV4", 0x0070003c,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700040", 0x00700040,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00700044", 0x00700044,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);

    for (idx = 0; idx < 0x7d2; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_SLOT_CONTROL", 0x700080 + 4 * idx,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 18; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_DSP_OUT", 0x702000 + 4 * idx,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 128; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_COEF", 0x00703000 + 4 * idx,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 64; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_MADDRS", 0x703200 + 4 * idx,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < (128 * 4); idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_MPRO", 0x703400 + 4 * idx,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 256; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_TEMP", 0x704000 + idx * 4,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 64; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_MEMS", 0x704400 + idx * 4,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 32; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_MIXS", 0x704500 + idx * 4,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 16; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_EFREG", 0x704580 + idx * 4,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }
    for (idx = 0; idx < 2; idx++) {
        mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                       "AICA_EXTS", 0x7045c0 + idx * 4,
                                       mmio_region_aica_reg_warn_read_handler,
                                       mmio_region_aica_reg_warn_write_handler);
    }

    /*
     * writing 1 to this register immediately stops whatever the ARM7 is doing
     * so that a new program can be loaded.  Subsequently writing 0 to this
     * register will reactivate the ARM7 and cause it to begin executing
     * instructions starting from 0x00800000 (like a power-on reset).
     *
     * I'm not sure what the initial value here ought to be, but logic would
     * seem to dictate that the ARM7 must be initially disabled since there
     * won't be any program loaded immediately after the Dreamcast powers on.
     */
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_ARM7_DISABLE", 0x00702c00,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);

    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_00702800", 0x00702800,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_0070289c", 0x0070289c,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_007028a0", 0x007028a0,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_007028a4", 0x007028a4,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_007028b4", 0x007028b4,
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
    mmio_region_aica_reg_init_cell(&mmio_region_aica_reg,
                                   "AICA_007028bc", 0x007028bc, 
                                   mmio_region_aica_reg_warn_read_handler,
                                   mmio_region_aica_reg_warn_write_handler);
}

void aica_reg_cleanup(void) {
    cleanup_mmio_region_aica_reg(&mmio_region_aica_reg);
}

float aica_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_aica_reg_read(&mmio_region_aica_reg, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void aica_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_aica_reg_write(&mmio_region_aica_reg, addr, tmp);
}

double aica_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t aica_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_aica_reg_read(&mmio_region_aica_reg, addr);
}

void aica_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_aica_reg_write(&mmio_region_aica_reg, addr, val);
}

uint16_t aica_reg_read_16(addr32_t addr, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t aica_reg_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

struct memory_interface aica_reg_intf = {
    .read32 = aica_reg_read_32,
    .read16 = aica_reg_read_16,
    .read8 = aica_reg_read_8,
    .readfloat = aica_reg_read_float,
    .readdouble = aica_reg_read_double,

    .write32 = aica_reg_write_32,
    .write16 = aica_reg_write_16,
    .write8 = aica_reg_write_8,
    .writefloat = aica_reg_write_float,
    .writedouble = aica_reg_write_double
};
