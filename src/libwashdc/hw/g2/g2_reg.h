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

#ifndef G2_REG_H_
#define G2_REG_H_

#include <stddef.h>

#include "washdc/types.h"
#include "washdc/MemoryMap.h"

uint8_t g2_reg_read_8(addr32_t addr, void *ctxt);
void g2_reg_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t g2_reg_read_16(addr32_t addr, void *ctxt);
void g2_reg_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t g2_reg_read_32(addr32_t addr, void *ctxt);
void g2_reg_write_32(addr32_t addr, uint32_t val, void *ctxt);
float g2_reg_read_float(addr32_t addr, void *ctxt);
void g2_reg_write_float(addr32_t addr, float val, void *ctxt);
double g2_reg_read_double(addr32_t addr, void *ctxt);
void g2_reg_write_double(addr32_t addr, double val, void *ctxt);

void g2_reg_init(void);
void g2_reg_cleanup(void);

extern struct memory_interface g2_intf;

#endif
