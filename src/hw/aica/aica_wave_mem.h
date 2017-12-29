/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef AICA_WAVE_MEM_H_
#define AICA_WAVE_MEM_H_

#include <stdbool.h>
#include <stdint.h>

#include "types.h"

float aica_wave_mem_read_float(addr32_t addr);
void aica_wave_mem_write_float(addr32_t addr, float val);
double aica_wave_mem_read_double(addr32_t addr);
void aica_wave_mem_write_double(addr32_t addr, double val);
uint8_t aica_wave_mem_read_8(addr32_t addr);
void aica_wave_mem_write_8(addr32_t addr, uint8_t val);
uint16_t aica_wave_mem_read_16(addr32_t addr);
void aica_wave_mem_write_16(addr32_t addr, uint16_t val);
uint32_t aica_wave_mem_read_32(addr32_t addr);
void aica_wave_mem_write_32(addr32_t addr, uint32_t val);

void aica_log_verbose(bool verbose);

#endif
