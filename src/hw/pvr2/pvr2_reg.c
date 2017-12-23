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

#include "pvr2_reg.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)

static struct mmio_region pvr2_reg_mmio;

void pvr2_reg_init(void) {
    init_mmio_region(&pvr2_reg_mmio, ADDR_PVR2_FIRST, ADDR_PVR2_LAST);

    init_mmio_cell(&pvr2_reg_mmio, "SB_PDSTAP", 0x5f7c00,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDSTAR", 0x5f7c04,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDLEN", 0x5f7c08,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDDIR", 0x5f7c0c,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDTSEL", 0x5f7c10,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDEN", 0x5f7c14,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDST", 0x5f7c18,
                   mmio_warn_read_handler, mmio_warn_write_handler);
    init_mmio_cell(&pvr2_reg_mmio, "SB_PDAPRO", 0x5f7c80,
                   mmio_warn_read_handler, mmio_warn_write_handler);
}

void pvr2_reg_cleanup(void) {
    cleanup_mmio_region(&pvr2_reg_mmio);
}

int pvr2_reg_read(void *buf, size_t addr, size_t len) {
    if (len != 4)
        return MEM_ACCESS_FAILURE;
    *(uint32_t*)buf = mmio_read_32(&pvr2_reg_mmio, addr);
    return MEM_ACCESS_SUCCESS;
}

int pvr2_reg_write(void const *buf, size_t addr, size_t len) {
    if (len != 4)
        return MEM_ACCESS_FAILURE;
    mmio_write_32(&pvr2_reg_mmio, addr, *(uint32_t*)buf);
    return MEM_ACCESS_SUCCESS;
}
