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

#include <cstring>

#include "BaseException.hpp"
#include "sh4.hpp"

typedef boost::error_info<struct tag_feature_name_error_info, std::string>
errinfo_regname;

struct Sh4::MemMappedReg Sh4::mem_mapped_regs[] = {
    { "EXPEVT", 0xff000024, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0x20 },
    { "INTEVT", 0xff000028, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0x20 },
    { "MMUCR", 0xff000010, ~addr32_t(0), 4, SH4_REG_MMUCR, false,
      &Sh4::MmucrRegReadHandler, &Sh4::MmucrRegWriteHandler, 0, 0 },
    { "CCR", 0xff00001c, ~addr32_t(0), 4, SH4_REG_CCR, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "QACR0", 0xff000038, ~addr32_t(0), 4, SH4_REG_QACR0, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "QACR1", 0xff00003c, ~addr32_t(0), 4, SH4_REG_QACR1, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "PTEH", 0xff000000, ~addr32_t(0), 4, SH4_REG_PTEH, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "PTEL", 0xff000004, ~addr32_t(0), 4, SH4_REG_PTEL, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "TTB", 0xff000008, ~addr32_t(0), 4, SH4_REG_TTB, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "TEA", 0xff00000c, ~addr32_t(0), 4, SH4_REG_TEA, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "PTEA", 0xff000034, ~addr32_t(0), 4, SH4_REG_PTEA, false,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "TRA", 0xff000020, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      &Sh4::TraRegReadHandler, &Sh4::TraRegWriteHandler, 0, 0 },


