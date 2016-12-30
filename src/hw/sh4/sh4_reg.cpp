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
    { "EXPEVT", 0xff000024, ~addr32_t(0), 4, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0x20 },
    { "MMUCR", 0xff000010, ~addr32_t(0), 4, false,
      &Sh4::MmucrRegReadHandler, &Sh4::MmucrRegWriteHandler, 0, 0 },
    { "CCR", 0xff00001c, ~addr32_t(0), 4, false,
      &Sh4::CcrRegReadHandler, &Sh4::CcrRegWriteHandler, 0, 0 },

    /*
     * Bus-state registers.
     *
     * These all seem pretty low-level, so we just blindly let
     * read/write operations pass through and don't do anything
     * to react to them.
     */
    { "BCR1", 0xff800000, ~addr32_t(0), 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "BCR2", 0xff800004, ~addr32_t(0), 2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0x3ffc },
    { "WCR1", 0xff800008, ~addr32_t(0), 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0x77777777 },
    { "WCR2", 0xff80000c, ~addr32_t(0), 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0xfffeefff },
    { "WCR3", 0xff800010, ~addr32_t(0), 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0x07777777 },
    { "MCR", 0xff800014, ~addr32_t(0), 4, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0 },
    { "RFCR", 0xff800028, ~addr32_t(0), 2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0 },
    { "RTCOR", 0xff800024, ~addr32_t(0), 2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0 },
    { "RTCSR", 0xff80001c, ~addr32_t(0), 2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      0, 0 },

    /*
     * These two registers are kind of weird.  When you write to them, the value
     * is discarded and instead the offset from the beginning of the register
     * (either 0xff900000 for SDMR2 or 0xff940000 for SDMR3) is right-shifted
     * by 2 and that is used as the value instead.
     *
     * Like the other bus-state control registers, I've decided that these
     * registers are low-level enough that they can *probably* be ignored.
     * I've allowed all writes to transparently pass through.
     * The current implementation does not respect the unusual addressing
     * described above.  It does make the register write-only (as described in
     * the spec), which is why I feel like I don't need to bother with the
     * weird address-as-value semantics of these registers.
     */
    { "SDMR2", 0xff900000, 0xffff0000, 1, true,
      &Sh4::WriteOnlyRegReadHandler, &Sh4::DefaultRegWriteHandler },
    { "SDMR3", 0xff940000, 0xffff0000, 1, true,
      &Sh4::WriteOnlyRegReadHandler, &Sh4::DefaultRegWriteHandler },

    { NULL }
};

void Sh4::init_regs() {
    poweron_reset_regs();
}

void Sh4::poweron_reset_regs() {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        RegWriteHandler handler = curs->on_p4_write;
        if ((this->*handler)(&curs->poweron_reset_val,
                             curs->addr, curs->len) != 0)
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_wtf("the reg write handler returned "
                                              "error during a poweron reset") <<
                                  errinfo_guest_addr(curs->addr) <<
                                  errinfo_regname(curs->reg_name));

        curs++;
    }
}

void Sh4::manual_reset_regs() {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        RegWriteHandler handler = curs->on_p4_write;
        if ((this->*handler)(&curs->manual_reset_val,
                             curs->addr, curs->len) != 0)
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_wtf("the reg write handler returned "
                                              "error during a poweron reset") <<
                                  errinfo_guest_addr(curs->addr) <<
                                  errinfo_regname(curs->reg_name));

        curs++;
    }
}

Sh4::MemMappedReg *Sh4::find_reg_by_addr(addr32_t addr) {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        if (curs->addr == (addr & curs->addr_mask))
            return curs;
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("accessing one of the "
                                          "mem-mapped registers") <<
                          errinfo_guest_addr(addr));
}

int Sh4::read_mem_mapped_reg(void *buf, addr32_t addr, unsigned len) {
    MemMappedReg *mm_reg = find_reg_by_addr(addr);
    RegReadHandler handler = mm_reg->on_p4_read;

    return (this->*handler)(buf, addr, len);
}

int Sh4::write_mem_mapped_reg(void const *buf, addr32_t addr, unsigned len) {
    MemMappedReg *mm_reg = find_reg_by_addr(addr);
    RegWriteHandler handler = mm_reg->on_p4_write;

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

int Sh4::WriteOnlyRegReadHandler(void *buf, addr32_t addr, unsigned len) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sh4 CPU exception for trying to "
                                          "read from a write-only CPU "
                                          "register") <<
                          errinfo_guest_addr(addr));
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
