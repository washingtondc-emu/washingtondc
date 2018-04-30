/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#ifndef AICA_REG_H_
#define AICA_REG_H_

#include <stddef.h>

#include "types.h"
#include "MemoryMap.h"

void aica_reg_init(void);
void aica_reg_cleanup(void);

float aica_reg_read_float(addr32_t addr);
void aica_reg_write_float(addr32_t addr, float val);
double aica_reg_read_double(addr32_t addr);
void aica_reg_write_double(addr32_t addr, double val);
uint32_t aica_reg_read_32(addr32_t addr);
void aica_reg_write_32(addr32_t addr, uint32_t val);
uint16_t aica_reg_read_16(addr32_t addr);
void aica_reg_write_16(addr32_t addr, uint16_t val);
uint8_t aica_reg_read_8(addr32_t addr);
void aica_reg_write_8(addr32_t addr, uint8_t val);

extern struct memory_interface aica_reg_intf;

#endif
