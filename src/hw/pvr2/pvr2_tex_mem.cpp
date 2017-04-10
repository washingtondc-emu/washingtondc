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

#include "MemoryMap.hpp"
#include "BaseException.hpp"

uint8_t pvr2_tex_mem[ADDR_TEX_LAST - ADDR_TEX_FIRST + 1];

int pvr2_tex_mem_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = pvr2_tex_mem + (addr - ADDR_TEX_FIRST);

    if (addr < ADDR_TEX_FIRST || addr > ADDR_TEX_LAST ||
        ((addr - 1 + len) > ADDR_TEX_LAST) ||
        ((addr - 1 + len) < ADDR_TEX_FIRST)) {
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("aw fuck it"));
    }

    memcpy(buf, start_addr, len);
    return 0;
}

int pvr2_tex_mem_write(void const *buf, size_t addr, size_t len) {
    void *start_addr = pvr2_tex_mem + (addr - ADDR_TEX_FIRST);

    if (addr < ADDR_TEX_FIRST || addr > ADDR_TEX_LAST ||
        ((addr - 1 + len) > ADDR_TEX_LAST) ||
        ((addr - 1 + len) < ADDR_TEX_FIRST)) {
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("aw fuck it"));
    }

    memcpy(start_addr, buf, len);
    return 0;
}
