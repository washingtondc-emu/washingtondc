/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#ifndef MEMORYMAP_HPP_
#define MEMORYMAP_HPP_

#include "BiosFile.hpp"
#include "Memory.hpp"
#include "hw/g1/g1.hpp"

// System Boot ROM
const static size_t ADDR_BIOS_FIRST = 0;
const static size_t ADDR_BIOS_LAST  = 0x001fffff;

// main system memory
const static size_t ADDR_RAM_FIRST  = 0x0c000000;
const static size_t ADDR_RAM_LAST   = 0x0cffffff;

// G1 bus control registers
const static size_t ADDR_G1_FIRST = 0x005F7400;
const static size_t ADDR_G1_LAST  = 0x005F74FF;

void memory_map_init(BiosFile *bios_new, struct Memory *mem_new, G1Bus *g1_new);
void memory_map_set_bios(BiosFile *bios_new);
void memory_map_set_mem(struct Memory *mem_new);
void memory_map_set_g1(G1Bus *g1_new);

int memory_map_read(void *buf, size_t addr, size_t len);
int memory_map_write(void const *buf, size_t addr, size_t len);

#endif
