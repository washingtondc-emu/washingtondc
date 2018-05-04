/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"
#include "log.h"
#include "mmio.h"
#include "error.h"

#include "pvr2_reg.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)

DECL_MMIO_REGION(pvr2_reg, N_PVR2_REGS, ADDR_PVR2_FIRST, uint32_t)
DEF_MMIO_REGION(pvr2_reg, N_PVR2_REGS, ADDR_PVR2_FIRST, uint32_t)

static uint8_t reg_backing[N_PVR2_REGS];

void pvr2_reg_init(void) {
    init_mmio_region_pvr2_reg(&mmio_region_pvr2_reg, (void*)reg_backing);

    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDSTAP", 0x5f7c00,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDSTAR", 0x5f7c04,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDLEN", 0x5f7c08,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDDIR", 0x5f7c0c,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDTSEL", 0x5f7c10,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDEN", 0x5f7c14,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDST", 0x5f7c18,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
    mmio_region_pvr2_reg_init_cell(&mmio_region_pvr2_reg,
                                   "SB_PDAPRO", 0x5f7c80,
                                   mmio_region_pvr2_reg_warn_read_handler,
                                   mmio_region_pvr2_reg_warn_write_handler);
}

void pvr2_reg_cleanup(void) {
    cleanup_mmio_region_pvr2_reg(&mmio_region_pvr2_reg);
}

double pvr2_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

float pvr2_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_pvr2_reg_read(&mmio_region_pvr2_reg, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_pvr2_reg_write(&mmio_region_pvr2_reg, addr, tmp);
}

uint32_t pvr2_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_pvr2_reg_read(&mmio_region_pvr2_reg, addr);
}

void pvr2_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_pvr2_reg_write(&mmio_region_pvr2_reg, addr, val);
}

uint16_t pvr2_reg_read_16(addr32_t addr, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t pvr2_reg_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

struct memory_interface pvr2_reg_intf = {
    .read32 = pvr2_reg_read_32,
    .read16 = pvr2_reg_read_16,
    .read8 = pvr2_reg_read_8,
    .readfloat = pvr2_reg_read_float,
    .readdouble = pvr2_reg_read_double,

    .write32 = pvr2_reg_write_32,
    .write16 = pvr2_reg_write_16,
    .write8 = pvr2_reg_write_8,
    .writefloat = pvr2_reg_write_float,
    .writedouble = pvr2_reg_write_double
};
