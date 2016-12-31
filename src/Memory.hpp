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

#include "types.hpp"

class Memory {
public:
    Memory(size_t size);
    ~Memory();

    int read(void *buf, size_t addr, size_t len) const;
    int write(void const *buf, size_t addr, size_t len);

    void clear();

    size_t get_size() const;

    /*
     * loads a program into the given address.  the InputIterator's
     * indirect method (overload*) should return an inst_t.
     */
    template<typename data_tp, class InputIterator>
    void load_binary(addr32_t where, InputIterator start, InputIterator end);

private:
    size_t size;
    boost::uint8_t *mem;
};

template<typename data_tp, class InputIterator>
void Memory::load_binary(addr32_t where, InputIterator start,
                         InputIterator end) {
    for (InputIterator it = start; it != end; it++, where++) {
        data_tp tmp = *it;
        write(&tmp, where, sizeof(tmp));
    }
}

#endif
