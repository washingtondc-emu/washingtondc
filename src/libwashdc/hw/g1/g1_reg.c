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
#include <stdio.h>

#include <stdint.h>

#include "g1_reg.h"

#include "mem_code.h"
#include "washdc/error.h"
#include "washdc/types.h"
#include "mem_areas.h"
#include "log.h"

DEF_MMIO_REGION(g1_reg_32, N_G1_REGS, ADDR_G1_FIRST, uint32_t)
DEF_MMIO_REGION(g1_reg_16, N_G1_REGS, ADDR_G1_FIRST, uint16_t)

static struct mmio_region_g1_reg_32 mmio_region_g1_reg_32;
static struct mmio_region_g1_reg_16 mmio_region_g1_reg_16;

static uint8_t reg_backing[N_G1_REGS];

uint8_t g1_reg_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t g1_reg_read_16(addr32_t addr, void *ctxt) {
    return mmio_region_g1_reg_16_read(&mmio_region_g1_reg_16, addr);
}

void g1_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    mmio_region_g1_reg_16_write(&mmio_region_g1_reg_16, addr, val);
}

uint32_t g1_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_g1_reg_32_read(&mmio_region_g1_reg_32, addr);
}

void g1_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_g1_reg_32_write(&mmio_region_g1_reg_32, addr, val);
}

float g1_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_g1_reg_32_read(&mmio_region_g1_reg_32, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void g1_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_g1_reg_32_write(&mmio_region_g1_reg_32, addr, tmp);
}

double g1_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_init(void) {
    init_mmio_region_g1_reg_32(&mmio_region_g1_reg_32, (void*)reg_backing);
    init_mmio_region_g1_reg_16(&mmio_region_g1_reg_16, (void*)reg_backing);

    /* system boot-rom registers */
    // XXX this is supposed to be write-only, but currently it's readable
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler,
                                    NULL);

    /* flash rom registers */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FRC", 0x5f7488,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FWC", 0x5f748c,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);

    /* GD PIO timing registers - I guess this is related to GD-ROM ? */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRC", 0x5f7490,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CWC", 0x5f7494,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);

    // TODO: SB_G1SYSM should be read-only
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1SYSM", 0x5f74b0,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRDYC", 0x5f74b4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "UNKNOWN", 0x005f74e4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
}

void g1_reg_cleanup(void) {
    cleanup_mmio_region_g1_reg_32(&mmio_region_g1_reg_32);
    cleanup_mmio_region_g1_reg_16(&mmio_region_g1_reg_16);
}

struct memory_interface g1_intf = {
    .read32 = g1_reg_read_32,
    .read16 = g1_reg_read_16,
    .read8 = g1_reg_read_8,
    .readfloat = g1_reg_read_float,
    .readdouble = g1_reg_read_double,

    .write32 = g1_reg_write_32,
    .write16 = g1_reg_write_16,
    .write8 = g1_reg_write_8,
    .writefloat = g1_reg_write_float,
    .writedouble = g1_reg_write_double
};

void g1_mmio_cell_init_32(char const *name, uint32_t addr,
                          mmio_region_g1_reg_32_read_handler on_read,
                          mmio_region_g1_reg_32_write_handler on_write,
                          void *ctxt) {
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32, name, addr,
                                    on_read, on_write, ctxt);
}
