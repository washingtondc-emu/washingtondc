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

#include <fstream>
#include <iostream>

#include "flash_memory.hpp"

static uint8_t flash_mem[FLASH_MEM_SZ];

void flash_mem_load(char const *path) {
    std::ifstream file(path, std::ifstream::in | std::ifstream::binary);

    file.seekg(0, file.end);
    size_t len = file.tellg();
    file.seekg(0, file.beg);

    if (len != FLASH_MEM_SZ) {
        std::cout << "WARNING - unexpected flash memory size (expected " <<
            FLASH_MEM_SZ << " bytes).  This will still be loaded even " <<
            "though it's incorrect." << std::endl;
    }

    if (len > FLASH_MEM_SZ)
        len = FLASH_MEM_SZ;

    file.read((char*)flash_mem, len);

    file.close();
}

int flash_mem_read(void *buf, size_t addr, size_t len) {
    if ((addr + len - 1 > ADDR_FLASH_LAST) || (addr < ADDR_FLASH_FIRST)) {
        BOOST_THROW_EXCEPTION(MemBoundsError() <<
                              errinfo_guest_addr(addr) <<
                              errinfo_length(len));
    }

    memcpy(buf, flash_mem + (addr - ADDR_FLASH_FIRST), len);

    return 0;
}

int flash_mem_write(void const *buf, size_t addr, size_t len) {
    if ((addr + len - 1 > ADDR_FLASH_LAST) || (addr < ADDR_FLASH_FIRST)) {
        BOOST_THROW_EXCEPTION(MemBoundsError() <<
                              errinfo_guest_addr(addr) <<
                              errinfo_length(len));
    }

    memcpy(flash_mem + (addr - ADDR_FLASH_FIRST), buf, len);

    return 0;
}
