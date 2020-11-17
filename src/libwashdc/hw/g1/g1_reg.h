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

#ifndef G1_REG_H_
#define G1_REG_H_

#include <stddef.h>
#include <stdint.h>

#include "mmio.h"
#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

#define N_G1_REGS (ADDR_G1_LAST - ADDR_G1_FIRST + 1)
DECL_MMIO_REGION(g1_reg_32, N_G1_REGS, ADDR_G1_FIRST, uint32_t)
DECL_MMIO_REGION(g1_reg_16, N_G1_REGS, ADDR_G1_FIRST, uint16_t)

uint8_t g1_reg_read_8(addr32_t addr, void *ctxt);
void g1_reg_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t g1_reg_read_16(addr32_t addr, void *ctxt);
void g1_reg_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t g1_reg_read_32(addr32_t addr, void *ctxt);
void g1_reg_write_32(addr32_t addr, uint32_t val, void *ctxt);
float g1_reg_read_float(addr32_t addr, void *ctxt);
void g1_reg_write_float(addr32_t addr, float val, void *ctxt);
double g1_reg_read_double(addr32_t addr, void *ctxt);
void g1_reg_write_double(addr32_t addr, double val, void *ctxt);

void g1_reg_init(void);
void g1_reg_cleanup(void);

extern struct memory_interface g1_intf;

void g1_mmio_cell_init_32(char const *name, uint32_t addr,
                          mmio_region_g1_reg_32_read_handler on_read,
                          mmio_region_g1_reg_32_write_handler on_write,
                          void *ctxt);

#endif
