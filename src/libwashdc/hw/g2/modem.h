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

#ifndef MODEM_H_
#define MODEM_H_

#include "washdc/types.h"
#include "washdc/MemoryMap.h"

float modem_read_float(addr32_t addr, void *ctxt);
void modem_write_float(addr32_t addr, float val, void *ctxt);
double modem_read_double(addr32_t addr, void *ctxt);
void modem_write_double(addr32_t addr, double val, void *ctxt);
uint8_t modem_read_8(addr32_t addr, void *ctxt);
void modem_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t modem_read_16(addr32_t addr, void *ctxt);
void modem_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t modem_read_32(addr32_t addr, void *ctxt);
void modem_write_32(addr32_t addr, uint32_t val, void *ctxt);

extern struct memory_interface modem_intf;

#endif
