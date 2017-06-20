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

#ifndef SH4_REG_H_
#define SH4_REG_H_

#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "sh4_reg_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Sh4;
typedef struct Sh4 Sh4;

typedef enum sh4_reg_idx {
    /* general-purpose registers 0-7 */
    SH4_REG_R0,
    SH4_REG_R1,
    SH4_REG_R2,
    SH4_REG_R3,
    SH4_REG_R4,
    SH4_REG_R5,
    SH4_REG_R6,
    SH4_REG_R7,

    /* general-purpose registers 8-15 */
    SH4_REG_R8,
    SH4_REG_R9,
    SH4_REG_R10,
    SH4_REG_R11,
    SH4_REG_R12,
    SH4_REG_R13,
    SH4_REG_R14,
    SH4_REG_R15,

    /* general-purpose registers 0-7, (banked) */
    SH4_REG_R0_BANK,
    SH4_REG_R1_BANK,
    SH4_REG_R2_BANK,
    SH4_REG_R3_BANK,
    SH4_REG_R4_BANK,
    SH4_REG_R5_BANK,
    SH4_REG_R6_BANK,
    SH4_REG_R7_BANK,

    /* Floating-point registers */
    SH4_REG_FR0,
    SH4_REG_DR0 = SH4_REG_FR0,
    SH4_REG_FV0 = SH4_REG_FR0,
    SH4_REG_FR1,
    SH4_REG_FR2,
    SH4_REG_DR2 = SH4_REG_FR2,
    SH4_REG_FR3,
    SH4_REG_FR4,
    SH4_REG_DR4 = SH4_REG_FR4,
    SH4_REG_FV4 = SH4_REG_FR4,
    SH4_REG_FR5,
    SH4_REG_FR6,
    SH4_REG_DR6 = SH4_REG_FR6,
    SH4_REG_FR7,
    SH4_REG_FR8,
    SH4_REG_DR8 = SH4_REG_FR8,
    SH4_REG_FV8 = SH4_REG_FR8,
    SH4_REG_FR9,
    SH4_REG_FR10,
    SH4_REG_DR10 = SH4_REG_FR10,
    SH4_REG_FR11,
    SH4_REG_FR12,
    SH4_REG_DR12 = SH4_REG_FR12,
    SH4_REG_FV12 = SH4_REG_FR12,
    SH4_REG_FR13,
    SH4_REG_FR14,
    SH4_REG_DR14 = SH4_REG_FR14,
    SH4_REG_FR15,

    /* floating-point registers (banked) */
    SH4_REG_XF0,
    SH4_REG_XD0 = SH4_REG_XF0,
    SH4_REG_XMTRX = SH4_REG_XF0,
    SH4_REG_XF1,
    SH4_REG_XF2,
    SH4_REG_XD2 = SH4_REG_XF2,
    SH4_REG_XF3,
    SH4_REG_XF4,
    SH4_REG_XD4 = SH4_REG_XF4,
    SH4_REG_XF5,
    SH4_REG_XF6,
    SH4_REG_XD6 = SH4_REG_XF6,
    SH4_REG_XF7,
    SH4_REG_XF8,
    SH4_REG_XD8 = SH4_REG_XF8,
    SH4_REG_XF9,
    SH4_REG_XF10,
    SH4_REG_XD10 = SH4_REG_XF10,
    SH4_REG_XF11,
    SH4_REG_XF12,
    SH4_REG_XD12 = SH4_REG_XF12,
    SH4_REG_XF13,
    SH4_REG_XF14,
    SH4_REG_XD14 = SH4_REG_XF14,
    SH4_REG_XF15,

    /* floating-point status/control register */
    SH4_REG_FPSCR,

    /* floating-point communication register */
    SH4_REG_FPUL,

    /* status register */
    SH4_REG_SR,

    /* saved-status register */
    SH4_REG_SSR,

    /* saved program counter */
    SH4_REG_SPC,

    /* global base register */
    SH4_REG_GBR,

    /* vector base register */
    SH4_REG_VBR,

    /* saved general register 15 */
    SH4_REG_SGR,

    /* debug base register */
    SH4_REG_DBR,

    /* Multiply-and-accumulate register high */
    SH4_REG_MACH,

    /* multiply-and-accumulate register low */
    SH4_REG_MACL,

    /* procedure register */
    SH4_REG_PR,

    /* program counter */
    SH4_REG_PC,

    /* Page table entry high */
    SH4_REG_PTEH,

    /* Page table entry low */
    SH4_REG_PTEL,

    /* Page table entry assisstance */
    SH4_REG_PTEA,

    /* Translation table base */
    SH4_REG_TTB,

    /* TLB exception address */
    SH4_REG_TEA,

    /* MMU control */
    SH4_REG_MMUCR,

    /* Cache control register */
    SH4_REG_CCR,

    /* Queue address control register 0 */
    SH4_REG_QACR0,

    /* Queue address control register 1 */
    SH4_REG_QACR1,

    /* TRAPA immediate data     - 0xff000020 */
    SH4_REG_TRA,

    /* exception event register - 0xff000024 */
    SH4_REG_EXPEVT,

    /* interrupt event register - 0xff000028 */
    SH4_REG_INTEVT,

    /* Timer output control register */
    SH4_REG_TOCR,

    /* Timer start register */
    SH4_REG_TSTR,

    /* Timer channel 0 constant register */
    SH4_REG_TCOR0,

    /* Timer channel 0 counter */
    SH4_REG_TCNT0,

    /* Timer channel 0 control register */
    SH4_REG_TCR0,

    /* Timer channel 1 constant register */
    SH4_REG_TCOR1,

    /* Timer channel 1 counter */
    SH4_REG_TCNT1,

    /* Timer channel 1 control register */
    SH4_REG_TCR1,

    /* Timer channel 2 constant register */
    SH4_REG_TCOR2,

    /* Timer channel 2 counter */
    SH4_REG_TCNT2,

    /* Timer channel 2 control register */
    SH4_REG_TCR2,

    /* Timer channel 2 input capture register */
    SH4_REG_TCPR2,

    /* DMAC Source Address Register 1 */
    SH4_REG_SAR1,

    /* DMAC Destination Address Register 1 */
    SH4_REG_DAR1,

    /* DMAC transfer count register 1 */
    SH4_REG_DMATCR1,

    /* DMAC channel control register 1 */
    SH4_REG_CHCR1,

    /* DMAC Source Address Register 2 */
    SH4_REG_SAR2,

    /* DMAC Destination Address Register 2 */
    SH4_REG_DAR2,

    /* DMAC transfer count register 2 */
    SH4_REG_DMATCR2,

    /* DMAC channel control register 2 */
    SH4_REG_CHCR2,

    /* DMAC Source Address Register 3 */
    SH4_REG_SAR3,

    /* DMAC Destination Address Register 3 */
    SH4_REG_DAR3,

    /* DMAC transfer count register 3 */
    SH4_REG_DMATCR3,

    /* DMAC channel control register 3 */
    SH4_REG_CHCR3,

    /* DMAC Operation Register */
    SH4_REG_DMAOR,

    /* Interrupt Control Register */
    SH4_REG_ICR,

    /* Interrupt Priority Registers A-D */
    SH4_REG_IPRA,
    SH4_REG_IPRB,
    SH4_REG_IPRC,
    SH4_REG_IPRD,

    SH4_REG_PCTRA,
    SH4_REG_PDTRA,
    SH4_REG_PCTRB,
    SH4_REG_PDTRB,

    /* SCIF Serial Mode Register */
    SH4_REG_SCSMR2,

    /* SCIF Bitrate Register */
    SH4_REG_SCBRR2,

    /* SCIF Serial Control Register */
    SH4_REG_SCSCR2,

    /* SCIF Transmit FIFO Data Register */
    // SH4_REG_SCFTDR2,

    /* SCIF Serial Status Register */
    SH4_REG_SCFSR2,

    /* SCIF Receive FIFO Data Register */
    // SH4_REG_SCFRDR2,

    /* SCIF FIFO Control Register */
    SH4_REG_SCFCR2,

    /* SCIF Serial Port Register */
    SH4_REG_SCSPTR2,

    /* SCIF Line Status Register */
    SH4_REG_SCLSR2,

    /* Standby Control Register */
    SH4_REG_STBCR,

    SH4_REGISTER_COUNT
} sh4_reg_idx_t;

