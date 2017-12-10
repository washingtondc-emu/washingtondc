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

#ifndef MEMORYMAP_H_
#define MEMORYMAP_H_

#include "BiosFile.h"
#include "memory.h"
#include "mem_areas.h"

void memory_map_init(BiosFile *bios_new, struct Memory *mem_new);
void memory_map_set_bios(BiosFile *bios_new);
void memory_map_set_mem(struct Memory *mem_new);

/*
 * the error codes returned by these functions are the same as the error codes
 * defined in mem_code.h
 */
int memory_map_read(void *buf, size_t addr, size_t len);
int memory_map_write(void const *buf, size_t addr, size_t len);

#endif
