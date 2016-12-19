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

#include <cstring>

#include "BaseException.hpp"

#include "Memory.hpp"

Memory::Memory(size_t size) {
    this->size = size;
    this->mem = new boost::uint8_t[size];
}

Memory::~Memory() {
    delete[] mem;
}

int Memory::read(void *buf, size_t addr, size_t len) const {
    size_t end_addr = addr + (len - 1);
    if (addr >= size || end_addr >= size || end_addr < addr) {
        BOOST_THROW_EXCEPTION(MemBoundsError() << errinfo_guest_addr(addr));
    }

    memcpy(buf, mem + addr, len);

    return 0;
}

int Memory::write(void const *buf, size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (addr >= size || end_addr >= size || end_addr < addr) {
        BOOST_THROW_EXCEPTION(MemBoundsError() << errinfo_guest_addr(addr));
    }

    memcpy(mem + addr, buf, len);

    return 0;
}

size_t Memory::get_size() const {
    return size;
}
