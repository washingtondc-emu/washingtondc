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
#include <iostream>
#include <iomanip>

#include "BaseException.hpp"
#include "sh4_excp.hpp"
#include "sh4_reg.hpp"
#include "sh4_tmu.hpp"
#include "sh4.hpp"

static struct Sh4MemMappedReg *find_reg_by_addr(addr32_t addr);

static struct Sh4MemMappedReg mem_mapped_regs[] = {
    { "EXPEVT", 0xff000024, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0x20 },
    { "INTEVT", 0xff000028, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0x20 },
    { "MMUCR", 0xff000010, ~addr32_t(0), 4, SH4_REG_MMUCR, false,
      Sh4MmucrRegReadHandler, Sh4MmucrRegWriteHandler, 0, 0 },
    { "CCR", 0xff00001c, ~addr32_t(0), 4, SH4_REG_CCR, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "QACR0", 0xff000038, ~addr32_t(0), 4, SH4_REG_QACR0, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "QACR1", 0xff00003c, ~addr32_t(0), 4, SH4_REG_QACR1, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEH", 0xff000000, ~addr32_t(0), 4, SH4_REG_PTEH, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEL", 0xff000004, ~addr32_t(0), 4, SH4_REG_PTEL, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TTB", 0xff000008, ~addr32_t(0), 4, SH4_REG_TTB, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TEA", 0xff00000c, ~addr32_t(0), 4, SH4_REG_TEA, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEA", 0xff000034, ~addr32_t(0), 4, SH4_REG_PTEA, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TRA", 0xff000020, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, false,
      Sh4TraRegReadHandler, Sh4TraRegWriteHandler, 0, 0 },

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
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BCR2", 0xff800004, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0x3ffc },
    { "WCR1", 0xff800008, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0x77777777 },
    { "WCR2", 0xff80000c, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0xfffeefff },
    { "WCR3", 0xff800010, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0x07777777 },
    { "MCR", 0xff800014, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PCR", 0xff800018, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PCTRA", 0xff80002c, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PDTRA", 0xff800030, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PCTRB", 0xff800040, ~addr32_t(0), 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PDTRB", 0xff800044, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "GPIOIC", 0xff800048, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RFCR", 0xff800028, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RTCOR", 0xff800024, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RTCSR", 0xff80001c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
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
      Sh4WriteOnlyRegReadHandler, Sh4IgnoreRegWriteHandler },
    { "SDMR3", 0xff940000, 0xffff0000, 1, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, Sh4IgnoreRegWriteHandler },

