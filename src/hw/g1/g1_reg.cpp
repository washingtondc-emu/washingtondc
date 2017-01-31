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

#include <cstring>
#include <iostream>

#include "g1.hpp"

#include "types.hpp"
#include "MemoryMap.hpp"
#include "BaseException.hpp"

struct G1Bus::MemMappedReg G1Bus::mem_mapped_regs[] = {
    // XXX this is supposed to be write-only, but currently it's readable
    { "SB_G1RRC", 0x005f7480, 0xffffffff, 4,
    &G1Bus::DefaultRegReadHandler, &G1Bus::DefaultRegWriteHandler },
    { "UNKNOWN", 0x005f74e4, 0xffffffff, 4,
      &G1Bus::WarnRegReadHandler, &G1Bus::WarnRegWriteHandler },
    { NULL }
};

G1Bus::G1Bus() {
    reg_area = new uint8_t[REG_AREA_SZ];
    memset(reg_area, 0, sizeof(uint8_t) * REG_AREA_SZ);
}

G1Bus::~G1Bus() {
    delete[] reg_area;
}

G1Bus::MemMappedReg *G1Bus::find_reg_by_addr(addr32_t addr) {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        if (curs->addr == (addr & curs->addr_mask))
            return curs;
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("accessing one of the "
                                          "G1 Bus control registers") <<
                          errinfo_guest_addr(addr));
}

int G1Bus::read(void *buf, size_t addr, size_t len) {
    MemMappedReg *mm_reg = find_reg_by_addr(addr);
    RegReadHandler handler = mm_reg->on_read;

    return (this->*handler)(buf, addr, len);
}

int G1Bus::write(void const *buf, size_t addr, size_t len) {
    MemMappedReg *mm_reg = find_reg_by_addr(addr);
    RegWriteHandler handler = mm_reg->on_write;

    return (this->*handler)(buf, addr, len);
}

int G1Bus::DefaultRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, addr - ADDR_G1_FIRST + reg_area, len);

    return 0;
}

int G1Bus::DefaultRegWriteHandler(void const *buf, addr32_t addr,
                                  unsigned len) {
    memcpy(addr - ADDR_G1_FIRST  + reg_area, buf, len);

    return 0;
}

int G1Bus::WarnRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    std::cerr << "WARNING: attempted " << len << "-byte read from G1 bus "
        "register 0x" << std::hex << addr << std::endl;
    return DefaultRegReadHandler(buf, addr, len);
}

int G1Bus::WarnRegWriteHandler(void const *buf, addr32_t addr, unsigned len) {
    std::cerr << "WARNING: attempted " << len << "-byte write to G1 bus "
        "register 0x" << std::hex << addr << std::endl;
    return DefaultRegWriteHandler(buf, addr, len);
}
