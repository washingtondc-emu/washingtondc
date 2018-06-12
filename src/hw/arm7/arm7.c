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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"

#include "arm7.h"

static DEF_ERROR_U32_ATTR(arm7_inst)
static DEF_ERROR_U32_ATTR(arm7_pc)

#define ARM7_INST_COND_SHIFT 28
#define ARM7_INST_COND_MASK (0xf << ARM7_INST_COND_SHIFT)

/*
 * Used to weigh different types of cycles.
 *
 * TODO: I think the different cycle types refer to different clocks (CPU clock,
 * memory clock, etc).  I'm not sure how fast these are relative to each other,
 * so for now I weigh them all equally.
 *
 * see chapter 5.0 (Memory Interface) of the data sheet.
 */
#define S_CYCLE 1 // access address at or one word after previous address.
#define N_CYCLE 1 // access address with no relation to previous address.
#define I_CYCLE 1

static void arm7_check_excp(struct arm7 *arm7);

static uint32_t do_fetch_inst(struct arm7 *arm7, uint32_t addr);
static void reset_pipeline(struct arm7 *arm7);

static void arm7_op_branch(struct arm7 *arm7, arm7_inst inst);
static void arm7_op_ldr(struct arm7 *arm7, arm7_inst inst);
static void arm7_op_mrs(struct arm7 *arm7, arm7_inst inst);
static void arm7_op_orr_immed(struct arm7 *arm7, arm7_inst inst);

static bool arm7_cond_eq(struct arm7 *arm7);
static bool arm7_cond_ne(struct arm7 *arm7);
static bool arm7_cond_cs(struct arm7 *arm7);
static bool arm7_cond_cc(struct arm7 *arm7);

static arm7_cond_fn arm7_cond(arm7_inst inst);

static unsigned arm7_reg_idx(struct arm7 *arm7, unsigned reg);
static unsigned arm7_spsr_idx(struct arm7 *arm7);

static bool arm7_cond_eq(struct arm7 *arm7) {
    return (bool)(arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_Z_MASK);
}

static bool arm7_cond_ne(struct arm7 *arm7) {
    return !arm7_cond_eq(arm7);
}

static bool arm7_cond_cs(struct arm7 *arm7) {
    return (bool)(arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_C_MASK);
}

static bool arm7_cond_cc(struct arm7 *arm7) {
    return !arm7_cond_cs(arm7);
}

static bool arm7_cond_mi(struct arm7 *arm7) {
    return (bool)(arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_N_MASK);
}

static bool arm7_cond_pl(struct arm7 *arm7) {
    return !arm7_cond_mi(arm7);
}

static bool arm7_cond_vs(struct arm7 *arm7) {
    return (bool)(arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_V_MASK);
}

static bool arm7_cond_vc(struct arm7 *arm7) {
    return !arm7_cond_vs(arm7);
}

static bool arm7_cond_hi(struct arm7 *arm7) {
    return arm7_cond_ne(arm7) && arm7_cond_cs(arm7);
}

static bool arm7_cond_ls(struct arm7 *arm7) {
    return arm7_cond_cc(arm7) || arm7_cond_eq(arm7);
}

static bool arm7_cond_ge(struct arm7 *arm7) {
    return arm7_cond_mi(arm7) == arm7_cond_vs(arm7);
}

static bool arm7_cond_lt(struct arm7 *arm7) {
    return !arm7_cond_ge(arm7);
}

static bool arm7_cond_gt(struct arm7 *arm7) {
    return arm7_cond_ne(arm7) && arm7_cond_ge(arm7);
}

static bool arm7_cond_le(struct arm7 *arm7) {
    return !arm7_cond_gt(arm7);
}

static bool arm7_cond_al(struct arm7 *arm7) {
    return true;
}

