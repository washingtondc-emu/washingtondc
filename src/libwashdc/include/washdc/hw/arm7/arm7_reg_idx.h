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

#ifndef WASHDC_ARM7_REG_IDX_H_
#define WASHDC_ARM7_REG_IDX_H_

#ifdef __cplusplus
extern "C" {
#endif

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
    ARM7_REG_PC = ARM7_REG_R15,

    ARM7_REG_R8_BANK,
    ARM7_REG_R9_BANK,
    ARM7_REG_R10_BANK,
    ARM7_REG_R11_BANK,
    ARM7_REG_R12_BANK,
    ARM7_REG_R13_BANK,
    ARM7_REG_R14_BANK,

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

#ifdef __cplusplus
}
#endif

#endif
