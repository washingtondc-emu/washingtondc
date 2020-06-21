/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef WASHDC_SH4_REG_IDX_H_
#define WASHDC_SH4_REG_IDX_H_

#ifdef __cplusplus
extern "C" {
#endif

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

    /*
     * undocumented SH4 register certain games (Virtua Fighter 3tb, Shenume,
     * etc) want to access.
     */
    SH4_REG_UNKNOWN_ff100008,

    SH4_REGISTER_COUNT
} sh4_reg_idx_t;

#ifdef __cplusplus
}
#endif

#endif