    /*
     * Bus-state registers.
     *
     * These all seem pretty low-level, so we just blindly let
     * read/write operations pass through and don't do anything
     * to react to them.
     *
     * I *am* a bit worried about ignoring GPIOIC, though.  It sounds like that
     * one might be important, but I'm just not show how (or if) I should
     * handle it at this point.
     */
    { "BCR1", 0xff800000, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "BCR2", 0xff800004, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0x3ffc },
    { "WCR1", 0xff800008, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0x77777777 },
    { "WCR2", 0xff80000c, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0xfffeefff },
    { "WCR3", 0xff800010, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0x07777777 },
    { "MCR", 0xff800014, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "PCR", 0xff800018, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "PCTRA", 0xff80002c, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "PDTRA", 0xff800030, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "PCTRB", 0xff800040, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "PDTRB", 0xff800044, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "GPIOIC", 0xff800048, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "RFCR", 0xff800028, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "RTCOR", 0xff800024, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
      0, 0 },
    { "RTCSR", 0xff80001c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler,
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
    { "SDMR2", 0xff900000, 0xffff0000, 1, (sh4_reg_idx_t)-1, true,
      &Sh4::WriteOnlyRegReadHandler, &Sh4::IgnoreRegWriteHandler },
    { "SDMR3", 0xff940000, 0xffff0000, 1, (sh4_reg_idx_t)-1, true,
      &Sh4::WriteOnlyRegReadHandler, &Sh4::IgnoreRegWriteHandler },

    /*
     * RTC registers
     * From what I can tell, it doesn't look like these actually get used
     * because they refer to the Sh4's internal RTC and not the Dreamcast's own
     * battery-powered RTC.
     */
    { "R64CNT", 0xffc80000, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::ReadOnlyRegWriteHandler, 0, 0 },
    { "RSECCNT", 0xffc80004, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RMINCNT", 0xffc80008, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RHRCNT", 0xffc8000c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RWKCNT", 0xffc80010, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RDAYCNT", 0xffc80014, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RMONCNT", 0xffc80018, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RYRCNT", 0xffc8001c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RSECAR", 0xffc80020, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RMINAR", 0xffc80024, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RHRAR", 0xffc80028, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RWKAR", 0xffc8002c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RDAYAR", 0xffc80030, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RMONAR", 0xffc80034, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RCR1", 0xffc80038, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "RCR2", 0xffc8003c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },

    /*
     * I'm not sure what this does - something to do with standby mode (which is
     * prohibited) and low-power-consumption mode (which isn't prohibited...?),
     * but the bios always writes 3 to it, which disables the clock source for
     * the RTC and the SCI.
     */
    { "STBCR", 0xffc00004, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "STBCR2", 0xffc00010, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },

    /*
     * watchdog timer - IDK if this is needed.
     * If it's like other watchdog timers I've encountered in my travels then
     * all it does is it resets the system when it thinks it might be hanging.
     *
     * These two registers are supposed to be 16-bits when reading and 8-bits
     * when writing - I only support 16-bit accesses right now which is wrong
     * but hopefully inconsequential.
     */
    { "WTCNT", 0xffc00008, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },
    { "WTCSR", 0xffc0000c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      &Sh4::IgnoreRegReadHandler, &Sh4::IgnoreRegWriteHandler, 0, 0 },

    //The Timer Unit
    { "TOCR", 0xffd80000, ~addr32_t(0), 1, SH4_REG_TOCR, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "TSTR", 0xffd80004, ~addr32_t(0), 1, SH4_REG_TSTR, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },
    { "TCOR0", 0xffd80008, ~addr32_t(0), 4, SH4_REG_TCOR0, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT0", 0xffd8000c, ~addr32_t(0), 4, SH4_REG_TCNT0, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR0", 0xffd80010, ~addr32_t(0), 2, SH4_REG_TCR0, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCOR1", 0xffd80014, ~addr32_t(0), 4, SH4_REG_TCOR1, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT1", 0xffd80018, ~addr32_t(0), 4, SH4_REG_TCNT1, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR1", 0xffd8001c, ~addr32_t(0), 2, SH4_REG_TCR1, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCOR2", 0xffd80020, ~addr32_t(0), 4, SH4_REG_TCOR2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT2", 0xffd80024, ~addr32_t(0), 4, SH4_REG_TCNT2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR2", 0xffd80028, ~addr32_t(0), 2, SH4_REG_TCR2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCPR2", 0xffd8002c, ~addr32_t(0), 4, SH4_REG_TCPR2, true,
      &Sh4::DefaultRegReadHandler, &Sh4::DefaultRegWriteHandler, 0, 0 },

    { NULL }
};

void Sh4::init_regs() {
    poweron_reset_regs();
}

void Sh4::poweron_reset_regs() {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        RegWriteHandler handler = curs->on_p4_write;

        if (handler != &Sh4::ReadOnlyRegWriteHandler) {
            if ((this->*handler)(&curs->poweron_reset_val, curs) != 0)
                BOOST_THROW_EXCEPTION(IntegrityError() <<
                                      errinfo_wtf("the reg write handler "
                                                  "returned error during a "
                                                  "poweron reset") <<
                                      errinfo_guest_addr(curs->addr) <<
                                      errinfo_regname(curs->reg_name));
        }
        curs++;
    }
}

void Sh4::manual_reset_regs() {
    MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        RegWriteHandler handler = curs->on_p4_write;
        if ((this->*handler)(&curs->manual_reset_val, curs) != 0)
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

    return (this->*handler)(buf, mm_reg);
}

int Sh4::write_mem_mapped_reg(void const *buf, addr32_t addr, unsigned len) {
    MemMappedReg *mm_reg = find_reg_by_addr(addr);
    RegWriteHandler handler = mm_reg->on_p4_write;

    return (this->*handler)(buf, mm_reg);
}

int Sh4::DefaultRegReadHandler(void *buf, struct Sh4::MemMappedReg const *reg_info) {
    assert(reg_info->len <= sizeof(reg32_t));

    memcpy(buf, reg + reg_info->reg_idx, reg_info->len);

    return 0;
}

int Sh4::DefaultRegWriteHandler(void const *buf,
                                struct Sh4::MemMappedReg const *reg_info) {
    assert(reg_info->len <= sizeof(reg32_t));

