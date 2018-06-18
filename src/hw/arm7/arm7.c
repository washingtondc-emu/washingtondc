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
#include "intmath.h"

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

static void arm7_inst_branch(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_ldr_str(struct arm7 *arm7, arm7_inst inst);
static void arm7_block_xfer(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_mrs(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_msr(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_orr(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_bic(struct arm7 *arm7, arm7_inst inst);
static void arm7_inst_mov(struct arm7 *arm7, arm7_inst inst);

static bool arm7_cond_eq(struct arm7 *arm7);
static bool arm7_cond_ne(struct arm7 *arm7);
static bool arm7_cond_cs(struct arm7 *arm7);
static bool arm7_cond_cc(struct arm7 *arm7);

static arm7_cond_fn arm7_cond(arm7_inst inst);

static unsigned arm7_spsr_idx(struct arm7 *arm7);

static uint32_t decode_immed(arm7_inst inst);
static void next_inst(struct arm7 *arm7);

static uint32_t decode_shift(struct arm7 *arm7, arm7_inst inst);

static uint32_t decode_shift_ldr_str(struct arm7 *arm7, arm7_inst inst);

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

#define MASK_LDR_STR BIT_RANGE(26, 27)
#define VAL_LDR_STR  0x04000000

#define MASK_STR (BIT_RANGE(26, 27) | (1 << 20))
#define VAL_STR  0x04000000

#define MASK_MRS (BIT_RANGE(23, 27) | BIT_RANGE(16, 21) | BIT_RANGE(0, 11))
#define VAL_MRS  0x010f0000

#define MASK_MSR (BIT_RANGE(23, 27) | BIT_RANGE(4, 21))
#define VAL_MSR  (0x0129f000)

// data processing opcodes
#define MASK_ORR (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_ORR (12 << 21)

#define MASK_BIC (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_BIC (14 << 21)

#define MASK_ADD (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_ADD (4 << 21)

#define MASK_SUB (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_SUB (2 << 21)

#define MASK_CMP (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_CMP (10 << 21)

#define MASK_TST (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_TST (8 << 21)

#define MASK_MOV (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_MOV (13 << 21)

#define MASK_BLOCK_XFER BIT_RANGE(25, 27)
#define VAL_BLOCK_XFER (4 << 25)

/* #define MASK_TEQ_IMMED BIT_RANGE(20, 27) */
/* #define VAL_TEQ_IMMED ((1 << 25) | (9 << 21)) */

#define DATA_OP_FUNC_NAME(op_name) arm7_op_##op_name

#define DATA_OP_FUNC_PROTO(op_name) \
DATA_OP_FUNC_NAME(op_name)(uint32_t lhs, uint32_t rhs, bool carry_in,\
                           bool *n_out, bool *c_out, bool *z_out, bool *v_out)

#define DEF_DATA_OP(op_name)                                            \
    static inline uint32_t                                              \
    DATA_OP_FUNC_PROTO(op_name)

/* DEF_DATA_OP(and) { */
/*     return lhs & rhs; */
/* } */

/* DEF_DATA_OP(eor) { */
/*     return lhs ^ rhs; */
/* } */

DEF_DATA_OP(sub) {
    /*
     * XXX The nomenclature for lhs/rhs is flipped in ARM7's notation compared
     * to the SH4's notation; that's why I have rhs on the left and lhs on the
     * right here.
     */
    uint32_t val = sub_flags(rhs, lhs, false, c_out, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    return val;
}

/* DEF_DATA_OP(rsb) { */
/*     return rhs - lhs; */
/* } */

DEF_DATA_OP(add) {
    uint32_t val = add_flags(lhs, rhs, false, c_out, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    return val;
}

/* DEF_DATA_OP(adc) { */
/*     return lhs + rhs + carry_in; */
/* } */

/* DEF_DATA_OP(sbc) { */
/*     return (lhs - rhs) + (carry_in - 1); */
/* } */

/* DEF_DATA_OP(rsc) { */
/*     return (rhs - lhs) + (carry_in - 1); */
/* } */

DEF_DATA_OP(tst) {
    uint32_t val = lhs & rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    /*
     * TODO: c_out is supposed to be set to the output from the barrel shifter
     * (whatever that means)
     */
    *c_out = false;
    return 0xdeadbabe; // result should never be written
}

/* DEF_DATA_OP(teq) { */
/*     return lhs ^ rhs; */
/* } */

DEF_DATA_OP(cmp) {
    /*
     * XXX The nomenclature for lhs/rhs is flipped in ARM7's notation compared
     * to the SH4's notation; that's why I have rhs on the left and lhs on the
     * right here.
     */
    uint32_t val = sub_flags(rhs, lhs, false, c_out, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    return 0xdeadbabe; // result should never be written
}

/* DEF_DATA_OP(cmn) { */
/*     return lhs + rhs; */
/* } */

DEF_DATA_OP(orr) {
    uint32_t val = lhs | rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    /*
     * TODO: c_out is supposed to be set to the output from the barrel shifter
     * (whatever that means)
     */
    *c_out = false;
    return val;
}

DEF_DATA_OP(mov) {
    *n_out = rhs & (1 << 31);
    *z_out = !rhs;

    /*
     * TODO: c_out is supposed to be set to the output from the barrel shifter
     * (whatever that means)
     */
    *c_out = false;
    return rhs;
}

DEF_DATA_OP(bic) {
    uint32_t val = lhs & ~rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    /*
     * TODO: c_out is supposed to be set to the output from the barrel shifter
     * (whatever that means)
     */
    *c_out = false;
    return val;
}

/* DEF_DATA_OP(mv) { */
/*     return ~rhs; */
/* } */

#define DEF_INST_FN(op_name, is_logic, require_s, write_result)         \
    __attribute__((unused)) static void                                 \
    arm7_inst_##op_name(struct arm7 *arm7, arm7_inst inst) {            \
        bool s_flag = inst & (1 << 20);                                 \
        bool i_flag = inst & (1 << 25);                                 \
        unsigned rn = (inst >> 16) & 0xf;                               \
        unsigned rd = (inst >> 12) & 0xf;                               \
                                                                        \
        bool carry_in = arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_C_MASK;    \
        bool n_out, c_out, z_out, v_out;                                \
                                                                        \
        uint32_t input_1 = *arm7_gen_reg(arm7, rn);                     \
        uint32_t input_2;                                               \
                                                                        \
        if (i_flag) {                                                   \
            input_2 = decode_immed(inst);                               \
        } else {                                                        \
            input_2 = decode_shift(arm7, inst);                         \
        }                                                               \
                                                                        \
        if (rn == 15) {                                                 \
            printf("Unimplemented: PC as source register\n");           \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
        if (rd == 15 && s_flag) {                                       \
            printf("Unimplemented: PC as destination register with s_flag\n"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
                                                                        \
        uint32_t res = DATA_OP_FUNC_NAME(op_name)(input_1, input_2,     \
                                                  carry_in, &n_out,     \
                                                  &c_out, &z_out, &v_out); \
        if (s_flag) {                                                   \
            if (is_logic) {                                             \
                uint32_t z_flag = z_out ? ARM7_CPSR_Z_MASK : 0;         \
                uint32_t n_flag = n_out ? ARM7_CPSR_N_MASK : 0;         \
                uint32_t c_flag = c_out ? ARM7_CPSR_C_MASK : 0;         \
                arm7->reg[ARM7_REG_CPSR] &= ~(ARM7_CPSR_Z_MASK |        \
                                              ARM7_CPSR_N_MASK |        \
                                              ARM7_CPSR_C_MASK);        \
                arm7->reg[ARM7_REG_CPSR] |= (z_flag | n_flag | c_flag); \
            } else {                                                    \
                uint32_t z_flag = z_out ? ARM7_CPSR_Z_MASK : 0;         \
                uint32_t n_flag = n_out ? ARM7_CPSR_N_MASK : 0;         \
                uint32_t c_flag = c_out ? ARM7_CPSR_C_MASK : 0;         \
                uint32_t v_flag = c_out ? ARM7_CPSR_V_MASK : 0;         \
                arm7->reg[ARM7_REG_CPSR] &= ~(ARM7_CPSR_Z_MASK |        \
                                              ARM7_CPSR_N_MASK |        \
                                              ARM7_CPSR_C_MASK |        \
                                              ARM7_CPSR_V_MASK);        \
                arm7->reg[ARM7_REG_CPSR] |= (z_flag | n_flag |          \
                                             c_flag | v_flag);          \
            }                                                           \
        } else if (require_s) {                                         \
            RAISE_ERROR(ERROR_INTEGRITY);                               \
        }                                                               \
                                                                        \
        if (write_result)                                               \
            *arm7_gen_reg(arm7, rd) = res;                              \
                                                                        \
        if (rd == 15)                                                   \
            reset_pipeline(arm7);                                       \
                                                                        \
        next_inst(arm7);                                                \
    }

DEF_INST_FN(orr, true, false, true)
DEF_INST_FN(bic, true, false, true)
DEF_INST_FN(mov, true, false, true)
DEF_INST_FN(add, false, false, true)
DEF_INST_FN(sub, false, false, true)
DEF_INST_FN(cmp, false, true, false)
DEF_INST_FN(tst, true, true, false)

typedef void(*arm7_opcode_fn)(struct arm7*, arm7_inst);

static struct arm7_opcode {
    arm7_opcode_fn fn;
    arm7_inst mask;
    arm7_inst val;
    unsigned n_cycles;
} const ops[] = {
    /*
     * TODO: these cycle counts are mostly bullshit.  I don't even know if it's
     * valid to assume that any given opcode will always take the same number
     * of cycles.  ARM has a lot of corner cases and it's all bullsht, really.
     */

    // branch (with or without link)
    { arm7_inst_branch, MASK_B, VAL_B, 2 * S_CYCLE + 1 * N_CYCLE },

    /*
     * TODO: this is supposed to take 2 * S_CYCLE + 2 * N_CYCLE + I_CYCLE
     * cycles if R15 is involved...?
     */
    { arm7_inst_ldr_str, MASK_LDR_STR, VAL_LDR_STR,
      1 * S_CYCLE + 1 * N_CYCLE + 1 * I_CYCLE },

    // TODO: yet another made up fictional cycle-count
    { arm7_block_xfer, MASK_BLOCK_XFER, VAL_BLOCK_XFER,
      1 * S_CYCLE + 1 * N_CYCLE + 1 * I_CYCLE },

    /*
     * It's important that these always go *before* the data processing
     * instructions due to opcode overlap.
     */
    { arm7_inst_mrs, MASK_MRS, VAL_MRS, 1 * S_CYCLE },
    { arm7_inst_msr, MASK_MSR, VAL_MSR, 1 * S_CYCLE },

    /*
     * TODO: this cycle count is literally just something I made up with no
     * basis in reality.  It needs to be corrected.
     */
    { arm7_inst_orr, MASK_ORR, VAL_ORR, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_bic, MASK_BIC, VAL_BIC, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_mov, MASK_MOV, VAL_MOV, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_add, MASK_ADD, VAL_ADD, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_sub, MASK_SUB, VAL_SUB, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_cmp, MASK_CMP, VAL_CMP, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_tst, MASK_TST, VAL_TST, 2 * S_CYCLE + 1 * N_CYCLE },

    { NULL }
};

void arm7_decode(struct arm7 *arm7, struct arm7_decoded_inst *inst_out,
                 arm7_inst inst) {
    struct arm7_opcode const *curs = ops;
    while (curs->fn) {
        if ((curs->mask & inst) == curs->val) {
            inst_out->op = curs->fn;
            inst_out->cycles = curs->n_cycles;
            goto return_success;
        }
        curs++;
    }

    error_set_arm7_inst(inst);
    error_set_arm7_pc(arm7->reg[ARM7_REG_PC]);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);

return_success:
    inst_out->cond = arm7_cond(inst);
    inst_out->inst = inst;
}

static void next_inst(struct arm7 *arm7) {
    arm7->reg[ARM7_REG_PC] += 4;
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
        arm7->pipeline[0] = do_fetch_inst(arm7, arm7->reg[ARM7_REG_PC]);
        arm7_decode(arm7, inst_out, ret);
        return;
    case 1:
        arm7->pipeline[1] = arm7->pipeline[0];
    case 0:
        arm7->pipeline[0] = do_fetch_inst(arm7, arm7->reg[ARM7_REG_PC]);
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
        arm7->reg[ARM7_REG_R14_SVC] = arm7->reg[ARM7_REG_PC] - 4;
        arm7->reg[ARM7_REG_PC] = 0;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_SVC | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_RESET;
    } else if (excp & ARM7_EXCP_DATA_ABORT) {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else if ((excp & ARM7_EXCP_FIQ) && !(cpsr & ARM7_CPSR_I_MASK)) {
        arm7->reg[ARM7_REG_SPSR_FIQ] = cpsr;
        arm7->reg[ARM7_REG_R14_FIQ] = arm7->reg[ARM7_REG_PC] - 4;
        arm7->reg[ARM7_REG_PC] = 0x1c;
        arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
            ARM7_MODE_FIQ | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
        reset_pipeline(arm7);
        arm7->excp &= ~ARM7_EXCP_FIQ;
    } else if ((excp & ARM7_EXCP_IRQ) && !(cpsr & ARM7_CPSR_F_MASK)) {
        arm7->reg[ARM7_REG_SPSR_IRQ] = cpsr;
        arm7->reg[ARM7_REG_R14_IRQ] = arm7->reg[ARM7_REG_PC] - 4;
        arm7->reg[ARM7_REG_PC] = 0x18;
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
        arm7->reg[ARM7_REG_R14_SVC] = arm7->reg[ARM7_REG_PC] - 4;
        arm7->reg[ARM7_REG_PC] = 0;
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

static void arm7_inst_branch(struct arm7 *arm7, arm7_inst inst) {
    uint32_t offs = inst & ((1 << 24) - 1);
    if (offs & (1 << 23))
        offs |= 0xff000000;
    offs <<= 2;

    if (inst & (1 << 24)) {
        // link bit
        *arm7_gen_reg(arm7, 14) = arm7->reg[ARM7_REG_PC] - 4;
    }

    uint32_t pc_new = offs + arm7->reg[ARM7_REG_PC];

    arm7->reg[ARM7_REG_PC] = pc_new;
    reset_pipeline(arm7);
}

static void arm7_inst_ldr_str(struct arm7 *arm7, arm7_inst inst) {
    unsigned rn = (inst >> 16) & 0xf;
    unsigned rd = (inst >> 12) & 0xf;

    bool writeback = inst & (1 << 21);
    int len = (inst & (1 << 22)) ? 1 : 4;
    int sign = (inst & (1 << 23)) ? 1 : -1;
    bool pre = inst & (1 << 24);
    bool offs_reg = inst & (1 << 25);
    bool to_mem = !(inst & (1 << 20));

    uint32_t offs;

    if (rd == 15)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (offs_reg) {
        offs = decode_shift_ldr_str(arm7, inst);
    } else {
        offs = inst & ((1 << 12) - 1);
    }

    uint32_t addr = *arm7_gen_reg(arm7, rn);

    if (pre) {
        if (sign < 0)
            addr -= offs;
        else
            addr += offs;
    }

    if (len == 4) {
        if (addr % 4) {
            // unaligned memory access
            error_set_arm7_inst(inst);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (to_mem)
            memory_map_write_32(arm7->map, addr, *arm7_gen_reg(arm7, rd));
        else
            *arm7_gen_reg(arm7, rd) = memory_map_read_32(arm7->map, addr);
    } else {
        if (to_mem)
            memory_map_write_8(arm7->map, addr, *arm7_gen_reg(arm7, rd));
        else
            *arm7_gen_reg(arm7, rd) = (uint32_t)memory_map_read_8(arm7->map, addr);
    }

    if (!pre) {
        if (writeback) {
            /*
             * docs say the writeback is implied when the pre bit is set, and
             * that the writeback bit should be zero in this case.
             */
            error_set_arm7_inst(inst);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        writeback = true;
        if (sign < 0)
            addr -= offs;
        else
            addr += offs;
    }

    if (writeback) {
        if (rn == 15)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        *arm7_gen_reg(arm7, rn) = addr;
    }

    next_inst(arm7);
}

static void arm7_block_xfer(struct arm7 *arm7, arm7_inst inst) {
    unsigned rn = (inst & BIT_RANGE(16, 19)) >> 16;
    unsigned reg_list = inst & 0xffff;
    bool pre = (bool)(inst & (1 << 24));
    bool up = (bool)(inst & (1 << 23));
    bool psr_user_force = (bool)(inst & (1 << 22));
    bool writeback = (bool)(inst & (1 << 21));
    bool load = (bool)(inst & (1 << 20));

    if (psr_user_force)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    // docs say you cant do this
    if (rn == 15)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    // docs say you *can* do this, but I haven't implemented it yet
    if (reg_list & (1 << 15))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    uint32_t base = *arm7_gen_reg(arm7, rn);

    /*
     * This is actually not illegal, but there are some weird corner cases I
     * have to consider first.
     */
    if (writeback && (reg_list & (1 << rn)))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    int reg_no;
    if (up) {
        if (load) {
            for (reg_no = 0; reg_no < 16; reg_no++)
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base += 4;
                    *arm7_gen_reg(arm7, reg_no) =
                        memory_map_read_32(arm7->map, base);
                    if (!pre)
                        base += 4;
                }
        } else {
            for (reg_no = 0; reg_no < 16; reg_no++)
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base += 4;
                    memory_map_write_32(arm7->map, base,
                                        *arm7_gen_reg(arm7, reg_no));
                    if (!pre)
                        base += 4;
                }
        }
    } else {
        /*
         * TODO:
         * This transfers higher registers before lower registers.  The spec
         * says that lower registers must always go first.  I don't think that
         * will be a problem since it all happens instantly, but it's something
         * to keep in mind if you ever try to use this interpreter on a system
         * which has a FIFO register like the one SH4 uses to communicate with
         * PowerVR2's Tile Accelerator.
         */
        if (load) {
            for (reg_no = 15; reg_no >= 0; reg_no--) {
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base -= 4;
                    *arm7_gen_reg(arm7, reg_no) =
                        memory_map_read_32(arm7->map, base);
                    if (!pre)
                        base -= 4;
                }
            }
        } else {
            for (reg_no = 15; reg_no >= 0; reg_no--) {
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base -= 4;
                    memory_map_write_32(arm7->map, base,
                                        *arm7_gen_reg(arm7, reg_no));
                    if (!pre)
                        base -= 4;
                }
            }
        }
    }

    /*
     * Now handle the writeback.  Spec has some fairly complicated rules about
     * this when the rn is in the register list, but the code above should have
     * raised an ERROR_UNIMPLEMENTED if that was the case.
     */
    if (writeback)
        *arm7_gen_reg(arm7, rn) = base;

    next_inst(arm7);
}

/*
 * MRS
 * Copy CPSR (or SPSR) to a register
 */
static void arm7_inst_mrs(struct arm7 *arm7, arm7_inst inst) {
    bool src_psr = (1 << 22) & inst;
    unsigned dst_reg = (inst >> 12) & 0xf;

    uint32_t const *src_p;
    if (src_psr)
        src_p = arm7->reg + arm7_spsr_idx(arm7);
    else
        src_p = arm7->reg + ARM7_REG_CPSR;

    *arm7_gen_reg(arm7, dst_reg) = *src_p;

    next_inst(arm7);
}

/*
 * MSR
 * Copy a register to CPSR (or SPSR)
 */
static void arm7_inst_msr(struct arm7 *arm7, arm7_inst inst) {
    bool dst_psr = (1 << 22) & inst;

    uint32_t *dst_p;
    if (dst_psr)
        dst_p = arm7->reg + arm7_spsr_idx(arm7);
    else
        dst_p = arm7->reg + ARM7_REG_CPSR;

    unsigned src_reg = inst & 0xff;
    *dst_p = *arm7_gen_reg(arm7, src_reg);

    next_inst(arm7);
}

static uint32_t ror(uint32_t in, unsigned n_bits) {
    // TODO: I know there has to be an O(1) way to do this
    while (n_bits--)
        in = ((in & 1) << 31) | (in >> 1);
    return in;
}

static uint32_t decode_immed(arm7_inst inst) {
    uint32_t n_bits = 2 * ((inst & BIT_RANGE(8, 11)) >> 8);
    uint32_t imm = inst & BIT_RANGE(0, 7);

    return ror(imm, n_bits);
}

static uint32_t
decode_shift_ldr_str(struct arm7 *arm7, arm7_inst inst) {
    bool amt_in_reg = inst & (1 << 4);
    unsigned shift_fn = (inst & BIT_RANGE(5, 6)) >> 5;
    uint32_t shift_amt;

    if (amt_in_reg) {
        // Docs say this feature isn't available for load/store
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else {
        shift_amt = (inst & BIT_RANGE(7, 11)) >> 7;
    }

    unsigned src_reg = inst & 0xf;
    uint32_t src_val = *arm7_gen_reg(arm7, src_reg);

    switch (shift_fn) {
    case 0:
        // logical left-shift
        return src_val << shift_amt;
    case 1:
        // logical right-shift
        return src_val >> shift_amt;
    case 2:
        // arithmetic right-shift
        return ((int32_t)src_val) >> shift_amt;
    case 3:
        // right-rotate
        return ror(src_val, shift_amt);
    }

    RAISE_ERROR(ERROR_INTEGRITY);
}

static uint32_t
decode_shift(struct arm7 *arm7, arm7_inst inst) {
    bool amt_in_reg = inst & (1 << 4);
    unsigned shift_fn = (inst & BIT_RANGE(5, 6)) >> 5;
    uint32_t shift_amt;

    if (amt_in_reg) {
        unsigned reg_no = (inst & BIT_RANGE(8, 11)) >> 8;
        if (reg_no == 15)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        shift_amt = *arm7_gen_reg(arm7, reg_no) & 0xff;
    } else {
        shift_amt = (inst & BIT_RANGE(7, 11)) >> 7;
    }

    unsigned src_reg = inst & 0xf;
    uint32_t src_val = *arm7_gen_reg(arm7, src_reg);

    switch (shift_fn) {
    case 0:
        // logical left-shift
        return src_val << shift_amt;
    case 1:
        // logical right-shift
        return src_val >> shift_amt;
    case 2:
        // arithmetic right-shift
        return ((int32_t)src_val) >> shift_amt;
    case 3:
        // right-rotate
        return ror(src_val, shift_amt);
    }

    RAISE_ERROR(ERROR_INTEGRITY);
}

unsigned arm7_exec(struct arm7 *arm7, struct arm7_decoded_inst const *inst) {
    if (inst->cond(arm7))
        inst->op(arm7, inst->inst);
    else
        next_inst(arm7);

    /*
     * TODO: how many cycles does it take to execute an instruction when the
     * conditional fails?
     */

    return inst->cycles;
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