static_assert(SH4_REG_FR15 - SH4_REG_FR0 + 1 == 16,
              "incorrect number of FPU registers");
static_assert(SH4_REG_XF15 - SH4_REG_XF0 + 1 == 16,
              "incorrect number of banked FPU registers");

/*
 * for the purpose of these handlers, you may assume that the caller has
 * already checked the permissions.
 */
struct Sh4MemMappedReg;
typedef int(*Sh4RegReadHandler)(Sh4 *sh4, void *buf,
                                struct Sh4MemMappedReg const *reg_info);
typedef int(*Sh4RegWriteHandler)(Sh4 *sh4, void const *buf,
                                 struct Sh4MemMappedReg const *reg_info);

/*
 * TODO: turn this into a radix tree of some sort.
 *
 * Alternatively, I could turn this into a simple lookup array; this
 * would incur a huge memory overhead (hundreds of MB), but it looks like
 * it would be feasible in the $CURRENT_YEAR and it would net a
 * beautiful O(1) mapping from addr32_t to MemMappedReg.
 */
struct Sh4MemMappedReg {
    char const *reg_name;

    /*
     * Some registers can be referenced over a range of addresses.
     * To check for equality between this register and a given physical
     * address, AND the address with addr_mask and then check for equality
     * with addr
     */
    addr32_t addr;  // addr shoud be the p4 addr, not the area7 addr
    addr32_t addr_mask;

