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

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include "BiosFile.hpp"

BiosFile::BiosFile() {
    dat = new uint8_t[SZ_EXPECT];
    dat_len = SZ_EXPECT;
    clear();
}

BiosFile::BiosFile(char const *path) {
    do_init(path);
}

BiosFile::BiosFile(const std::string &path) {
    do_init(path.c_str());
}

BiosFile::~BiosFile() {
    if (dat)
        delete[] dat;
}

void BiosFile::clear() {
    memset(dat, 0, dat_len);
}

void BiosFile::do_init(char const *path) {
    std::ifstream file(path, std::ifstream::in | std::ifstream::binary);

    file.seekg(0, file.end);
    size_t len = file.tellg();
    file.seekg(0, file.beg);

    dat = new uint8_t[len];
    file.read((char*)dat, sizeof(uint8_t) * len);
    dat_len = len;

    if (dat_len != SZ_EXPECT) {
        std::cout << "WARNING - unexpected bios size (expected " <<
            SZ_EXPECT << ", got " << dat_len << ").  This BIOS will " <<
            "still be loaded but you should be aware that things may get " <<
            "funky" << std::endl;
    }
}

uint8_t *BiosFile::begin() {
    return dat;
}

uint8_t *BiosFile::end() {
    return dat + dat_len;
}

int BiosFile::read(void *buf, size_t addr, size_t len) const {
    memcpy(buf, dat + addr, len);
    return 0;
}
