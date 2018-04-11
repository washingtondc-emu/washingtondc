/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

#ifndef MEMORYMAP_H_
#define MEMORYMAP_H_

#include <stdint.h>

#include "BiosFile.h"
#include "memory.h"
#include "mem_areas.h"

typedef float(*memory_map_readfloat_func)(uint32_t addr);
typedef double(*memory_map_readdouble_func)(uint32_t addr);
typedef uint32_t(*memory_map_read32_func)(uint32_t addr);
typedef uint16_t(*memory_map_read16_func)(uint32_t addr);
typedef uint8_t(*memory_map_read8_func)(uint32_t addr);

typedef void(*memory_map_writefloat_func)(uint32_t addr, float val);
typedef void(*memory_map_writedouble_func)(uint32_t addr, double val);
typedef void(*memory_map_write32_func)(uint32_t addr, uint32_t val);
typedef void(*memory_map_write16_func)(uint32_t addr, uint16_t val);
typedef void(*memory_map_write8_func)(uint32_t addr, uint8_t val);

struct memory_map_region {
    uint32_t first_addr, last_addr;

    uint32_t range_mask;

    uint32_t mask;

    memory_map_readdouble_func readdouble;
    memory_map_readfloat_func readfloat;
    memory_map_read32_func read32;
    memory_map_read16_func read16;
    memory_map_read8_func read8;

    memory_map_writedouble_func writedouble;
    memory_map_writefloat_func writefloat;
    memory_map_write32_func write32;
    memory_map_write16_func write16;
    memory_map_write8_func write8;
};

void memory_map_init(BiosFile *bios_new, struct Memory *mem_new);
void memory_map_set_bios(BiosFile *bios_new);
void memory_map_set_mem(struct Memory *mem_new);

uint8_t memory_map_read_8(uint32_t addr);
uint16_t memory_map_read_16(uint32_t addr);
uint32_t memory_map_read_32(uint32_t addr);
float memory_map_read_float(uint32_t addr);
double memory_map_read_double(uint32_t addr);

void memory_map_write_8(uint8_t val, uint32_t addr);
void memory_map_write_16(uint16_t val, uint32_t addr);
void memory_map_write_32(uint32_t val, uint32_t addr);
void memory_map_write_float(float val, uint32_t addr);
void memory_map_write_double(double val, uint32_t addr);

#endif