    unsigned len;

    /* index of the register in the register file */
    sh4_reg_idx_t reg_idx;

    /*
     * if true, the value will be preserved during a manual ("soft") reset
     * and manual_reset_val will be ignored; else value will be set to
     * manual_reset_val during a manual reset.
     */
    bool hold_on_reset;

    Sh4RegReadHandler on_p4_read;
    Sh4RegWriteHandler on_p4_write;

    /*
     * if len < 4, then only the lower "len" bytes of
     * these values will be used.
     */
    reg32_t poweron_reset_val;
    reg32_t manual_reset_val;
};
typedef struct Sh4MemMappedReg Sh4MemMappedReg;

/*
 * this is called from the sh4 constructor to
 * initialize all memory-mapped registers
 */
void sh4_init_regs(Sh4 *sh4);

// set up the memory-mapped registers for a reset;
void sh4_poweron_reset_regs(Sh4 *sh4);

/* read/write handler callbacks for when you don't give a fuck */
int Sh4IgnoreRegReadHandler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info);
int Sh4IgnoreRegWriteHandler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info);

/* default reg reg/write handler callbacks */
int Sh4DefaultRegReadHandler(Sh4 *sh4, void *buf,
                             struct Sh4MemMappedReg const *reg_info);
int Sh4DefaultRegWriteHandler(Sh4 *sh4, void const *buf,
                              struct Sh4MemMappedReg const *reg_info);

/*
 * read handle callback that always fails (although currently it throws an
 * UnimplementedError because I don't know what the proper response is when
 * the software tries to read from an unreadable register).
 *
 * This is used for certain registers which are write-only.
 */
int Sh4WriteOnlyRegReadHandler(Sh4 *sh4, void *buf,
                               struct Sh4MemMappedReg const *reg_info);

/*
 * likewise, this is a write handler for read-only registers.
 * It will also raise an exception whenever it is invokled.
 */
int Sh4ReadOnlyRegWriteHandler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info);

/*
 * These handlers are functionally equivalent to the default read/write
 * handlers, except they log a warning to std::cerr every time they are called.
 */
int Sh4WarnRegReadHandler(Sh4 *sh4, void *buf,
                          struct Sh4MemMappedReg const *reg_info);
int Sh4WarnRegWriteHandler(Sh4 *sh4, void const *buf,
                           struct Sh4MemMappedReg const *reg_info);

/*
 * called for P4 area read/write ops that
 * fall in the memory-mapped register range
 */
int sh4_read_mem_mapped_reg(Sh4 *sh4, void *buf,
                            addr32_t addr, unsigned len);
int sh4_write_mem_mapped_reg(Sh4 *sh4, void const *buf,
                             addr32_t addr, unsigned len);

#ifdef __cplusplus
}
#endif

#endif
