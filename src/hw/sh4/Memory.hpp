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

#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <boost/cstdint.hpp>

// Temporary memory emulator
// This module will be used as a "dumb" memory device for the Sh4 interpreter
// core to interact with during testing while I get this emulator bootstrapped.
// It does not implement any of the Dreamcast's memory mappings.
// Later it might stay on as part of some sort of unit testing system.

class Memory {
public:
    Memory(size_t size);
    ~Memory();

    int read(void *buf, size_t addr, size_t len) const;
    int write(void const *buf, size_t addr, size_t len);

    size_t get_size() const;
private:
    size_t size;
    boost::uint8_t *mem;
};

#endif
