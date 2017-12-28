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

#include "g1_reg.h"
#include "hw/gdrom/gdrom_reg.h"

#include "mem_code.h"
#include "error.h"
#include "types.h"
#include "mem_areas.h"
#include "log.h"

DEF_MMIO_REGION(g1_reg_32, N_G1_REGS, ADDR_G1_FIRST, uint32_t)
DEF_MMIO_REGION(g1_reg_16, N_G1_REGS, ADDR_G1_FIRST, uint16_t)

int g1_reg_read(void *buf, size_t addr, size_t len) {
    if (len == 4) {
        *(uint32_t*)buf =
            mmio_region_g1_reg_32_read(&mmio_region_g1_reg_32, addr);
        return MEM_ACCESS_SUCCESS;
    } else if (len == 2) {
        *(uint16_t*)buf =
            mmio_region_g1_reg_16_read(&mmio_region_g1_reg_16, addr);
        return MEM_ACCESS_SUCCESS;
    } else {
        error_set_address(addr);
        error_set_length(len);
        return MEM_ACCESS_FAILURE;
    }
}

int g1_reg_write(void const *buf, size_t addr, size_t len) {
    if (len == 4) {
        mmio_region_g1_reg_32_write(&mmio_region_g1_reg_32,
                                    addr, *(uint32_t*)buf);
        return MEM_ACCESS_SUCCESS;
    } else if (len == 2) {
        mmio_region_g1_reg_16_write(&mmio_region_g1_reg_16,
                                    addr, *(uint32_t*)buf);
        return MEM_ACCESS_SUCCESS;
    } else {
        error_set_address(addr);
        error_set_length(len);
        return MEM_ACCESS_FAILURE;
    }
}

void g1_reg_init(void) {
    init_mmio_region_g1_reg_32(&mmio_region_g1_reg_32);
    init_mmio_region_g1_reg_16(&mmio_region_g1_reg_16);

    /* GD-ROM DMA registers */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDAPRO", 0x5f74b8,
                                    gdrom_gdapro_mmio_read,
                                    gdrom_gdapro_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1GDRC", 0x5f74a0,
                                    gdrom_g1gdrc_mmio_read,
                                    gdrom_g1gdrc_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1GDWC", 0x5f74a4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDSTAR", 0x5f7404,
                                    gdrom_gdstar_mmio_read,
                                    gdrom_gdstar_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDLEN", 0x5f7408,
                                    gdrom_gdlen_mmio_read,
                                    gdrom_gdlen_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDDIR", 0x5f740c,
                                    gdrom_gddir_mmio_read,
                                    gdrom_gddir_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDEN", 0x5f7414,
                                    gdrom_gden_mmio_read,
                                    gdrom_gden_mmio_write);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDST", 0x5f7418,
                                    gdrom_gdst_reg_read_handler,
                                    gdrom_gdst_reg_write_handler);

    /* system boot-rom registers */
    // XXX this is supposed to be write-only, but currently it's readable
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler);

    /* flash rom registers */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FRC", 0x5f7488,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FWC", 0x5f748c,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);

    /* GD PIO timing registers - I guess this is related to GD-ROM ? */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRC", 0x5f7490,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CWC", 0x5f7494,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);

    // TODO: SB_G1SYSM should be read-only
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1SYSM", 0x5f74b0,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRDYC", 0x5f74b4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "UNKNOWN", 0x005f74e4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_GDLEND", 0x005f74f8,
                                    gdrom_gdlend_mmio_read,
                                    mmio_region_g1_reg_32_readonly_write_error);
}

void g1_reg_cleanup(void) {
    cleanup_mmio_region_g1_reg_32(&mmio_region_g1_reg_32);
    cleanup_mmio_region_g1_reg_16(&mmio_region_g1_reg_16);
}
