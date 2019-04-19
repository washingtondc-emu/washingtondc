/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include "washdc/types.h"
#include "washdc/error.h"
#include "washdc/MemoryMap.h"

struct boot_rom {
    size_t dat_len;
    uint8_t *dat;
};

#define BIOS_SZ_EXPECT (0x1fffff + 1)

void boot_rom_init(struct boot_rom *rom, char const *path);
void boot_rom_cleanup(struct boot_rom *rom);

extern struct memory_interface boot_rom_intf;

#endif
