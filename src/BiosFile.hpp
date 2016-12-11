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

class BiosFile {
public:
    BiosFile(char const *path);
    BiosFile(const std::string &path);
    ~BiosFile();

    uint8_t *begin();
    uint8_t *end();

private:
    static const size_t SZ_EXPECT = 0x1fffff + 1;

    void do_init(char const *path);

    size_t dat_len;
    uint8_t *dat;
};

#endif
