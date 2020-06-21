/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
