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

#include "BaseException.hpp"

#include "MemoryMap.hpp"

MemoryMap::MemoryMap(BiosFile *bios, Memory *mem) {
    this->bios = bios;
    this->mem = mem;
}

MemoryMap::~MemoryMap() {
}

int MemoryMap::read(void *buf, size_t addr, size_t len) const {
    try {
        // check RAM first because that's the case we want to optimize for
        if (addr >= RAM_FIRST && addr <= RAM_LAST) {
            return mem->read(buf, addr - RAM_FIRST, len);
        } else if (addr >= BIOS_FIRST && addr <= BIOS_LAST) {
            if (addr + len > BIOS_LAST)
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end"));
            return bios->read(buf, addr - BIOS_FIRST, len);
        }

        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("memory mapping") <<
                              errinfo_guest_addr(addr));
    } catch(BaseException& exc) {
        exc << errinfo_guest_addr(addr);
        throw;
    }
}

int MemoryMap::write(void const *buf, size_t addr, size_t len) {
    try {
        // check RAM first because that's the case we want to optimize for
        if (addr >= RAM_FIRST && addr <= RAM_LAST) {
            return mem->write(buf, addr - RAM_FIRST, len);
        } else if (addr >= BIOS_FIRST && addr <= BIOS_LAST) {
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("Proper response for when "
                                                  "the guest tries to write to "
                                                  "read-only memory"));
        }
    } catch(BaseException& exc) {
        exc << errinfo_guest_addr(addr);
        throw;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("memory mapping") <<
                          errinfo_guest_addr(addr));
}