    memcpy(reg + reg_info->reg_idx, buf, reg_info->len);

    return 0;
}

int Sh4::IgnoreRegReadHandler(void *buf, struct Sh4::MemMappedReg const *reg_info) {
    memcpy(buf, reg_info->addr - P4_REGSTART + reg_area, reg_info->len);

    return 0;
}

int Sh4::IgnoreRegWriteHandler(void const *buf, struct Sh4::MemMappedReg const *reg_info) {
    memcpy(reg_info->addr - P4_REGSTART + reg_area, buf, reg_info->len);

    return 0;
}

int Sh4::WriteOnlyRegReadHandler(void *buf, struct Sh4::MemMappedReg const *reg_info) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sh4 CPU exception for trying to "
                                          "read from a write-only CPU "
                                          "register") <<
                          errinfo_guest_addr(reg_info->addr));
}

int Sh4::ReadOnlyRegWriteHandler(void const *buf, struct Sh4::MemMappedReg const *reg_info) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sh4 CPU exception for trying to "
                                          "write to a read-only CPU "
                                          "register") <<
                          errinfo_guest_addr(reg_info->addr));
}

int Sh4::MmucrRegReadHandler(void *buf, struct Sh4::MemMappedReg const *reg_info) {
    memcpy(buf, reg + SH4_REG_MMUCR, sizeof(reg[SH4_REG_MMUCR]));

    return 0;
}

int Sh4::MmucrRegWriteHandler(void const *buf, struct Sh4::MemMappedReg const *reg_info) {
    reg32_t mmucr_tmp;
    memcpy(&mmucr_tmp, buf, sizeof(mmucr_tmp));

    if (mmucr_tmp & SH4_MMUCR_AT_MASK) {
        /*
         * The thing is, I have a lot of code to support MMU operation in place,
         * but it's not all tested and I also don't think I have all the
         * functionality in place.  MMU support is definitely something I want
         * to do eventuaally and it's something I always have in mind when
         * writing new code, but it's just not there yet.
         */
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_regname("MMUCR") <<
                              errinfo_guest_addr(reg_info->addr));
    }

    reg[SH4_REG_MMUCR] = mmucr_tmp;

    return 0;
}

int Sh4::TraRegReadHandler(void *buf,
                           struct Sh4::MemMappedReg const *reg_info) {
    memcpy(buf, &reg[SH4_REG_TRA], sizeof(reg[SH4_REG_TRA]));

    return 0;
}

int Sh4::TraRegWriteHandler(void const *buf,
                            struct Sh4::MemMappedReg const *reg_info) {
    memcpy(&reg[SH4_REG_TRA], buf, sizeof(reg[SH4_REG_TRA]));

    return 0;
}

int Sh4::ExpevtRegReadHandler(void *buf,
                              struct Sh4::MemMappedReg const *reg_info) {
    memcpy(buf, &reg[SH4_REG_EXPEVT], sizeof(reg[SH4_REG_EXPEVT]));

    return 0;
}

int Sh4::ExpevtRegWriteHandler(void const *buf,
                               struct Sh4::MemMappedReg const *reg_info) {
    memcpy(&reg[SH4_REG_EXPEVT], buf, sizeof(reg[SH4_REG_EXPEVT]));

    return 0;
}

int Sh4::IntevtRegReadHandler(void *buf,
                              struct Sh4::MemMappedReg const *reg_info) {
    memcpy(buf, &reg[SH4_REG_INTEVT], sizeof(reg[SH4_REG_INTEVT]));

    return 0;
}

int Sh4::IntevtRegWriteHandler(void const *buf,
                               struct Sh4::MemMappedReg const *reg_info) {
    memcpy(&reg[SH4_REG_INTEVT], buf, sizeof(reg[SH4_REG_INTEVT]));

    return 0;
}
