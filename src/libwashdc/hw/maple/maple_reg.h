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

#ifndef MAPLE_REG_H_
#define MAPLE_REG_H_

#include <stddef.h>

#include "washdc/types.h"

float maple_reg_read_float(addr32_t addr, void *ctxt);
void maple_reg_write_float(addr32_t addr, float val, void *ctxt);
double maple_reg_read_double(addr32_t addr, void *ctxt);
void maple_reg_write_double(addr32_t addr, double val, void *ctxt);
uint32_t maple_reg_read_32(addr32_t addr, void *ctxt);
void maple_reg_write_32(addr32_t addr, uint32_t val, void *ctxt);
uint16_t maple_reg_read_16(addr32_t addr, void *ctxt);
void maple_reg_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint8_t maple_reg_read_8(addr32_t addr, void *ctxt);
void maple_reg_write_8(addr32_t addr, uint8_t val, void *ctxt);

addr32_t maple_get_dma_prot_bot(void);
addr32_t maple_get_dma_prot_top(void);

void maple_reg_init(void);
void maple_reg_cleanup(void);

extern struct memory_interface maple_intf;

#endif
