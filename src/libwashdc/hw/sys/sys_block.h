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

/*
 * sys block - the Dreamcast's System Block.
 *
 * Currently it's a dumping ground for a bunch of things that I know probably
 * belong in separate compoments
 */

#ifndef SYS_BLOCK_H_
#define SYS_BLOCK_H_

#include <stdint.h>

#include "mmio.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"
#include "dc_sched.h"

#define N_SYS_REGS (ADDR_SYS_LAST - ADDR_SYS_FIRST + 1)

DECL_MMIO_REGION(sys_block, N_SYS_REGS, ADDR_SYS_FIRST, uint32_t)

struct Sh4;
struct pvr2;

struct sys_block_ctxt {
    struct Sh4 *sh4;
    struct Memory *main_memory;
    struct pvr2 *pvr2;
    struct dc_clock *clk;

    // mmio metadata
    struct mmio_region_sys_block mmio_region_sys_block;
    uint32_t reg_backing[N_SYS_REGS / sizeof(uint32_t)];

    // channel-2 dma state
    uint32_t reg_sb_c2dstat, reg_sb_c2dlen;

    bool sort_dma_in_progress;
    struct SchedEvent sort_dma_complete_int_event;
};

void
sys_block_init(struct sys_block_ctxt *ctxt, struct dc_clock *clk,
               struct Sh4 *sh4, struct Memory *main_memory, struct pvr2 *pvr2);
void sys_block_cleanup(struct sys_block_ctxt *ctxt);

float sys_block_read_float(addr32_t addr, void *argp);
void sys_block_write_float(addr32_t addr, float val, void *argp);
double sys_block_read_double(addr32_t addr, void *argp);
void sys_block_write_double(addr32_t addr, double val, void *argp);
uint8_t sys_block_read_8(addr32_t addr, void *argp);
void sys_block_write_8(addr32_t addr, uint8_t val, void *argp);
uint16_t sys_block_read_16(addr32_t addr, void *argp);
void sys_block_write_16(addr32_t addr, uint16_t val, void *argp);
uint32_t sys_block_read_32(addr32_t addr, void *argp);
void sys_block_write_32(addr32_t addr, uint32_t val, void *argp);

extern struct memory_interface sys_block_intf;

#endif