    /*
     * RTC registers
     * From what I can tell, it doesn't look like these actually get used
     * because they refer to the Sh4's internal RTC and not the Dreamcast's own
     * battery-powered RTC.
     */
    { "R64CNT", 0xffc80000, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "RSECCNT", 0xffc80004, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMINCNT", 0xffc80008, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RHRCNT", 0xffc8000c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RWKCNT", 0xffc80010, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RDAYCNT", 0xffc80014, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMONCNT", 0xffc80018, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RYRCNT", 0xffc8001c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RSECAR", 0xffc80020, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMINAR", 0xffc80024, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RHRAR", 0xffc80028, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RWKAR", 0xffc8002c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RDAYAR", 0xffc80030, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMONAR", 0xffc80034, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RCR1", 0xffc80038, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RCR2", 0xffc8003c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    /*
     * I'm not sure what this does - something to do with standby mode (which is
     * prohibited) and low-power-consumption mode (which isn't prohibited...?),
     * but the bios always writes 3 to it, which disables the clock source for
     * the RTC and the SCI.
     */
    { "STBCR", 0xffc00004, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "STBCR2", 0xffc00010, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

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
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "WTCSR", 0xffc0000c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    //The Timer Unit
    { "TOCR", 0xffd80000, ~addr32_t(0), 1, SH4_REG_TOCR, true,
      sh4_tmu_tocr_read_handler, sh4_tmu_tocr_write_handler, 1, 1 },
    { "TSTR", 0xffd80004, ~addr32_t(0), 1, SH4_REG_TSTR, true,
      sh4_tmu_tstr_read_handler, sh4_tmu_tstr_write_handler, 0, 0 },
    { "TCOR0", 0xffd80008, ~addr32_t(0), 4, SH4_REG_TCOR0, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT0", 0xffd8000c, ~addr32_t(0), 4, SH4_REG_TCNT0, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR0", 0xffd80010, ~addr32_t(0), 2, SH4_REG_TCR0, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCOR1", 0xffd80014, ~addr32_t(0), 4, SH4_REG_TCOR1, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT1", 0xffd80018, ~addr32_t(0), 4, SH4_REG_TCNT1, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR1", 0xffd8001c, ~addr32_t(0), 2, SH4_REG_TCR1, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCOR2", 0xffd80020, ~addr32_t(0), 4, SH4_REG_TCOR2, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCNT2", 0xffd80024, ~addr32_t(0), 4, SH4_REG_TCNT2, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~reg32_t(0), ~reg32_t(0) },
    { "TCR2", 0xffd80028, ~addr32_t(0), 2, SH4_REG_TCR2, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCPR2", 0xffd8002c, ~addr32_t(0), 4, SH4_REG_TCPR2, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },

    /* DMA Controller (DMAC) */
    { "SAR1", 0xffa00010, ~addr32_t(0), 4, SH4_REG_SAR1, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DAR1", 0xffa00014, ~addr32_t(0), 4, SH4_REG_DAR1, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DMATCR1", 0xffa00018, ~addr32_t(0), 4, SH4_REG_DMATCR1, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "CHCR1", 0xffa0001c, ~addr32_t(0), 4, SH4_REG_CHCR1, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "SAR2", 0xffa00020, ~addr32_t(0), 4, SH4_REG_SAR2, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DAR2", 0xffa00024, ~addr32_t(0), 4, SH4_REG_DAR2, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DMATCR2", 0xffa00028, ~addr32_t(0), 4, SH4_REG_DMATCR2, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "CHCR2", 0xffa0002c, ~addr32_t(0), 4, SH4_REG_CHCR2, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "SAR3", 0xffa00030, ~addr32_t(0), 4, SH4_REG_SAR3, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DAR2", 0xffa00034, ~addr32_t(0), 4, SH4_REG_DAR3, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DMATCR3", 0xffa00038, ~addr32_t(0), 4, SH4_REG_DMATCR3, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "CHCR3", 0xffa0003c, ~addr32_t(0), 4, SH4_REG_CHCR3, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },
    { "DMAOR", 0xffa00040, ~addr32_t(0), 4, SH4_REG_DMAOR, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },

    /*
     * Serial port - I know some of the homebrew stuff uses this, but AFAIK
     * it's not critical
     *
     * SCFSR2 and SCLSR2 are only supposed to be writeable if you write 0,
     * but I made them fully R/W
     */
    { "SCSMR2", 0xffe80000, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "SCBRR2", 0xffe80004, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0xff, 0xff },
    { "SCSCR2", 0xffe80008, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "SCFTDR2", 0xffe8000c, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, false,
      Sh4WriteOnlyRegReadHandler, Sh4IgnoreRegWriteHandler, 0xff, 0xff },
    { "SCFSR2", 0xffe80010, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0x0060, 0x0060 },
    { "SCFRDR2", 0xffe80014, ~addr32_t(0), 1, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "SCFCR2", 0xffe80018, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "SCFDR2", 0xffe8001c, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "SCSPTR2", 0xffe80020, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "SCLSR2", 0xffe80024, ~addr32_t(0), 2, (sh4_reg_idx_t)-1, false,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    { NULL }
};

void sh4_init_regs(Sh4 *sh4) {
    sh4_poweron_reset_regs(sh4);
}

void sh4_poweron_reset_regs(Sh4 *sh4) {
    Sh4MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        Sh4RegWriteHandler handler = curs->on_p4_write;

        if (handler != Sh4ReadOnlyRegWriteHandler) {
            if (handler(sh4, &curs->poweron_reset_val, curs) != 0)
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

void sh4_manual_reset_regs(Sh4 *sh4) {
    Sh4MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        Sh4RegWriteHandler handler = curs->on_p4_write;
        if (handler(sh4, &curs->manual_reset_val, curs) != 0)
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_wtf("the reg write handler returned "
                                              "error during a poweron reset") <<
                                  errinfo_guest_addr(curs->addr) <<
                                  errinfo_regname(curs->reg_name));

        curs++;
    }
}

static struct Sh4MemMappedReg *find_reg_by_addr(addr32_t addr) {
    struct Sh4MemMappedReg *curs = mem_mapped_regs;

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

int sh4_read_mem_mapped_reg(Sh4 *sh4, void *buf,
                            addr32_t addr, unsigned len) {
    struct Sh4MemMappedReg *mm_reg = find_reg_by_addr(addr);
    Sh4RegReadHandler handler = mm_reg->on_p4_read;

    if (len != mm_reg->len) {
        BOOST_THROW_EXCEPTION(InvalidParamError() <<
                              errinfo_length(len) <<
                              errinfo_guest_addr(addr));
    }

    return handler(sh4, buf, mm_reg);
}

int sh4_write_mem_mapped_reg(Sh4 *sh4, void const *buf,
                             addr32_t addr, unsigned len) {
    struct Sh4MemMappedReg *mm_reg = find_reg_by_addr(addr);
    Sh4RegWriteHandler handler = mm_reg->on_p4_write;

    if (len != mm_reg->len) {
        BOOST_THROW_EXCEPTION(InvalidParamError() <<
                              errinfo_length(len) <<
                              errinfo_guest_addr(addr));
    }

    return handler(sh4, buf, mm_reg);
}

int Sh4DefaultRegReadHandler(Sh4 *sh4, void *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    assert(reg_info->len <= sizeof(reg32_t));

    memcpy(buf, sh4->reg + reg_info->reg_idx, reg_info->len);

    return 0;
}

int Sh4DefaultRegWriteHandler(Sh4 *sh4, void const *buf,
                              struct Sh4MemMappedReg const *reg_info) {
    assert(reg_info->len <= sizeof(reg32_t));

    memcpy(sh4->reg + reg_info->reg_idx, buf, reg_info->len);

    return 0;
}

int Sh4IgnoreRegReadHandler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, reg_info->addr - SH4_P4_REGSTART + sh4->reg_area,
           reg_info->len);

    return 0;
}

int Sh4IgnoreRegWriteHandler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(reg_info->addr - SH4_P4_REGSTART + sh4->reg_area,
           buf, reg_info->len);

    return 0;
}

int Sh4WriteOnlyRegReadHandler(Sh4 *sh4, void *buf,
                               struct Sh4MemMappedReg const *reg_info) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sh4 CPU exception for trying to "
                                          "read from a write-only CPU "
                                          "register") <<
                          errinfo_guest_addr(reg_info->addr));
}

