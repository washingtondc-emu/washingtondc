/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef ARM7_H_
#define ARM7_H_

#include "dc_sched.h"
#include "MemoryMap.h"

// negative/less-than
#define ARM7_CPSR_N_SHIFT 31
#define ARM7_CPSR_N_MASK (1 << ARM7_CPSR_N_SHIFT)

// zero
#define ARM7_CPSR_Z_SHIFT 30
#define ARM7_CPSR_Z_MASK (1 << ARM7_CPSR_Z_SHIFT)

// carry/borrow/extend
#define ARM7_CPSR_C_SHIFT 29
#define ARM7_CPSR_C_MASK (1 << ARM7_CPSR_C_SHIFT)

// overflow
#define ARM7_CPSR_V_SHIFT 28
#define ARM7_CPSR_V_MASK (1 << ARM7_CPSR_V_SHIFT)

// IRQ disable
#define ARM7_CPSR_I_SHIFT 7
#define ARM7_CPSR_I_MASK (1 << ARM7_CPSR_I_SHIFT)

// FIQ disable
#define ARM7_CPSR_F_SHIFT 6
#define ARM7_CPSR_F_MASK (1 << ARM7_CPSR_F_SHIFT)

// CPU mode
#define ARM7_CPSR_M_SHIFT 0
#define ARM7_CPSR_M_MASK (0x1f << ARM7_CPSR_M_SHIFT)

enum arm7_mode {
    ARM7_MODE_USER = 0x10,
    ARM7_MODE_FIQ = 0x11,
    ARM7_MODE_IRQ = 0x12,
    ARM7_MODE_SVC = 0x13,
    ARM7_MODE_ABT = 0x17,
    ARM7_MODE_UND = 0x33
};

/*
 * ARM7DI-type CPU wired into the AICA sound system.
 *
 * Like the SH4, it supports both little-endian and big-endian byte orders.
 * AFAIK, this is always little-endian on the Dreamcast.  Documentation seems to
 * indicate the endianess is set by an external pin on the CPU, and that is
 * hopefully hardwired into LE mode.
 */

enum arm7_reg_idx {

    // general-purpose registers
    ARM7_REG_R0,
    ARM7_REG_R1,
    ARM7_REG_R2,
    ARM7_REG_R3,
    ARM7_REG_R4,
    ARM7_REG_R5,
    ARM7_REG_R6,
    ARM7_REG_R7,
    ARM7_REG_R8,
    ARM7_REG_R9,
    ARM7_REG_R10,
    ARM7_REG_R11,
    ARM7_REG_R12,
    ARM7_REG_R13,
    ARM7_REG_R14,

    /*
     * this is actually the program counter, but the ARM7DI manual refers to it
     * as R15.
     */
    ARM7_REG_R15,

    /*
     * banked-registers.  ARM7 defines six distinct execution modes:
     * User (normal), FIQ, Superviser (kernel), Abort, IRQ, and Undefined.
     * These each have their own banked versions of R13 and R14.  FIQ mode also
     * has banked versions of R8-R12 in addition to R13 and R14.
     */
    ARM7_REG_R8_FIQ,  // fiq
    ARM7_REG_R9_FIQ,  // fiq
    ARM7_REG_R10_FIQ, // fiq
    ARM7_REG_R11_FIQ, // fiq
    ARM7_REG_R12_FIQ, // fiq
    ARM7_REG_R13_FIQ, // fiq
    ARM7_REG_R14_FIQ, // fiq
    ARM7_REG_R13_SVC, // supervisor
    ARM7_REG_R14_SVC, // supervisor
    ARM7_REG_R13_ABT, // abort
    ARM7_REG_R14_ABT, // abort
    ARM7_REG_R13_IRQ, // irq
    ARM7_REG_R14_IRQ, // irq
    ARM7_REG_R13_UND, // undefined
    ARM7_REG_R14_UND, // undefined

    // Current Program Status Register
    ARM7_REG_CPSR,

    /*
     * Saved Program Status Registers
     * Whenever there's a context-switch, the current value of CPSR gets
     * written to whichever one of these corresponds to the new mode.
     * User-mode is the only context which doesn't have an SPSR.
     */
    ARM7_REG_SPSR_FIQ, // fiq
    ARM7_REG_SPSR_SVC, // supervisor
    ARM7_REG_SPSR_ABT, // abort
    ARM7_REG_SPSR_IRQ, // irq
    ARM7_REG_SPSR_UND, // undefined

    ARM7_REGISTER_COUNT
};

struct arm7 {
    struct dc_clock *clk;

    uint32_t regs[ARM7_REGISTER_COUNT];
    struct memory_map *map;
};

void arm7_init(struct arm7 *arm7, struct dc_clock *clk);
void arm7_cleanup(struct arm7 *arm7);

void arm7_set_mem_map(struct arm7 *arm7, struct memory_map *arm7_mem_map);

#endif
