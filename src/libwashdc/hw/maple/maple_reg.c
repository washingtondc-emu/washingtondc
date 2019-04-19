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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh4.h"
#include "sh4_dmac.h"
#include "washdc/error.h"
#include "mem_code.h"
#include "mem_areas.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"
#include "maple.h"
#include "mmio.h"

#include "maple_reg.h"

#define N_MAPLE_REGS (ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1)

DECL_MMIO_REGION(maple_reg, N_MAPLE_REGS, ADDR_MAPLE_FIRST, uint32_t)
DEF_MMIO_REGION(maple_reg, N_MAPLE_REGS, ADDR_MAPLE_FIRST, uint32_t)

static struct mmio_region_maple_reg mmio_region_maple_reg;

static uint8_t reg_backing[N_MAPLE_REGS];

static addr32_t maple_dma_prot_bot = 0;
static addr32_t maple_dma_prot_top = (0x1 << 27) | (0x7f << 20);

static addr32_t maple_dma_cmd_start;

float maple_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_maple_reg_read(&mmio_region_maple_reg, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void maple_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_maple_reg_write(&mmio_region_maple_reg, addr, tmp);
}

double maple_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void maple_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t maple_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_maple_reg_read(&mmio_region_maple_reg, addr);
}

void maple_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_maple_reg_write(&mmio_region_maple_reg, addr, val);
}

uint16_t maple_reg_read_16(addr32_t addr, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void maple_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t maple_reg_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void maple_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t mden_reg_mmio_read(struct mmio_region_maple_reg *region,
                                   unsigned idx, void *ctxt) {
    MAPLE_TRACE("reading 0 from register \"SB_MDEN\"\n");
    return 0;
}

static void
mden_reg_mmio_write(struct mmio_region_maple_reg *region,
                    unsigned idx, uint32_t val, void *ctxt) {
    if (val)
        MAPLE_TRACE("WARNING: enabling DMA\n");
    else
        MAPLE_TRACE("WARNING: aborting DMA\n");
}

static uint32_t
mdstar_reg_mmio_read(struct mmio_region_maple_reg *region,
                     unsigned idx, void *ctxt) {
    MAPLE_TRACE("reading %08x from MDSTAR\n",
                (unsigned)maple_dma_cmd_start);
    return maple_dma_cmd_start;
}

static void mdstar_reg_mmio_write(struct mmio_region_maple_reg *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    maple_dma_cmd_start = val;
    MAPLE_TRACE("writing %08x to MDSTAR\n",
                (unsigned)maple_dma_cmd_start);
}

static uint32_t
mdtsel_reg_mmio_read(struct mmio_region_maple_reg *region,
                     unsigned idx, void *ctxt) {
    MAPLE_TRACE("reading 0 from MDTSEL\n");
    return 0;
}

static void mdtsel_reg_mmio_write(struct mmio_region_maple_reg *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    if (val) {
        error_set_feature("vblank Maple-DMA initialization");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static uint32_t
mdst_reg_mmio_read(struct mmio_region_maple_reg *region,
                   unsigned idx, void *ctxt) {
    MAPLE_TRACE("reading 0 from MDST\n");
    return 0;
}

#include "dreamcast.h"

static void
mdst_reg_mmio_write(struct mmio_region_maple_reg *region,
                    unsigned idx, uint32_t val, void *ctxt) {
    if (val) {
        MAPLE_TRACE("starting maple DMA operation\n");
        MAPLE_TRACE("\tstarting address is %08x\n",
                    (unsigned)maple_dma_cmd_start);
        MAPLE_TRACE("SH4 PC address is 0x%08x\n", (unsigned)dreamcast_get_cpu()->reg[SH4_REG_PC]);

        maple_process_dma(maple_dma_cmd_start);
    }
}

addr32_t maple_get_dma_prot_bot(void) {
    return maple_dma_prot_bot;
}

addr32_t maple_get_dma_prot_top(void) {
    return maple_dma_prot_top;
}

void maple_reg_init(void) {
    init_mmio_region_maple_reg(&mmio_region_maple_reg, (void*)reg_backing);

    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDSTAR", 0x5f6c04,
                                    mdstar_reg_mmio_read,
                                    mdstar_reg_mmio_write, NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDTSEL", 0x5f6c10,
                                    mdtsel_reg_mmio_read,
                                    mdtsel_reg_mmio_write, NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDEN", 0x5f6c14,
                                    mden_reg_mmio_read,
                                    mden_reg_mmio_write, NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDST", 0x5f6c18,
                                    mdst_reg_mmio_read,
                                    mdst_reg_mmio_write, NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MSYS", 0x5f6c80,
                                    mmio_region_maple_reg_warn_read_handler,
                                    mmio_region_maple_reg_warn_write_handler,
                                    NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MDAPRO", 0x5f6c8c,
                                    mmio_region_maple_reg_writeonly_read_error,
                                    mmio_region_maple_reg_warn_write_handler,
                                    NULL);
    mmio_region_maple_reg_init_cell(&mmio_region_maple_reg,
                                    "SB_MMSEL", 0x5f6ce8,
                                    mmio_region_maple_reg_warn_read_handler,
                                    mmio_region_maple_reg_warn_write_handler,
                                    NULL);
}

void maple_reg_cleanup(void) {
    cleanup_mmio_region_maple_reg(&mmio_region_maple_reg);
}

struct memory_interface maple_intf = {
    .read32 = maple_reg_read_32,
    .read16 = maple_reg_read_16,
    .read8 = maple_reg_read_8,
    .readfloat = maple_reg_read_float,
    .readdouble = maple_reg_read_double,

    .write32 = maple_reg_write_32,
    .write16 = maple_reg_write_16,
    .write8 = maple_reg_write_8,
    .writefloat = maple_reg_write_float,
    .writedouble = maple_reg_write_double
};
