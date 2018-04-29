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

#ifndef BIOSFILE_H_
#define BIOSFILE_H_

#include <string.h>
#include <stdint.h>

#include "types.h"
#include "error.h"

#define BIOS_SZ_EXPECT (0x1fffff + 1)

void bios_file_init(char const *path);
void bios_file_cleanup(void);

// consider yourself warned: these functions don't do bounds-checking
uint8_t bios_file_read_8(addr32_t addr);
uint16_t bios_file_read_16(addr32_t addr);
uint32_t bios_file_read_32(addr32_t addr);
float bios_file_read_float(addr32_t addr);
double bios_file_read_double(addr32_t addr);

#endif
