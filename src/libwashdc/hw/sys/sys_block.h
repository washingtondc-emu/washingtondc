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

/*
 * sys block - the Dreamcast's System Block.
 *
 * Currently it's a dumping ground for a bunch of things that I know probably
 * belong in separate compoments
 */

#ifndef SYS_BLOCK_H_
#define SYS_BLOCK_H_

#include "mmio.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

#define N_SYS_REGS (ADDR_SYS_LAST - ADDR_SYS_FIRST + 1)

DECL_MMIO_REGION(sys_block, N_SYS_REGS, ADDR_SYS_FIRST, uint32_t)

void sys_block_init(void);
void sys_block_cleanup(void);

float sys_block_read_float(addr32_t addr, void *ctxt);
void sys_block_write_float(addr32_t addr, float val, void *ctxt);
double sys_block_read_double(addr32_t addr, void *ctxt);
void sys_block_write_double(addr32_t addr, double val, void *ctxt);
uint8_t sys_block_read_8(addr32_t addr, void *ctxt);
void sys_block_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t sys_block_read_16(addr32_t addr, void *ctxt);
void sys_block_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t sys_block_read_32(addr32_t addr, void *ctxt);
void sys_block_write_32(addr32_t addr, uint32_t val, void *ctxt);

extern struct memory_interface sys_block_intf;

#endif
