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
#include "sh4.hpp"

typedef boost::error_info<struct tag_feature_name_error_info, std::string>
errinfo_regname;

struct Sh4::MemMappedReg Sh4::mem_mapped_regs[] = {
    { "EXPEVT", 0xff000024, 4, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0x20 },
    { "MMUCR", 0xff000010, 4, false,
      &Sh4::MmucrRegReadHandler, &Sh4::MmucrRegWriteHandler, 0, 0 },
    { "CCR", 0xff00001c, 4, false,
      &Sh4::CcrRegReadHandler, &Sh4::CcrRegWriteHandler, 0, 0 },

    /*
     * Bus-state registers.
     *
     * These all seem pretty low-level, so we just blindly let
     * read/write operations pass through and don't do anything
     * to react to them.
     */
    { "BCR1", 0xff800000, 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "BCR2", 0xff800004, 2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0x3ffc },
    { "WCR1", 0xff800008, 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0x77777777 },
    { "WCR2", 0xff80000c, 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0xfffeefff },
    { "WCR3", 0xff800010, 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0x07777777 },
    { "MCR", 0xff800014, 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0 },

    { NULL }
};


void Sh4::init_regs() {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        reg_map[curs->addr] = *curs;

        curs++;
    }

    poweron_reset_regs();
}

void Sh4::poweron_reset_regs() {
    for (RegMetaMap::iterator it = reg_map.begin(); it != reg_map.end(); it++) {
        RegWriteHandler handler = it->second.on_p4_write;
        if ((this->*handler)(&it->second.poweron_reset_val,
                             it->second.addr, it->second.len) != 0)
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_wtf("the reg write handler returned "
                                              "error during a poweron reset") <<
                                  errinfo_guest_addr(it->second.addr) <<
                                  errinfo_regname(it->second.reg_name));
    }
}

void Sh4::manual_reset_regs() {
    for (RegMetaMap::iterator it = reg_map.begin(); it != reg_map.end(); it++) {
        RegWriteHandler handler = it->second.on_p4_write;
        if ((this->*handler)(&it->second.manual_reset_val,
                             it->second.addr, it->second.len) != 0)
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_wtf("the reg write handler returned "
                                              "error during a manual reset") <<
                                  errinfo_guest_addr(it->second.addr) <<
                                  errinfo_regname(it->second.reg_name));
    }
}

int Sh4::read_mem_mapped_reg(void *buf, addr32_t addr, unsigned len) {
    RegMetaMap::iterator pos = reg_map.find(addr);

    if (pos == reg_map.end()) {
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("reading from one of the "
                                              "mem-mapped registers") <<
                              errinfo_guest_addr(addr));
    }

    RegReadHandler handler = pos->second.on_p4_read;

    return (this->*handler)(buf, addr, len);
}

int Sh4::write_mem_mapped_reg(void const *buf, addr32_t addr, unsigned len) {
    RegMetaMap::iterator pos = reg_map.find(addr);

    if (pos == reg_map.end()) {
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("writing to one of the "
                                              "mem-mapped registers") <<
                              errinfo_guest_addr(addr));
    }

    RegWriteHandler handler = pos->second.on_p4_write;

    return (this->*handler)(buf, addr, len);
}


int Sh4::DefaultRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, addr - P4_REGSTART + reg_area, len);

    return 0;
}

int Sh4::DefaultRegWriteHandler(void const *buf, addr32_t addr, unsigned len) {
    memcpy(addr - P4_REGSTART + reg_area, buf, len);

    return 0;
}

int Sh4::MmucrRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &mmu.mmucr, sizeof(mmu.mmucr));

    return 0;
}

int Sh4::MmucrRegWriteHandler(void const *buf, addr32_t addr, unsigned len) {
    reg32_t mmucr_tmp;
    memcpy(&mmucr_tmp, buf, sizeof(mmucr_tmp));

    if (mmucr_tmp & MMUCR_AT_MASK) {
        /*
         * The thing is, I have a lot of code to support MMU operation in place,
         * but it's not all tested and I also don't think I have all the
         * functionality in place.  MMU support is definitely something I want
         * to do eventuaally and it's something I always have in mind when
         * writing new code, but it's just not there yet.
         */
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_regname("MMUCR") <<
                              errinfo_guest_addr(addr));
    }

    mmu.mmucr = mmucr_tmp;

    return 0;
}

int Sh4::CcrRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &cache_reg.ccr, sizeof(cache_reg.ccr));

    return 0;
}

int Sh4::CcrRegWriteHandler(void const *buf, addr32_t addr, unsigned len) {
    memcpy(&cache_reg.ccr, buf, sizeof(cache_reg.ccr));

    return 0;
}
