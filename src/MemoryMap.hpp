/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

class MemoryMap {
public:
    const static size_t BIOS_FIRST = 0;
    const static size_t BIOS_LAST  = 0x001fffff;

    const static size_t RAM_FIRST  = 0x0c000000;
    const static size_t RAM_LAST   = 0x0cffffff;

    MemoryMap(BiosFile *bios, Memory *mem);
    ~MemoryMap();

    int read(void *buf, size_t addr, size_t len) const;
    int write(void const *buf, size_t addr, size_t len);

private:
    BiosFile *bios;
    Memory *mem;
};

#endif
