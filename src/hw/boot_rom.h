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

#ifndef BOOT_ROM_H_
#define BOOT_ROM_H_

#include <string.h>
#include <stdint.h>

#include "types.h"
#include "error.h"
#include "MemoryMap.h"

struct boot_rom {
    size_t dat_len;
    uint8_t *dat;
};

#define BIOS_SZ_EXPECT (0x1fffff + 1)

void boot_rom_init(struct boot_rom *rom, char const *path);
void boot_rom_cleanup(struct boot_rom *rom);

// consider yourself warned: these functions don't do bounds-checking
uint8_t boot_rom_read_8(addr32_t addr, void *ctxt);
uint16_t boot_rom_read_16(addr32_t addr, void *ctxt);
uint32_t boot_rom_read_32(addr32_t addr, void *ctxt);
float boot_rom_read_float(addr32_t addr, void *ctxt);
double boot_rom_read_double(addr32_t addr, void *ctxt);

void boot_rom_write_8(addr32_t addr, uint8_t val, void *ctxt);
void boot_rom_write_16(addr32_t addr, uint16_t val, void *ctxt);
void boot_rom_write_32(addr32_t addr, uint32_t val, void *ctxt);
void boot_rom_write_float(addr32_t addr, float val, void *ctxt);
void boot_rom_write_double(addr32_t addr, double val, void *ctxt);

extern struct memory_interface boot_rom_intf;

#endif