static arm7_cond_fn arm7_cond(arm7_inst inst) {
    switch ((inst & ARM7_INST_COND_MASK) >> ARM7_INST_COND_SHIFT) {
    case 0:
        return arm7_cond_eq;
    case 1:
        return arm7_cond_ne;
    case 2:
        return arm7_cond_cs;
    case 3:
        return arm7_cond_cc;
    case 4:
        return arm7_cond_mi;
    case 5:
        return arm7_cond_pl;
    case 6:
        return arm7_cond_vs;
    case 7:
        return arm7_cond_vc;
    case 8:
        return arm7_cond_hi;
    case 9:
        return arm7_cond_ls;
    case 10:
        return arm7_cond_ge;
    case 11:
        return arm7_cond_lt;
    case 12:
        return arm7_cond_gt;
    case 13:
        return arm7_cond_le;
    case 14:
        return arm7_cond_al;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void arm7_init(struct arm7 *arm7, struct dc_clock *clk) {
    memset(arm7, 0, sizeof(*arm7));
    arm7->clk = clk;
}

void arm7_cleanup(struct arm7 *arm7) {
}

void arm7_set_mem_map(struct arm7 *arm7, struct memory_map *arm7_mem_map) {
    arm7->map = arm7_mem_map;
    reset_pipeline(arm7);
}

void arm7_reset(struct arm7 *arm7, bool val) {
    // TODO: set the ARM7 to supervisor (svc) mode and enter a reset exception.
    printf("%s(%s)\n", __func__, val ? "true" : "false");

    if (!arm7->enabled && val) {
        // enable the CPU
        arm7->excp |= ARM7_EXCP_RESET;
    }

    arm7->enabled = val;
}

// Set all bits up to but not including bit_no:
#define SET_TO_BIT(bit_no)   ((uint32_t)((((uint64_t)1) << (bit_no)) - 1))

// set all bits between first and last (inclusive)
#define BIT_RANGE(first, last) (SET_TO_BIT(last + 1) & ~SET_TO_BIT(first))

// B or BL instruction
#define MASK_B BIT_RANGE(25, 27)
#define VAL_B  0x0a000000

#define MASK_LDR (BIT_RANGE(26, 27) | (1 << 20))
#define VAL_LDR  0x04100000

#define MASK_STR (BIT_RANGE(26, 27) | (1 << 20))
#define VAL_STR  0x04000000

#define MASK_MRS (BIT_RANGE(23, 27) | BIT_RANGE(16, 21) | BIT_RANGE(0, 11))
#define VAL_MRS  0x010f0000

// data processing opcodes
#define MASK_ORR_IMMED BIT_RANGE(21, 27)
#define VAL_ORR_IMMED ((1 << 25) | (12 << 21))

void arm7_decode(struct arm7 *arm7, struct arm7_decoded_inst *inst_out,
                 arm7_inst inst) {
    inst_out->cond = arm7_cond(inst);
    inst_out->inst = inst;

    if ((inst & MASK_B) == VAL_B) {
        // branch (with or without link)
        inst_out->op = arm7_op_branch;
        inst_out->cycles = 2 * S_CYCLE + 1 * N_CYCLE;
    } else if ((inst & MASK_LDR) == VAL_LDR) {
        /*
         * TODO: this is supposed to take 2 * S_CYCLE + 2 * N_CYCLE + I_CYCLE
         * cycles if R15 is involved...?
         */
        inst_out->op = arm7_op_ldr;
        inst_out->cycles = 1 * S_CYCLE + 1 * N_CYCLE + 1 * I_CYCLE;
    } else if ((inst & MASK_MRS) == VAL_MRS) {
        inst_out->op = arm7_op_mrs;
        inst_out->cycles = 1 * S_CYCLE;
    } else if ((inst & MASK_ORR_IMMED) == VAL_ORR_IMMED) {
        /*
         * TODO: this cycle count is literally just something I made up with no
         * basis in reality.  It needs to be corrected.
         */
        inst_out->op = arm7_op_orr_immed;
        inst_out->cycles = 2 * S_CYCLE + 1 * N_CYCLE;
    } else {
        error_set_arm7_inst(inst);
        error_set_arm7_pc(arm7->reg[ARM7_REG_R15]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void next_inst(struct arm7 *arm7) {
    arm7->reg[ARM7_REG_R15] += 4;
}

static void arm7_idle_fetch(struct arm7 *arm7, arm7_inst inst) {
    next_inst(arm7);
}

void arm7_fetch_inst(struct arm7 *arm7, struct arm7_decoded_inst *inst_out) {
    arm7_inst ret;

    arm7_check_excp(arm7);

    switch (arm7->pipeline_len) {
    case 2:
        ret = arm7->pipeline[1];
        arm7->pipeline[1] = arm7->pipeline[0];
        arm7->pipeline[0] = do_fetch_inst(arm7, arm7->reg[ARM7_REG_R15]);
        arm7_decode(arm7, inst_out, ret);
        return;
    case 1:
        arm7->pipeline[1] = arm7->pipeline[0];
    case 0:
        arm7->pipeline[0] = do_fetch_inst(arm7, arm7->reg[ARM7_REG_R15]);
        arm7->pipeline_len++;
        inst_out->op = arm7_idle_fetch;
        inst_out->cycles = 1;
        inst_out->cond = arm7_cond_al;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static void arm7_check_excp(struct arm7 *arm7) {
    enum arm7_excp excp = arm7->excp;
    uint32_t cpsr = arm7->reg[ARM7_REG_CPSR];

    if (excp & ARM7_EXCP_RESET) {
        arm7->reg[ARM7_REG_SPSR_SVC] = cpsr;
        arm7->reg[ARM7_REG_R14_SVC] = arm7->reg[ARM7_REG_R15] - 4;
        arm7->reg[ARM7_REG_R15] = 0;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_SVC | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_RESET;
    } else if (excp & ARM7_EXCP_DATA_ABORT) {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else if ((excp & ARM7_EXCP_FIQ) && !(cpsr & ARM7_CPSR_I_MASK)) {
        arm7->reg[ARM7_REG_SPSR_FIQ] = cpsr;
        arm7->reg[ARM7_REG_R14_FIQ] = arm7->reg[ARM7_REG_R15] - 4;
        arm7->reg[ARM7_REG_R15] = 0x1c;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_FIQ | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_FIQ;
    } else if ((excp & ARM7_EXCP_IRQ) && !(cpsr & ARM7_CPSR_F_MASK)) {
        arm7->reg[ARM7_REG_SPSR_IRQ] = cpsr;
        arm7->reg[ARM7_REG_R14_IRQ] = arm7->reg[ARM7_REG_R15] - 4;
        arm7->reg[ARM7_REG_R15] = 0x18;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_IRQ | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_IRQ;
    } else if (excp & ARM7_EXCP_PREF_ABORT) {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else if (excp & ARM7_EXCP_SWI) {
        /*
         * This will be called *after* the SWI instruction has executed, when
         * the arm7 is about to execute the next instruction.  The spec says
         * that R14_svc needs to point to the instruction immediately after the
         * SWI.  I expect the SWI instruction to not increment the PC at the
         * end, so the instruction after the SWI will be pipeline[1].
         * ARM7_REG_R15 points to the next instruction to be fetched, which is
         * pipeline[0].  Therefore, the next instruction to be executed is at
         * ARM7_REG_R15 - 4.
         */
        arm7->reg[ARM7_REG_SPSR_SVC] = cpsr;
        arm7->reg[ARM7_REG_R14_SVC] = arm7->reg[ARM7_REG_R15] - 4;
        arm7->reg[ARM7_REG_R15] = 0;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_SVC | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_SWI;
    }
}

static uint32_t do_fetch_inst(struct arm7 *arm7, uint32_t addr) {
    return memory_map_read_32(arm7->map, addr);
}

/*
 * call this when something like a branch or exception happens that invalidates
 * instructions in the pipeline.
 */
static void reset_pipeline(struct arm7 *arm7) {
    arm7->pipeline_len = 0;
}

static void arm7_op_branch(struct arm7 *arm7, arm7_inst inst) {
    uint32_t offs = inst & ((1 << 24) - 1);
    if (offs & (1 << 23))
        offs |= 0xff000000;
    offs <<= 2;

    if (inst & (1 << 24)) {
        // link bit
        arm7->reg[arm7_reg_idx(arm7, ARM7_REG_R14)] = arm7->reg[ARM7_REG_R15] - 4;
    }

    uint32_t pc_new = offs + arm7->reg[ARM7_REG_R15];

    arm7->reg[ARM7_REG_R15] = pc_new;
    reset_pipeline(arm7);
}

static void arm7_op_ldr(struct arm7 *arm7, arm7_inst inst) {
    unsigned rn = (inst >> 16) & 0xf;
    unsigned rd = (inst >> 12) & 0xf;

    bool writeback = inst & (1 << 21);
    int len = (inst & (1 << 22)) ? 1 : 4;
    int sign = (inst & (1 << 23)) ? 1 : -1;
    bool pre = inst & (1 << 24);
    bool offs_reg = inst & (1 << 25);

    uint32_t offs;
    if (offs_reg) {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else {
        offs = inst & ((1 << 12) - 1);
    }

    uint32_t addr = arm7->reg[arm7_reg_idx(arm7, rn)];

    if (pre) {
        if (sign < 0)
            addr -= offs;
        else
            addr += offs;
    }

    if (len == 4) {
        arm7->reg[arm7_reg_idx(arm7, rd)] = memory_map_read_32(arm7->map, addr);
    } else {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (!pre)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (writeback)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    next_inst(arm7);
}

static void arm7_op_mrs(struct arm7 *arm7, arm7_inst inst) {
    bool src_psr = (1 << 22) & inst;
    unsigned dst_reg = (inst >> 12) & 0xf;

    uint32_t const *src_p;
    if (src_psr)
        src_p = arm7->reg + arm7_spsr_idx(arm7);
    else
        src_p = arm7->reg + ARM7_REG_CPSR;

    uint32_t *dst_p = arm7->reg + dst_reg;
    *dst_p = *src_p;

    next_inst(arm7);
}

static uint32_t ror(uint32_t in, unsigned n_bits) {
    // TODO: I know there has to be an O(1) way to do this
    while (n_bits--)
        in = ((in & 1) << 31) | (in >> 1);
    return in;
}

static uint32_t decode_immed(arm7_inst inst) {
    uint32_t n_bits = (inst & BIT_RANGE(8, 11)) >> 8;
    uint32_t imm = inst & BIT_RANGE(0, 7);

    return ror(imm, n_bits);
}

static void arm7_op_orr_immed(struct arm7 *arm7, arm7_inst inst) {
    bool s_flag = inst & (1 << 20);
    uint32_t immed = decode_immed(inst);
    unsigned rn = (inst >> 16) & 0xf;
    unsigned rd = (inst >> 12) & 0xf;

    uint32_t input_1 = arm7->reg[arm7_reg_idx(arm7, rn)];
    uint32_t input_2 = immed;

    if (rn == 15) {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t res = input_1 | input_2;

    if (s_flag) {
        uint32_t z_flag = !res ? ARM7_CPSR_Z_MASK : 0;
        uint32_t n_flag = res & (1 << 31) ? ARM7_CPSR_N_MASK : 0;
        arm7->reg[ARM7_REG_CPSR] &= ~(ARM7_CPSR_Z_MASK | ARM7_CPSR_N_MASK);
        arm7->reg[ARM7_REG_CPSR] |= (z_flag | n_flag);
    }

    arm7->reg[arm7_reg_idx(arm7, rd)] = res;

    next_inst(arm7);
}

unsigned arm7_exec(struct arm7 *arm7, struct arm7_decoded_inst const *inst) {
    if (inst->cond(arm7))
        inst->op(arm7, inst->inst);

    /*
     * TODO: how many cycles does it take to execute an instruction when the
     * conditional fails?
     */

    return inst->cycles;
}

static unsigned arm7_reg_idx(struct arm7 *arm7, unsigned reg) {
    switch (arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_M_MASK) {
    case ARM7_MODE_USER:
        return reg;
    case ARM7_MODE_FIQ:
        if (reg >= ARM7_REG_R8 && reg <= ARM7_REG_R14)
            return (reg - ARM7_REG_R8) + ARM7_REG_R8_FIQ;
        return reg;
    case ARM7_MODE_IRQ:
        if (reg >= ARM7_REG_R13 && reg <= ARM7_REG_R14)
            return (reg - ARM7_REG_R13) + ARM7_REG_R13_IRQ;
        return reg;
    case ARM7_MODE_SVC:
        if (reg >= ARM7_REG_R13 && reg <= ARM7_REG_R14)
            return (reg - ARM7_REG_R13) + ARM7_REG_R13_SVC;
        return reg;
    case ARM7_MODE_ABT:
        if (reg >= ARM7_REG_R13 && reg <= ARM7_REG_R14)
            return (reg - ARM7_REG_R13) + ARM7_REG_R13_ABT;
        return reg;
    case ARM7_MODE_UND:
        if (reg >= ARM7_REG_R13 && reg <= ARM7_REG_R14)
            return (reg - ARM7_REG_R13) + ARM7_REG_R13_UND;
        return reg;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static unsigned arm7_spsr_idx(struct arm7 *arm7) {
    switch (arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_M_MASK) {
    case ARM7_MODE_FIQ:
        return ARM7_REG_SPSR_FIQ;
    case ARM7_MODE_IRQ:
        return ARM7_EXCP_IRQ;
    case ARM7_MODE_SVC:
        return ARM7_REG_SPSR_SVC;
    case ARM7_MODE_ABT:
        return ARM7_REG_SPSR_ABT;
    case ARM7_MODE_UND:
        return ARM7_REG_SPSR_UND;
    case ARM7_MODE_USER:
        /* User mode doesn't have an SPSR */
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}
