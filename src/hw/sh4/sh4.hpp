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

#ifndef SH4_HPP_
#define SH4_HPP_

#include <boost/cstdint.h>

/* Hitachi SuperH-4 interpreter */

class Sh4 {
public:
private:

    // true/false condition or carry/borrow bit
    static const unsigned SR_FLAG_T_SHIFT = 0;
    static const unsigned SR_FLAG_T = 1 << SR_FLAG_T_SHIFT;

    // saturation operation for MAC instructions
    static const unsigned SR_FLAG_S_SHIFT = 1;
    static const unsigned SR_FLAG_S = 1 << SR_FLAG_S;

    // interrupt mask level
    static const unsigned SR_IMASK_SHIFT = 4;
    static const unsigned SR_IMASK = 0xf << SR_IMASK_SHIFT;

    static const unsigned SR_Q_SHIFT = 8;
    static const unsigned SR_Q = 1 << SR_Q_SHIFT;

    static const unsigned SR_M_SHIFT = 9;
    static const unsigned SR_M = 1 << SR_M_SHIFT;

    // FPU disable bit
    static const unsigned SR_FD_SHIFT = 15;
    static const unsigned SR_FD = 1 << SR_FD_SHIFT;

    // IRQ mask (1 == masked)
    static const unsigned SR_BL_SHIFT = 28;
    static const unsigned SR_BL = 1 << SR_BL_SHIFT;

    // general register bank switch
    static const unsigned SR_RB_SHIFT = 29;
    static const unsigned SR_RB = 1 << SR_RB_SHIFT;

    // processor mode
    static const unsigned SR_MD_SHIFT = 30;
    static const unsigned SR_MD = 1 << SR_MD_SHIFT;

    // floating-point rounding mode
    static const unsigned FR_RM_SHIFT = 0;
    static const unsigned FR_RM = 3 << FR_RM_SHIFT;

    // FPU exception flags
    static const unsigned FR_FLAG_SHIFT = 2;
    static const unsigned FR_FLAG = 0x1f << FR_FLAG_SHIFT;

    // FPU exception enable
    static const unsigned FR_ENABLE_SHIFT = 7;
    static const unsigned FR_ENABLE = 0x1f << FR_FLAG_SHIFT;

    // FPU exception cause
    static const unsigned FR_CAUSE_SHIFT = 12;
    static const unsigned FR_CAUSE = 0x1f << FR_CAUSE_SHIFT;

    // FPU Denormalization mode
    static const unsigned FR_DN_SHIFT = 18;
    static const unsigned FR_DN = 1 << FR_DN_SHIFT;

    // FPU Precision mode
    static const unsigned FR_PR_SHIFT = 19;
    static const unsigned FR_PR = 1 << FR_PR_SHIFT;

    // FPU Transfer size mode
    static const unsigned FR_SZ_SHIFT = 20;
    static const unsigned FR_SZ = 1 << FR_SZ_SHIFT;

    // FPU bank switch
    static const unsigned FR_FR_SHIFT = 21;
    static const unsigned FR_FR = 1 << FR_FR_SHIFT;

    typedef boost::uint32_t reg32_t;

    struct RegFile {
        // general-purpose registers R0_BANK0-R7_BANK0
        reg32_t r_bank0[8];

        // general-purpose registers R0_BANK1-R7_BANK1
        reg32_t r_bank1[8];

        // general-purpose registers R8-R15
        reg32_t rgen[8];

        // floating point bank0 registers FPR0_BANK0-FPR15_BANK0
        reg32_t fpr_bank0;

        // floating point bank1 registers FPR0_BANK1-FPR15_BANK1
        reg32_t fpr_bank1;

        // status register
        reg32_t sr;

        // saved-status register
        reg32_t ssr;

        // saved program counter
        reg32_t spc;

        // global base register
        reg32_t gbr;

        // vector base register
        reg32_t vbr;

        // saved general register 15
        reg32_t sgr;

        // debug base register
        reg32_t dbr;

        // Multiply-and-accumulate register high
        reg32_t mach;

        // multiply-and-accumulate register low
        reg32_t macl;

        // procedure register
        reg32_t pr;

        // program counter
        reg32_t pc;

        // floating point status/control register
        reg32_t fpscr;

        // floating-point communication register
        reg32_t fpul;
    } reg;

    struct Mmu {
        // Page table entry high
        reg32_t pteh;

        // Page table entry low
        reg32_t ptel;

        // Page table entry assisstance
        reg32_t ptea;

        // Translation table base
        reg32_t ttb;

        // TLB exception address
        reg32_t tea;

        // MMU control
        reg32_t mmucr;
    } mmu;
};

#endif