int Sh4ReadOnlyRegWriteHandler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sh4 CPU exception for trying to "
                                          "write to a read-only CPU "
                                          "register") <<
                          errinfo_guest_addr(reg_info->addr));
}

int Sh4WarnRegReadHandler(Sh4 *sh4, void *buf,
                          struct Sh4MemMappedReg const *reg_info) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = Sh4DefaultRegWriteHandler(sh4, buf, reg_info);

    if (ret_code) {
        std::cerr << "WARNING: read from register " <<
            reg_info->reg_name << std::endl;
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from register " <<
                reg_info->reg_name << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from register " <<
                reg_info->reg_name << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from register " <<
                reg_info->reg_name << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from register " <<
                reg_info->reg_name << std::endl;
        }
    }

    return ret_code;
}

int Sh4WarnRegWriteHandler(Sh4 *sh4, void const *buf,
                           struct Sh4MemMappedReg const *reg_info) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (reg_info->len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(2) <<
            unsigned(val8) << " to register " <<
            reg_info->reg_name << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to register " <<
            reg_info->reg_name << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to register " <<
            reg_info->reg_name << std::endl;
        break;
    default:
        std::cerr << "WARNING: reading from register " <<
            reg_info->reg_name << std::endl;
    }

    return Sh4DefaultRegWriteHandler(sh4, buf, reg_info);
}
