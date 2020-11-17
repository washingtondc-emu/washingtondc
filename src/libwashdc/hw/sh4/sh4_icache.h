/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef SH4_ICACHE_H_
#define SH4_ICACHE_H_

#include "sh4.h"
#include "washdc/types.h"

#define SH4_IC_ADDR_ARRAY_FIRST 0xf0000000
#define SH4_IC_ADDR_ARRAY_LAST  0xf0ffffff

void sh4_icache_write_addr_array_float(Sh4 *sh4, addr32_t paddr, float val);
void sh4_icache_write_addr_array_double(Sh4 *sh4, addr32_t paddr, double val);
void sh4_icache_write_addr_array_32(Sh4 *sh4, addr32_t paddr, uint32_t val);
void sh4_icache_write_addr_array_16(Sh4 *sh4, addr32_t paddr, uint16_t val);
void sh4_icache_write_addr_array_8(Sh4 *sh4, addr32_t paddr, uint8_t val);

float sh4_icache_read_addr_array_float(Sh4 *sh4, addr32_t paddr);
double sh4_icache_read_addr_array_double(Sh4 *sh4, addr32_t paddr);
uint32_t sh4_icache_read_addr_array_32(Sh4 *sh4, addr32_t paddr);
uint16_t sh4_icache_read_addr_array_16(Sh4 *sh4, addr32_t paddr);
uint8_t sh4_icache_read_addr_array_8(Sh4 *sh4, addr32_t paddr);

#endif
