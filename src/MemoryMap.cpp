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

#include "BaseException.hpp"

#include "MemoryMap.hpp"

MemoryMap::MemoryMap(BiosFile *bios, Memory *mem, G1Bus *g1) {
    this->bios = bios;
    this->mem = mem;
    this->g1 = g1;
}

MemoryMap::~MemoryMap() {
}

int MemoryMap::read(void *buf, size_t addr, size_t len) const {
    try {
        // check RAM first because that's the case we want to optimize for
        if (addr >= RAM_FIRST && addr <= RAM_LAST) {
            return mem->read(buf, addr - RAM_FIRST, len);
        } else if (addr <= BIOS_LAST) {
            /*
             * XXX In case you were wondering: we don't check to see if
             * addr >= BIOS_FIRST because BIOS_FIRST is 0
             */
            if (addr + len > BIOS_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end"));
            }
            return bios->read(buf, addr - BIOS_FIRST, len);
        } else if (addr >= G1_FIRST && addr <= G1_LAST) {
            if (addr + len > G1_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end"));
            }
            return g1->read(buf, addr - G1_FIRST, len);
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
        } else if (addr <= BIOS_LAST) {
            /*
             * XXX In case you were wondering: we don't check to see if
             * addr >= BIOS_FIRST because BIOS_FIRST is 0
             */
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("Proper response for when "
                                                  "the guest tries to write to "
                                                  "read-only memory"));
        } else if (addr >= G1_FIRST && addr <= G1_LAST) {
            if (addr + len > G1_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end"));
            }
            return g1->write(buf, addr, len);
        }
    } catch(BaseException& exc) {
        exc << errinfo_guest_addr(addr);
        throw;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("memory mapping") <<
                          errinfo_guest_addr(addr));
}
