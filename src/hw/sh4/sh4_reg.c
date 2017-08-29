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

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sh4_excp.h"
#include "sh4_reg.h"
#include "sh4_tmu.h"
#include "sh4_dmac.h"
#include "sh4.h"

static struct Sh4MemMappedReg *find_reg_by_addr(addr32_t addr);

static int sh4_pdtra_reg_read_handler(Sh4 *sh4, void *buf,
                                      struct Sh4MemMappedReg const *reg_info);
static int sh4_pdtra_reg_write_handler(Sh4 *sh4, void const *buf,
                                       struct Sh4MemMappedReg const *reg_info);

static int sh4_id_reg_read_handler(Sh4 *sh4, void *buf,
                                   struct Sh4MemMappedReg const *reg_info);

static int sh4_mmucr_reg_write_handler(Sh4 *sh4, void const *buf,
                                       struct Sh4MemMappedReg const *reg_info);

static int
sh4_zero_only_reg_write_handler(Sh4 *sh4, void const *buf,
                                struct Sh4MemMappedReg const *reg_info);

/*
 * SDMR2 and SDMR3 are  weird.  When you write to them, the value
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
 *
 * As for the weird address-as-data setup, I've chosen to implement these two
 * registers as a special case after all other registers have failed.  Both of
 * these registers occupy a 64k address-space so making 64k/4 registers is out
 * of the question.  I used to implement this by giving every register a mask
 * and address, but then I realized that these two registers are the only ones
 * using that infrastructure.  I'd rather not drag all these registers down just
 * for the sake of two which are almost never used.
 */
#define SH4_REG_SDMR2_ADDR 0xff900000
#define SH4_REG_SDMR3_ADDR 0xff940000
#define SH4_REG_SDMR2_MASK 0xffff0000
#define SH4_REG_SDMR3_MASK 0xffff0000

static struct Sh4MemMappedReg sh4_sdmr2_reg = {
    "SDMR2", 0xff900000, 1, (sh4_reg_idx_t)-1, true,
    Sh4WriteOnlyRegReadHandler, Sh4IgnoreRegWriteHandler };
static struct Sh4MemMappedReg sh4_sdmr3_reg = {
    "SDMR3", 0xff940000, 1, (sh4_reg_idx_t)-1, true,
    Sh4WriteOnlyRegReadHandler, Sh4IgnoreRegWriteHandler };

static struct Sh4MemMappedReg mem_mapped_regs[] = {
    { "EXPEVT", 0xff000024, 4, SH4_REG_EXPEVT, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0x20 },
    { "INTEVT", 0xff000028, 4, SH4_REG_INTEVT, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0x20 },
    { "MMUCR", 0xff000010, 4, SH4_REG_MMUCR, false,
      Sh4WarnRegReadHandler, sh4_mmucr_reg_write_handler, 0, 0 },
    { "CCR", 0xff00001c, 4, SH4_REG_CCR, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "QACR0", 0xff000038, 4, SH4_REG_QACR0, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "QACR1", 0xff00003c, 4, SH4_REG_QACR1, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEH", 0xff000000, 4, SH4_REG_PTEH, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEL", 0xff000004, 4, SH4_REG_PTEL, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TTB", 0xff000008, 4, SH4_REG_TTB, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TEA", 0xff00000c, 4, SH4_REG_TEA, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "PTEA", 0xff000034, 4, SH4_REG_PTEA, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "TRA", 0xff000020, 4, SH4_REG_TRA, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },

    /*
     * this is an odd one.  This register doesn't appear in any documentation
     * I have on hand, but from what I can gleam it's some sort of read-only
     * register that can be used to determine what specific SuperH CPU model
     * your program is running on.  Dreamcast BIOS checks this for some reason
     * even though there's only one CPU it could possibly be running on.
     *
     * The handler for this register simply returns a constant value I got by
     * running a program on my dreamcast that prints this register to the
     * framebuffer.
     */
    { "SUPERH-ID", 0xff000030, 4, (sh4_reg_idx_t)-1, false,
      sh4_id_reg_read_handler, Sh4ReadOnlyRegWriteHandler, 0, 0 },

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
    { "BCR1", 0xff800000, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BCR2", 0xff800004, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0x3ffc },
    { "WCR1", 0xff800008, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0x77777777 },
    { "WCR2", 0xff80000c, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0xfffeefff },
    { "WCR3", 0xff800010, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0x07777777 },
    { "MCR", 0xff800014, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PCR", 0xff800018, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "PCTRA", 0xff80002c, 4, SH4_REG_PCTRA, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler,
      0, 0 },
    { "PDTRA", 0xff800030, 2, SH4_REG_PDTRA, true,
      sh4_pdtra_reg_read_handler, sh4_pdtra_reg_write_handler,
      0, 0 },
    { "PCTRB", 0xff800040, 4, SH4_REG_PCTRB, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler,
      0, 0 },
    { "PDTRB", 0xff800044, 2, SH4_REG_PDTRB, true,
      Sh4IgnoreRegReadHandler, Sh4WarnRegWriteHandler,
      0, 0 },
    { "GPIOIC", 0xff800048, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RFCR", 0xff800028, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RTCOR", 0xff800024, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },
    { "RTCSR", 0xff80001c, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler,
      0, 0 },

    /*
     * RTC registers
     * From what I can tell, it doesn't look like these actually get used
     * because they refer to the Sh4's internal RTC and not the Dreamcast's own
     * battery-powered RTC.
     */
    { "R64CNT", 0xffc80000, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "RSECCNT", 0xffc80004, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMINCNT", 0xffc80008, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RHRCNT", 0xffc8000c, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RWKCNT", 0xffc80010, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RDAYCNT", 0xffc80014, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMONCNT", 0xffc80018, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RYRCNT", 0xffc8001c, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RSECAR", 0xffc80020, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMINAR", 0xffc80024, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RHRAR", 0xffc80028, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RWKAR", 0xffc8002c, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RDAYAR", 0xffc80030, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RMONAR", 0xffc80034, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RCR1", 0xffc80038, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "RCR2", 0xffc8003c, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    /*
     * I'm not sure what this does - something to do with standby mode (which is
     * prohibited) and low-power-consumption mode (which isn't prohibited...?),
     * but the bios always writes 3 to it, which disables the clock source for
     * the RTC and the SCI.
     */
    { "STBCR", 0xffc00004, 1, SH4_REG_STBCR, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "STBCR2", 0xffc00010, 1, (sh4_reg_idx_t)-1, true,
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
    { "WTCNT", 0xffc00008, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "WTCSR", 0xffc0000c, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    //The Timer Unit
    { "TOCR", 0xffd80000, 1, SH4_REG_TOCR, true,
      sh4_tmu_tocr_read_handler, sh4_tmu_tocr_write_handler, 1, 1 },
    { "TSTR", 0xffd80004, 1, SH4_REG_TSTR, true,
      sh4_tmu_tstr_read_handler, sh4_tmu_tstr_write_handler, 0, 0 },
    { "TCOR0", 0xffd80008, 4, SH4_REG_TCOR0, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCNT0", 0xffd8000c, 4, SH4_REG_TCNT0, true,
      sh4_tmu_tcnt_read_handler, sh4_tmu_tcnt_write_handler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCR0", 0xffd80010, 2, SH4_REG_TCR0, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCOR1", 0xffd80014, 4, SH4_REG_TCOR1, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCNT1", 0xffd80018, 4, SH4_REG_TCNT1, true,
      sh4_tmu_tcnt_read_handler, sh4_tmu_tcnt_write_handler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCR1", 0xffd8001c, 2, SH4_REG_TCR1, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCOR2", 0xffd80020, 4, SH4_REG_TCOR2, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCNT2", 0xffd80024, 4, SH4_REG_TCNT2, true,
      sh4_tmu_tcnt_read_handler, sh4_tmu_tcnt_write_handler,
      ~((reg32_t)0), ~((reg32_t)0) },
    { "TCR2", 0xffd80028, 2, SH4_REG_TCR2, true,
      sh4_tmu_tcr_read_handler, sh4_tmu_tcr_write_handler, 0, 0 },
    { "TCPR2", 0xffd8002c, 4, SH4_REG_TCPR2, true,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },

    /*
     * DMA channel 0
     *
     * software should not attempt to access this because it is controlled by
     * hardware.  I have seen some programs will zero this out, so I do allow
     * that through as long as it only writes 0.  I'm not sure what the effect
     * of this would be on real hardware, or if it even has an effect.
     */
    { "SAR0", 0xffa00000, 4, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "DAR0", 0xffa00004, 4, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "DMATCR0", 0xffa00008, 4, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "CHCR0", 0xffa0000c, 4, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },


    /* DMA Controller (DMAC) */
    { "SAR1", 0xffa00010, 4, SH4_REG_SAR1, true,
      sh4_dmac_sar_reg_read_handler, sh4_dmac_sar_reg_write_handler, 0, 0 },
    { "DAR1", 0xffa00014, 4, SH4_REG_DAR1, true,
      sh4_dmac_dar_reg_read_handler, sh4_dmac_dar_reg_write_handler, 0, 0 },
    { "DMATCR1", 0xffa00018, 4, SH4_REG_DMATCR1, true,
      sh4_dmac_dmatcr_reg_read_handler, sh4_dmac_dmatcr_reg_write_handler, 0, 0 },
    { "CHCR1", 0xffa0001c, 4, SH4_REG_CHCR1, true,
      sh4_dmac_chcr_reg_read_handler, sh4_dmac_chcr_reg_write_handler, 0, 0 },
    { "SAR2", 0xffa00020, 4, SH4_REG_SAR2, true,
      sh4_dmac_sar_reg_read_handler, sh4_dmac_sar_reg_write_handler, 0, 0 },
    { "DAR2", 0xffa00024, 4, SH4_REG_DAR2, true,
      sh4_dmac_dar_reg_read_handler, sh4_dmac_dar_reg_write_handler, 0, 0 },
    { "DMATCR2", 0xffa00028, 4, SH4_REG_DMATCR2, true,
      sh4_dmac_dmatcr_reg_read_handler, sh4_dmac_dmatcr_reg_write_handler, 0, 0 },
    { "CHCR2", 0xffa0002c, 4, SH4_REG_CHCR2, true,
      sh4_dmac_chcr_reg_read_handler, sh4_dmac_chcr_reg_write_handler, 0, 0 },
    { "SAR3", 0xffa00030, 4, SH4_REG_SAR3, true,
      sh4_dmac_sar_reg_read_handler, sh4_dmac_sar_reg_write_handler, 0, 0 },
    { "DAR3", 0xffa00034, 4, SH4_REG_DAR3, true,
      sh4_dmac_dar_reg_read_handler, sh4_dmac_dar_reg_write_handler, 0, 0 },
    { "DMATCR3", 0xffa00038, 4, SH4_REG_DMATCR3, true,
      sh4_dmac_dmatcr_reg_read_handler, sh4_dmac_dmatcr_reg_write_handler, 0, 0 },
    { "CHCR3", 0xffa0003c, 4, SH4_REG_CHCR3, true,
      sh4_dmac_chcr_reg_read_handler, sh4_dmac_chcr_reg_write_handler, 0, 0 },
    { "DMAOR", 0xffa00040, 4, SH4_REG_DMAOR, true,
      Sh4WarnRegReadHandler, Sh4WarnRegWriteHandler, 0, 0 },

    /* Serial port */
    { "SCSMR2", 0xffe80000, 2, SH4_REG_SCSMR2, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "SCBRR2", 0xffe80004, 1, SH4_REG_SCBRR2, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0xff, 0xff },
    { "SCSCR2", 0xffe80008, 2, SH4_REG_SCSCR2, false,
      sh4_scscr2_reg_read_handler, sh4_scscr2_reg_write_handler, 0, 0 },
    { "SCFTDR2", 0xffe8000c, 1, (sh4_reg_idx_t)-1, false,
      Sh4WriteOnlyRegReadHandler, sh4_scftdr2_reg_write_handler, 0xff, 0xff },
    { "SCFSR2", 0xffe80010, 2, SH4_REG_SCFSR2, false,
      sh4_scfsr2_reg_read_handler, sh4_scfsr2_reg_write_handler, 0x0060, 0x0060 },
    { "SCFRDR2", 0xffe80014, 1, (sh4_reg_idx_t)-1, false,
      sh4_scfrdr2_reg_read_handler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "SCFCR2", 0xffe80018, 2, SH4_REG_SCFCR2, false,
      sh4_scfcr2_reg_read_handler, sh4_scfcr2_reg_write_handler, 0, 0 },
    { "SCFDR2", 0xffe8001c, 2, (sh4_reg_idx_t)-1, false,
      sh4_scfdr2_reg_read_handler, Sh4ReadOnlyRegWriteHandler, 0, 0 },
    { "SCSPTR2", 0xffe80020, 2, SH4_REG_SCSPTR2, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },
    { "SCLSR2", 0xffe80024, 2, SH4_REG_SCLSR2, false,
      Sh4DefaultRegReadHandler, Sh4DefaultRegWriteHandler, 0, 0 },

    /* interrupt controller */
    { "ICR", 0xffd00000, 2, SH4_REG_ICR, true,
      Sh4DefaultRegReadHandler, sh4_excp_icr_reg_write_handler, 0, 0 },
    { "IPRA", 0xffd00004, 2, SH4_REG_IPRA, true,
      Sh4DefaultRegReadHandler, sh4_excp_ipra_reg_write_handler, 0, 0 },
    { "IPRB", 0xffd00008, 2, SH4_REG_IPRB, true,
      Sh4DefaultRegReadHandler, sh4_excp_iprb_reg_write_handler, 0, 0 },
    { "IPRC", 0xffd0000c, 2, SH4_REG_IPRC, true,
      Sh4DefaultRegReadHandler, sh4_excp_iprc_reg_write_handler, 0, 0 },
    { "IPRD", 0xffd0000d, 2, SH4_REG_IPRD, true,
      Sh4DefaultRegReadHandler, sh4_excp_iprd_reg_write_handler, 0xda74, 0xda74 },

    /*
     * strange "padding" that exists adjacent to the IPR registers.
     * IP.BIN wants to write 0 to these.  I'm not sure if this is related
     * to the IPR registers or not.  I'm also not sure if there should be any
     * similar padding between IPRA/IPRB.
     */
    { "IPR_MYSTERY_ffd00002", 0xffd00002, 2, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "IPR_MYSTERY_ffd00006", 0xffd00006, 2, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "IPR_MYSTERY_ffd0000a", 0xffd0000a, 2, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },
    { "IPR_MYSTERY_ffd0000e", 0xffd0000e, 2, (sh4_reg_idx_t)-1, true,
      Sh4WriteOnlyRegReadHandler, sh4_zero_only_reg_write_handler },

    /* User Break Controller - I don't need this, I got my own debugger */
    { "BARA", 0xff200000, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BAMRA", 0xff200004, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BBRA", 0xff200008, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BARB", 0xff20000c, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BAMRB", 0xff200010, 1, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BBRB", 0xff200014, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BDRB", 0xff200018, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BDMRB", 0xff20001c, 4, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },
    { "BRCR", 0xff200020, 2, (sh4_reg_idx_t)-1, true,
      Sh4IgnoreRegReadHandler, Sh4IgnoreRegWriteHandler, 0, 0 },

    { NULL }
};

void sh4_init_regs(Sh4 *sh4) {
    sh4_poweron_reset_regs(sh4);
}

/*
 * If a register's index (in the Sh4MemMappedReg struct) is not -1,
 * then this algorithm will write the Sh4MemMappedReg's poweron_reset_val to
 * that register's index in sh4->reg.
 *
 * If the register's index is -1, then instead the default value will be
 * written to the register's position in sh4->reg_area (which serves as a sort
 * of RAM for ignored registers.  There are many registers whose handlers do
 * not make use of the reg_area either because they store the value somewhere
 * else or because they don't require storage; these registers will have to find
 * some other way to make sure they're set to the default state (such as hardcoding).
 *
 * "But wait!  What about soft resets?", you may ask.  The answer is that I
 * haven't thought that through yet and this means a lot of the register code
 * probably needs to be fully refactored later.  In general, I'm thinking of
 * some sort of a tree-like structure where all registers are represented
 * regardless of which components they represent.  reset and soft-reset could
 * be served by special handlers.
 */
void sh4_poweron_reset_regs(Sh4 *sh4) {
    Sh4MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        if (curs->reg_idx != (sh4_reg_idx_t)-1)
            sh4->reg[curs->reg_idx] = curs->poweron_reset_val;
        else
            Sh4IgnoreRegWriteHandler(sh4, &curs->poweron_reset_val, curs);

        curs++;
    }

    /*
     * HACK
     *
     * *technically* the value of r15 is supposed to be undefined at startup (as
     * it is with the other general-purpose registers), but when wash boots
     * in direct-boot mode with the -u flag, some software will expect it to be
     * set.
     *
     * This value was obtained empirically by observing the value of
     * _arch_old_stack in KallistiOS; this value was 0x8c00f3fc.  KallistiOS
     * pushes pr onto the stack before moving r15 into _arch_old_stack, so the
     * actual initial value should be 0x8c00f400.
     *
     * The good news is that this still fits within the definition of
     * "undefined", so this won't effect bios boots and it *probably* won't
     * effect direct boots that don't use the -u flag.
     */
    *sh4_gen_reg(sh4, 15) = 0x8c00f400;
}

static struct Sh4MemMappedReg *find_reg_by_addr(addr32_t addr) {
    struct Sh4MemMappedReg *curs = mem_mapped_regs;

    while (curs->reg_name) {
        if (curs->addr == addr)
            return curs;
        curs++;
    }

    if ((addr & SH4_REG_SDMR2_MASK) == SH4_REG_SDMR2_ADDR)
        return &sh4_sdmr2_reg;
    if ((addr & SH4_REG_SDMR3_MASK) == SH4_REG_SDMR3_ADDR)
        return &sh4_sdmr3_reg;

    error_set_address(addr);
    error_set_feature("accessing one of the mem-mapped registers");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
    return NULL; // never happens
}

int sh4_read_mem_mapped_reg(Sh4 *sh4, void *buf,
                                       addr32_t addr, unsigned len) {
    struct Sh4MemMappedReg *mm_reg = find_reg_by_addr(addr);
    Sh4RegReadHandler handler = mm_reg->on_p4_read;

    if (len != mm_reg->len) {
        error_set_length(len);
        error_set_expected_length(mm_reg->len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    return handler(sh4, buf, mm_reg);
}

int sh4_write_mem_mapped_reg(Sh4 *sh4, void const *buf,
                                        addr32_t addr, unsigned len) {
    struct Sh4MemMappedReg *mm_reg = find_reg_by_addr(addr);
    Sh4RegWriteHandler handler = mm_reg->on_p4_write;

    if (len != mm_reg->len) {
        error_set_length(len);
        error_set_expected_length(mm_reg->len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_INVALID_PARAM);
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
    error_set_feature("sh4 CPU exception for trying to read from a write-only "
                      "CPU register");
    error_set_address(reg_info->addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
    return 1; // never happens
}

int Sh4ReadOnlyRegWriteHandler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info) {
    error_set_feature("sh4 CPU exception for trying to write to a write-only "
                      "CPU register");
    error_set_address(reg_info->addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
    return 1; // never happens
}

int Sh4WarnRegReadHandler(Sh4 *sh4, void *buf,
                          struct Sh4MemMappedReg const *reg_info) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = Sh4DefaultRegReadHandler(sh4, buf, reg_info);

    if (ret_code) {
        fprintf(stderr, "WARNING: read from register %s\n", reg_info->reg_name);
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            fprintf(stderr, "WARNING: read from register %s\n", reg_info->reg_name);
        }
    }
    fprintf(stderr, "(PC is %x)\n", (unsigned)sh4->reg[SH4_REG_PC]);

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
        fprintf(stderr, "WARNING: write 0x%02x to register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: write 0x%04x to register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: write 0x%08x to register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: write to register %s\n", reg_info->reg_name);
    }

    return Sh4DefaultRegWriteHandler(sh4, buf, reg_info);
}

static int sh4_pdtra_reg_read_handler(Sh4 *sh4, void *buf,
                                      struct Sh4MemMappedReg const *reg_info) {
    /*
     * HACK - prevent infinite loop during bios boot at pc=0x8c00b94e
     * I'm not 100% sure what I'm doing here, I *think* PDTRA has something to
     * do with the display adapter.
     *
     * Basically, the boot rom writes a sequence of values to PDTRA (with
     * pctra's i/o selects toggling occasionally) and it expects a certain
     * sequence of values when it reads back from pdtra.  I mask in the values
     * it writes as outputs into the value of pdtra which is read back (because
     * according to the sh4 spec, output bits can be read as inputs and they
     * will have the value which was last written to them) and send it either 0
     * or 3 on the input bits based on the address in the PR register.
     * Hopefully this is good enough.
     *
     * If the boot rom doesn't get a value it wants to see after 10 attempts,
     * then it branches to GBR (0x8c000000), where it will put the processor to
     * sleep with interrupts disabled (ie forever).  Presumably this is all it
     * can due to handle an error at such an early stage in the boot process.
     */

    /*
     * n_pup = "not pullup"
     * n_input = "not input"
     */
    uint16_t n_pup_mask = 0, n_input_mask = 0;
    uint32_t pctra = sh4->reg[SH4_REG_PCTRA];

    /* parse out the PCTRA register */
    unsigned bit_no;
    for (bit_no = 0; bit_no < 16; bit_no++) {
        uint32_t n_input = (1 << (bit_no * 2) & pctra) >> (bit_no * 2);
        uint32_t n_pup = (1 << (bit_no * 2 + 1) & pctra) >> (bit_no * 2 + 1);

        n_pup_mask |= n_pup << bit_no;
        n_input_mask |= n_input << bit_no;
    }

    /* show the bios what (i think) it wants to see... */
    uint16_t out_val;
    switch (sh4->reg[SH4_REG_PR]) {
    case 0x8c00b97a:
    case 0x8c00b996:
        out_val = 0;
        break;
    case 0x8c00b964:
    case 0x8c00b96e:
    case 0x8c00b980:
    case 0x8c00b98a:
    default:
        out_val = 3;
    }

    /*
     * Set cable type - for now I hardcode to composite video (because that's
     * the only one games are required to support).  In the future, there should
     * be a way to select different video output types.
     */
    out_val |= 0x0300;

    /*
     * I also need to add in a way to select the TV video type in bits 4:2.  For
     * now I leave those three bits at zero, which corresponds to NTSC.  For PAL
     * formats, some of those bits are supposed to be non-zero.
     */

    /*
     * Now combine this with the values previously written to PDTRA - remember
     * that bits set to output can be read back, and that they should have the
     * same values that were written to them.
     */
    out_val = (out_val & ~n_input_mask) |
        (sh4->reg[SH4_REG_PDTRA] & n_input_mask);

    memcpy(buf, &out_val, sizeof(out_val));

    /* I got my eye on you...*/
    fprintf(stderr, "WARNING: reading 0x%04x from register %s\n",
            (unsigned)out_val, reg_info->reg_name);

    return 0;
}

static int sh4_pdtra_reg_write_handler(Sh4 *sh4, void const *buf,
                                       struct Sh4MemMappedReg const *reg_info) {
    uint16_t val;
    memcpy(&val, buf, sizeof(val));
    uint16_t val_orig = val;

    /*
     * n_pup = "not pullup"
     * n_input = "not input"
     */
    uint16_t n_pup_mask = 0, n_input_mask = 0;
    uint32_t pctra = sh4->reg[SH4_REG_PCTRA];

    /* parse out the PCTRA register */
    unsigned bit_no;
    for (bit_no = 0; bit_no < 16; bit_no++) {
        uint32_t n_input = (1 << (bit_no * 2) & pctra) >> (bit_no * 2);
        uint32_t n_pup = (1 << (bit_no * 2 + 1) & pctra) >> (bit_no * 2 + 1);

        n_pup_mask |= n_pup << bit_no;
        n_input_mask |= n_input << bit_no;
    }

    /* I got my eye on you...*/
    fprintf(stderr, "WARNING: writing 0x%04x to register %s "
            "(attempted write was %x)\n",
            (unsigned)val, reg_info->reg_name, (unsigned)val_orig);

    sh4->reg[SH4_REG_PDTRA] = val;

    return 0;
}

static int sh4_id_reg_read_handler(Sh4 *sh4, void *buf,
                                   struct Sh4MemMappedReg const *reg_info) {
    // this value was obtained empircally on a real dreamcast
    uint32_t id_val = 0x040205c1;
    memcpy(buf, &id_val, sizeof(id_val));
    return 0;
}

static int sh4_mmucr_reg_write_handler(Sh4 *sh4, void const *buf,
                                       struct Sh4MemMappedReg const *reg_info) {
    uint32_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    sh4->reg[SH4_REG_MMUCR] = new_val;

    if (new_val & SH4_MMUCR_AT_MASK) {
        error_set_feature("SH4 MMU support");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    return 0;
}

static int
sh4_zero_only_reg_write_handler(Sh4 *sh4, void const *buf,
                                  struct Sh4MemMappedReg const *reg_info) {
    uint8_t *bufp;
    unsigned n_bytes = reg_info->len;

    while (n_bytes--)
        if (*bufp++) {
            error_set_feature("writing non-zero to a zero-only register");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

    return 0;
}
