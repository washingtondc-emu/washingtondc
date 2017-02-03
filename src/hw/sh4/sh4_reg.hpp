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

#ifndef SH4_REG_HPP_
#define SH4_REG_HPP_

typedef enum sh4_reg_idx {
    /* general-purpose registers 0-7, bank 0 */
    SH4_REG_R0_BANK0,
    SH4_REG_R1_BANK0,
    SH4_REG_R2_BANK0,
    SH4_REG_R3_BANK0,
    SH4_REG_R4_BANK0,
    SH4_REG_R5_BANK0,
    SH4_REG_R6_BANK0,
    SH4_REG_R7_BANK0,

    /* general-purpose registers 0-7, bank 1 */
    SH4_REG_R0_BANK1,
    SH4_REG_R1_BANK1,
    SH4_REG_R2_BANK1,
    SH4_REG_R3_BANK1,
    SH4_REG_R4_BANK1,
    SH4_REG_R5_BANK1,
    SH4_REG_R6_BANK1,
    SH4_REG_R7_BANK1,

    /* general-purpose registers 8-15 */
    SH4_REG_R8,
    SH4_REG_R9,
    SH4_REG_R10,
    SH4_REG_R11,
    SH4_REG_R12,
    SH4_REG_R13,
    SH4_REG_R14,
    SH4_REG_R15,

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

    SH4_REGISTER_COUNT
} sh4_reg_idx_t;

#endif
