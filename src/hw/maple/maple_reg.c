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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh4.h"
#include "sh4_dmac.h"
#include "error.h"
#include "mem_code.h"
#include "mem_areas.h"
#include "types.h"
#include "MemoryMap.h"
#include "maple.h"
#include "mmio.h"

#include "maple_reg.h"

#define N_MAPLE_REGS (ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1)

DECL_MMIO_REGION(maple_reg, N_MAPLE_REGS, ADDR_MAPLE_FIRST, uint32_t)
DEF_MMIO_REGION(maple_reg, N_MAPLE_REGS, ADDR_MAPLE_FIRST, uint32_t)

static addr32_t maple_dma_prot_bot = 0;
static addr32_t maple_dma_prot_top = (0x1 << 27) | (0x7f << 20);

static addr32_t maple_dma_cmd_start;

int maple_reg_read(void *buf, size_t addr, size_t len) {
    if (len != 4)
        return MEM_ACCESS_FAILURE;
    *(uint32_t*)buf =
        mmio_region_maple_reg_read(&mmio_region_maple_reg, addr);
    return MEM_ACCESS_SUCCESS;
}

int maple_reg_write(void const *buf, size_t addr, size_t len) {
    if (len != 4)
        return MEM_ACCESS_FAILURE;
    mmio_region_maple_reg_write(&mmio_region_maple_reg,
                                addr, *(uint32_t*)buf);
    return MEM_ACCESS_SUCCESS;
}

static uint32_t mden_reg_mmio_read(struct mmio_region_maple_reg *region,
                                   unsigned idx) {
    MAPLE_TRACE("reading 0 from register \"SB_MDEN\"\n");
    return 0;
}

static void
mden_reg_mmio_write(struct mmio_region_maple_reg *region,
                    unsigned idx, uint32_t val) {
    if (val)
        MAPLE_TRACE("WARNING: enabling DMA\n");
    else
        MAPLE_TRACE("WARNING: aborting DMA\n");
}

static uint32_t
mdstar_reg_mmio_read(struct mmio_region_maple_reg *region,
                     unsigned idx) {
    MAPLE_TRACE("reading %08x from MDSTAR\n",
                (unsigned)maple_dma_cmd_start);
    return maple_dma_cmd_start;
}

static void mdstar_reg_mmio_write(struct mmio_region_maple_reg *region,
                                  unsigned idx, uint32_t val) {
    maple_dma_cmd_start = val;
    MAPLE_TRACE("writing %08x to MDSTAR\n",
                (unsigned)maple_dma_cmd_start);
}

static uint32_t
mdtsel_reg_mmio_read(struct mmio_region_maple_reg *region,
                     unsigned idx) {
    MAPLE_TRACE("reading 0 from MDTSEL\n");
    return 0;
}

static void mdtsel_reg_mmio_write(struct mmio_region_maple_reg *region,
                                  unsigned idx, uint32_t val) {
    if (val) {
        error_set_feature("vblank Maple-DMA initialization");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static uint32_t
mdst_reg_mmio_read(struct mmio_region_maple_reg *region,
                   unsigned idx) {
    MAPLE_TRACE("reading 0 from MDST\n");
    return 0;
}

static void
mdst_reg_mmio_write(struct mmio_region_maple_reg *region,
                    unsigned idx, uint32_t val) {
    if (val) {
        MAPLE_TRACE("starting maple DMA operation\n");
        MAPLE_TRACE("\tstarting address is %08x\n",
                    (unsigned)maple_dma_cmd_start);
        addr32_t addr = maple_dma_cmd_start;

        // it's static because I don't want it allocated on the stack
        static struct maple_frame frame;

        do {
            addr = maple_read_frame(&frame, addr);

            maple_handle_frame(&frame);
        } while(!frame.last_frame);
    }
}

addr32_t maple_get_dma_prot_bot(void) {
    return maple_dma_prot_bot;
}

addr32_t maple_get_dma_prot_top(void) {
    return maple_dma_prot_top;
}

void maple_reg_init(void) {
    init_mmio_region_maple_reg(&mmio_region_maple_reg);

    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDSTAR", 0x5f6c04,
                                    mdstar_reg_mmio_read,
                                    mdstar_reg_mmio_write);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDTSEL", 0x5f6c10,
                                    mdtsel_reg_mmio_read,
                                    mdtsel_reg_mmio_write);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDEN", 0x5f6c14,
                                    mden_reg_mmio_read,
                                    mden_reg_mmio_write);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDST", 0x5f6c18,
                                    mdst_reg_mmio_read,
                                    mdst_reg_mmio_write);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MSYS", 0x5f6c80,
                                    mmio_region_maple_reg_warn_read_handler,
                                    mmio_region_maple_reg_warn_write_handler);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDAPRO", 0x5f6c8c,
                                    mmio_region_maple_reg_writeonly_read_error,
                                    mmio_region_maple_reg_warn_write_handler);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MMSEL", 0x5f6ce8,
                                    mmio_region_maple_reg_warn_read_handler,
                                    mmio_region_maple_reg_warn_write_handler);
}

void maple_reg_cleanup(void) {
    cleanup_mmio_region_maple_reg(&mmio_region_maple_reg);
}
