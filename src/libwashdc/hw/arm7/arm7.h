/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <stdbool.h>
#include <assert.h>

#include "washdc/error.h"
#include "dc_sched.h"
#include "washdc/MemoryMap.h"
#include "hw/aica/aica_wave_mem.h"

#define ARM7_CLOCK_SCALE (SCHED_FREQUENCY / (45 * 1000 * 1000))
static_assert(SCHED_FREQUENCY % (45 * 1000 * 1000) == 0,
              "scheduler frequency does not cleanly divide by SH4 frequency");

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
    ARM7_MODE_USER = (0x10 << ARM7_CPSR_M_SHIFT),
    ARM7_MODE_FIQ  = (0x11 << ARM7_CPSR_M_SHIFT),
    ARM7_MODE_IRQ  = (0x12 << ARM7_CPSR_M_SHIFT),
    ARM7_MODE_SVC  = (0x13 << ARM7_CPSR_M_SHIFT),
    ARM7_MODE_ABT  = (0x17 << ARM7_CPSR_M_SHIFT),
    ARM7_MODE_UND  = (0x1b << ARM7_CPSR_M_SHIFT)
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
    ARM7_REG_PC = ARM7_REG_R15,

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

enum arm7_excp {
    ARM7_EXCP_NONE = 0,
    ARM7_EXCP_RESET = 1,
    ARM7_EXCP_DATA_ABORT = 2,
    ARM7_EXCP_FIQ = 4,
    ARM7_EXCP_IRQ = 8,
    ARM7_EXCP_PREF_ABORT = 16,
    ARM7_EXCP_SWI = 32
};

typedef uint32_t arm7_inst;

typedef bool(*arm7_irq_fn)(void *dat);

struct arm7 {
    /*
     * For the sake of instruction-fetching, ARM7 disregards the memory_map and
     * goes straight here.  This is less modular than going to the memory_map
     * since it hardcodes for AICA's memory map but needs must.
     */
    struct aica_wave_mem *inst_mem;
    struct dc_clock *clk;
    struct memory_map *map;

    uint32_t reg[ARM7_REGISTER_COUNT];

    /*
     * One oddity about ARM7 (compared to saner CPUs like x86 and SH4) is that
     * the CPU does not hide its pipelining from software.  The Program Counter
     * register (ARM7_REG_R15) always points to the instruction being fetched;
     * since there's a 3-stage pipeline which is *not* hidden from software,
     * that means that ARM7_REG_R15 always points two instructions ahead of the
     * instruction being executed.
     *
     * For the sake of simplicity, this interpreter will actually mimic this
     * design by buffering three instructions in a fake "pipeline".  pipeline[2]
     * buffers the execution stage (ARM7_REG_R15 - 8), pipeline[1] buffers the
     * decoding stage (ARM7_REG_R15 - 4), and pipeline[0] buffers the fetch
     * stage (ARM7_REG_R15).  Instructions are actually fetched two cycles ahead
     * of their execution like in a real ARM, but the decoding isn't done until
     * it's at the execution stage.
     */
    arm7_inst pipeline[2];
    uint32_t pipeline_pc[2];

    enum arm7_excp excp;

    bool enabled;

    bool fiq_line;

    bool excp_dirty;

    bool pipeline_full;
};

typedef bool(*arm7_cond_fn)(struct arm7*);
typedef void(*arm7_op_fn)(struct arm7*,arm7_inst);

struct arm7_decoded_inst {
    arm7_cond_fn cond;
    arm7_op_fn op;
    arm7_inst inst;

    unsigned cycles;
};

void arm7_init(struct arm7 *arm7, struct dc_clock *clk, struct aica_wave_mem *inst_mem);
void arm7_cleanup(struct arm7 *arm7);

void arm7_fetch_inst(struct arm7 *arm7, struct arm7_decoded_inst *inst_out);

void arm7_decode(struct arm7 *arm7, struct arm7_decoded_inst *inst_out,
                 arm7_inst inst);

unsigned arm7_exec(struct arm7 *arm7, struct arm7_decoded_inst const *inst);

void arm7_set_mem_map(struct arm7 *arm7, struct memory_map *arm7_mem_map);

void arm7_reset(struct arm7 *arm7, bool val);

void arm7_get_regs(struct arm7 *arm7, void *dat_out);

uint32_t arm7_pc_next(struct arm7 *arm7);

void arm7_set_fiq(struct arm7 *arm7);
void arm7_clear_fiq(struct arm7 *arm7);

inline static uint32_t *arm7_gen_reg(struct arm7 *arm7, unsigned reg) {
    unsigned idx_actual;
    switch (arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_M_MASK) {
    case ARM7_MODE_USER:
        idx_actual = reg + ARM7_REG_R0;
        break;
    case ARM7_MODE_FIQ:
        if (reg >= 8 && reg <= 14)
            idx_actual = (reg - 8) + ARM7_REG_R8_FIQ;
        else
            idx_actual = reg + ARM7_REG_R0;
        break;
    case ARM7_MODE_IRQ:
        if (reg >= 13 && reg <= 14)
            idx_actual = (reg - 13) + ARM7_REG_R13_IRQ;
        else
            idx_actual = reg + ARM7_REG_R0;
        break;
    case ARM7_MODE_SVC:
        if (reg >= 13 && reg <= 14)
            idx_actual = (reg - 13) + ARM7_REG_R13_SVC;
        else
            idx_actual = reg + ARM7_REG_R0;
        break;
    case ARM7_MODE_ABT:
        if (reg >= 13 && reg <= 14)
            idx_actual = (reg - 13) + ARM7_REG_R13_ABT;
        else
            idx_actual = reg + ARM7_REG_R0;
        break;
    case ARM7_MODE_UND:
        if (reg >= 13 && reg <= 14)
            idx_actual = (reg - 13) + ARM7_REG_R13_UND;
        else
            idx_actual = reg + ARM7_REG_R0;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    return arm7->reg + idx_actual;
}

#endif
