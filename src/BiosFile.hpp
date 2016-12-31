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

#ifndef BIOSFILE_HPP_
#define BIOSFILE_HPP_

#include <string>
#include <vector>
#include <boost/cstdint.hpp>

#include "types.hpp"
#include "BaseException.hpp"

class BiosFile {
public:
    /*
     * default constructor allocates SZ_EXPECT bytes and zeroes it out.
     * the other constructors load the file indicated by path.
     */
    BiosFile();
    BiosFile(char const *path);
    BiosFile(const std::string &path);
    ~BiosFile();

    uint8_t *begin();
    uint8_t *end();

    int read(void *buf, size_t addr, size_t len) const;

    /*
     * loads a program into the given address.  the InputIterator's
     * indirect method (overload*) should return a data_tp.
     */
    template<typename data_tp, class InputIterator>
    void load_binary(addr32_t where, InputIterator start, InputIterator end);

    void clear();

private:
    static const size_t SZ_EXPECT = 0x1fffff + 1;

    void do_init(char const *path);

    size_t dat_len;
    uint8_t *dat;
};

template<typename data_tp, class InputIterator>
void BiosFile::load_binary(addr32_t where, InputIterator start,
                           InputIterator end) {
    size_t bytes_written = 0;

    clear();

    for (InputIterator it = start; it != end; it++) {
        data_tp tmp = *it;

        if (bytes_written + sizeof(data_tp) >= dat_len)
            BOOST_THROW_EXCEPTION(InvalidParamError());

        memcpy(dat + bytes_written, &tmp, sizeof(tmp));
        bytes_written += sizeof(tmp);
    }
}

#endif
