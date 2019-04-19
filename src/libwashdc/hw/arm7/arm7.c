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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "log.h"
#include "washdc/error.h"
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
static void arm7_inst_mul(struct arm7 *arm7, arm7_inst inst);

static void arm7_inst_swi(struct arm7 *arm7, arm7_inst inst);

static bool arm7_cond_eq(struct arm7 *arm7);
static bool arm7_cond_ne(struct arm7 *arm7);
static bool arm7_cond_cs(struct arm7 *arm7);
static bool arm7_cond_cc(struct arm7 *arm7);

static arm7_cond_fn arm7_cond(arm7_inst inst);

static unsigned arm7_spsr_idx(struct arm7 *arm7);

static uint32_t decode_immed(arm7_inst inst);
static void next_inst(struct arm7 *arm7);

static uint32_t decode_shift(struct arm7 *arm7, arm7_inst inst, bool *carry);

static uint32_t decode_shift_ldr_str(struct arm7 *arm7,
                                     arm7_inst inst, bool *carry);

static void arm7_error_set_regs(void *argptr);

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

static bool arm7_cond_nv(struct arm7 *arm7) {
    return false;
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
    case 15:
        /*
         * ARM7 docs say that software should not use this because its meaning
         * may change in later ARM versions.  Despite this, Daytona USA's devs
         * chose to throw caution to the wind and use it anyways.
         */
        return arm7_cond_nv;
    default:
        // it isn't even possible to jump here tbh
        error_set_arm7_inst(inst);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static struct error_callback arm7_error_callback;

void arm7_init(struct arm7 *arm7,
               struct dc_clock *clk, struct aica_wave_mem *inst_mem) {
    memset(arm7, 0, sizeof(*arm7));
    arm7->clk = clk;
    arm7->inst_mem = inst_mem;

    arm7_error_callback.arg = arm7;
    arm7_error_callback.callback_fn = arm7_error_set_regs;
    error_add_callback(&arm7_error_callback);
}

void arm7_cleanup(struct arm7 *arm7) {
    error_rm_callback(&arm7_error_callback);
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
        arm7->excp_dirty = true;
    }

    arm7->enabled = val;
}

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

#define MASK_EOR (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_EOR (1 << 21)

#define MASK_BIC (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_BIC (14 << 21)

#define MASK_ADD (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_ADD (4 << 21)

#define MASK_SUB (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_SUB (2 << 21)

#define MASK_RSB (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_RSB (3 << 21)

#define MASK_CMP (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_CMP (10 << 21)

#define MASK_TST (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_TST (8 << 21)

#define MASK_AND (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_AND 0

#define MASK_MOV (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_MOV (13 << 21)

#define MASK_MVN (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_MVN (15 << 21)

#define MASK_CMN (BIT_RANGE(21, 24) | BIT_RANGE(26, 27))
#define VAL_CMN (11 << 21)

#define MASK_BLOCK_XFER BIT_RANGE(25, 27)
#define VAL_BLOCK_XFER (4 << 25)

#define MASK_MUL (BIT_RANGE(22, 27) | BIT_RANGE(4, 7))
#define VAL_MUL  (9 << 4)

#define MASK_SWI BIT_RANGE(24, 27)
#define VAL_SWI BIT_RANGE(24, 27)

/* #define MASK_TEQ_IMMED BIT_RANGE(20, 27) */
/* #define VAL_TEQ_IMMED ((1 << 25) | (9 << 21)) */

#define DATA_OP_FUNC_NAME(op_name) arm7_op_##op_name

#define DATA_OP_FUNC_PROTO(op_name) \
DATA_OP_FUNC_NAME(op_name)(uint32_t lhs, uint32_t rhs, bool carry_in,\
                           bool *n_out, bool *c_out, bool *z_out, bool *v_out)

#define DEF_DATA_OP(op_name)                                            \
    static inline uint32_t                                              \
    DATA_OP_FUNC_PROTO(op_name)

DEF_DATA_OP(and) {
    uint32_t val = lhs & rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    return val;
}

/* DEF_DATA_OP(eor) { */
/*     return lhs ^ rhs; */
/* } */

DEF_DATA_OP(sub) {
    /*
     * XXX The nomenclature for lhs/rhs is flipped in ARM7's notation compared
     * to the SH4's notation; that's why I have rhs on the left and lhs on the
     * right here.
     */
    bool c_tmp;
    uint32_t val = sub_flags(rhs, lhs, false, &c_tmp, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    *c_out = !c_tmp;
    return val;
}

DEF_DATA_OP(rsb) {
    /*
     * XXX The nomenclature for lhs/rhs is flipped in ARM7's notation compared
     * to the SH4's notation; that's why I have rhs on the left and lhs on the
     * right here.
     */
    bool c_tmp;
    uint32_t val = sub_flags(lhs, rhs, false, &c_tmp, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    *c_out = !c_tmp;
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

DEF_DATA_OP(cmn) {
    uint32_t val = add_flags(lhs, rhs, false, c_out, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    return 0xdeadbeef;
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
    bool c_tmp;
    uint32_t val = sub_flags(rhs, lhs, false, &c_tmp, v_out);
    *n_out = val & (1 << 31);
    *z_out = !val;
    *c_out = !c_tmp;
    return 0xdeadbabe; // result should never be written
}

/*
 * This is really xor.  For some stupid reason the ARM mnemonic is 'eor' even
 * though literally everbody else in the entire world ignores the silent E and
 * calls this xor.
 */
DEF_DATA_OP(eor) {
    uint32_t val = lhs ^ rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    return val;
}

/* DEF_DATA_OP(cmn) { */
/*     return lhs + rhs; */
/* } */

DEF_DATA_OP(orr) {
    uint32_t val = lhs | rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    return val;
}

DEF_DATA_OP(mov) {
    *n_out = rhs & (1 << 31);
    *z_out = !rhs;

    return rhs;
}

DEF_DATA_OP(mvn) {
    uint32_t val = ~rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

    return val;
}

DEF_DATA_OP(bic) {
    uint32_t val = lhs & ~rhs;
    *n_out = val & (1 << 31);
    *z_out = !val;

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
        c_out = carry_in;                                               \
        if (i_flag) {                                                   \
            input_2 = decode_immed(inst);                               \
        } else {                                                        \
            input_2 = decode_shift(arm7, inst, &c_out);                 \
            if ((inst & (1 << 4)) && rn == 15)                          \
                input_1 += 4;                                           \
        }                                                               \
                                                                        \
        uint32_t res = DATA_OP_FUNC_NAME(op_name)(input_1, input_2,     \
                                                  carry_in, &n_out,     \
                                                  &c_out, &z_out, &v_out); \
        if (s_flag && rd != 15) {                                       \
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
                uint32_t v_flag = v_out ? ARM7_CPSR_V_MASK : 0;         \
                arm7->reg[ARM7_REG_CPSR] &= ~(ARM7_CPSR_Z_MASK |        \
                                              ARM7_CPSR_N_MASK |        \
                                              ARM7_CPSR_C_MASK |        \
                                              ARM7_CPSR_V_MASK);        \
                arm7->reg[ARM7_REG_CPSR] |= (z_flag | n_flag |          \
                                             c_flag | v_flag);          \
            }                                                           \
        } else if (s_flag && rd == 15) {                                \
            arm7->reg[ARM7_REG_CPSR] = arm7->reg[arm7_spsr_idx(arm7)];  \
        } else if (require_s) {                                         \
            RAISE_ERROR(ERROR_INTEGRITY);                               \
        }                                                               \
                                                                        \
        if (write_result) {                                             \
            *arm7_gen_reg(arm7, rd) = res;                              \
            if (rd == 15) {                                             \
                reset_pipeline(arm7);                                   \
                return;                                                 \
            }                                                           \
        }                                                               \
                                                                        \
        next_inst(arm7);                                                \
    }

DEF_INST_FN(orr, true, false, true)
DEF_INST_FN(eor, true, false, true)
DEF_INST_FN(and, true, false, true)
DEF_INST_FN(bic, true, false, true)
DEF_INST_FN(mov, true, false, true)
DEF_INST_FN(add, false, false, true)
DEF_INST_FN(sub, false, false, true)
DEF_INST_FN(rsb, false, false, true)
DEF_INST_FN(cmp, false, true, false)
DEF_INST_FN(tst, true, true, false)
DEF_INST_FN(mvn, true, false, true)
DEF_INST_FN(cmn, false, true, false)

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
     * this one also has to go before the data processing instructions
     * TODO: yet another fake cycle count.
     */
    { arm7_inst_mul, MASK_MUL, VAL_MUL, 4 * S_CYCLE },

    /*
     * TODO: this cycle count is literally just something I made up with no
     * basis in reality.  It needs to be corrected.
     */
    { arm7_inst_orr, MASK_ORR, VAL_ORR, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_eor, MASK_EOR, VAL_EOR, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_bic, MASK_BIC, VAL_BIC, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_mov, MASK_MOV, VAL_MOV, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_add, MASK_ADD, VAL_ADD, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_sub, MASK_SUB, VAL_SUB, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_rsb, MASK_RSB, VAL_RSB, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_cmp, MASK_CMP, VAL_CMP, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_tst, MASK_TST, VAL_TST, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_and, MASK_AND, VAL_AND, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_mvn, MASK_MVN, VAL_MVN, 2 * S_CYCLE + 1 * N_CYCLE },
    { arm7_inst_cmn, MASK_CMN, VAL_CMN, 2 * S_CYCLE + 1 * N_CYCLE },

    { arm7_inst_swi, MASK_SWI, VAL_SWI, 2 * S_CYCLE + 1 * N_CYCLE },

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

void arm7_fetch_inst(struct arm7 *arm7, struct arm7_decoded_inst *inst_out) {
    arm7_check_excp(arm7);

    int cycle_count = 0;
    uint32_t pc = arm7->reg[ARM7_REG_PC];

    if (!arm7->pipeline_full) {
        cycle_count = 2;

        arm7->pipeline_pc[0] = pc + 4;
        arm7->pipeline[0] = do_fetch_inst(arm7, pc + 4);

        arm7->pipeline_pc[1] = pc;
        arm7->pipeline[1] = do_fetch_inst(arm7, pc);

        arm7->pipeline_full = true;

        pc += 8;
        arm7->reg[ARM7_REG_PC] = pc;
    }

    arm7_inst inst_fetched = do_fetch_inst(arm7, pc);
    uint32_t newpc = arm7->pipeline_pc[0];
    arm7_inst newinst = arm7->pipeline[0];
    arm7_inst ret = arm7->pipeline[1];

    arm7->pipeline_pc[0] = pc;
    arm7->pipeline[0] = inst_fetched;
    arm7->pipeline_pc[1] = newpc;
    arm7->pipeline[1] = newinst;

    arm7_decode(arm7, inst_out, ret);
    inst_out->cycles += cycle_count;
}

uint32_t arm7_pc_next(struct arm7 *arm7) {
    if (arm7->pipeline_full)
        return arm7->pipeline_pc[1];
    return arm7->reg[ARM7_REG_PC];
}

static void arm7_check_excp(struct arm7 *arm7) {
    if (arm7->excp_dirty) {
        enum arm7_excp excp = arm7->excp;
        uint32_t cpsr = arm7->reg[ARM7_REG_CPSR];

        /*
         * TODO: if we ever add support for systems other than Dreamcast, we need to
         * check the IRQ line here.  Dreamcast only uses FIQ, so there's no point
         * in checking for IRQ.
         *
         * TODO: also need to check for ARM7_EXCP_DATA_ABORT.
         */

        if (arm7->fiq_line)
            excp |= ARM7_EXCP_FIQ;
        else
            excp &= ~ARM7_EXCP_FIQ;

        if (excp & ARM7_EXCP_RESET) {
            arm7->reg[ARM7_REG_SPSR_SVC] = cpsr;
            arm7->reg[ARM7_REG_R14_SVC] = arm7_pc_next(arm7) + 4;
            arm7->reg[ARM7_REG_PC] = 0;
            arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
                ARM7_MODE_SVC | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
            reset_pipeline(arm7);
            arm7->excp &= ~ARM7_EXCP_RESET;
        } else if ((excp & ARM7_EXCP_FIQ) && !(cpsr & ARM7_CPSR_F_MASK)) {
            arm7->reg[ARM7_REG_SPSR_FIQ] = cpsr;
            arm7->reg[ARM7_REG_R14_FIQ] = arm7_pc_next(arm7) + 4;
            arm7->reg[ARM7_REG_PC] = 0x1c;
            LOG_DBG("FIQ jump to 0x1c\n");
            arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
                ARM7_MODE_FIQ | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
            reset_pipeline(arm7);
            arm7->excp &= ~ARM7_EXCP_FIQ;
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
            arm7->reg[ARM7_REG_R14_SVC] = arm7_pc_next(arm7) + 4;
            arm7->reg[ARM7_REG_PC] = 0;
            arm7->reg[ARM7_REG_CPSR] = (cpsr & ~ARM7_CPSR_M_MASK) |
                ARM7_MODE_SVC | ARM7_CPSR_I_MASK | ARM7_CPSR_F_MASK;
            reset_pipeline(arm7);
            arm7->excp &= ~ARM7_EXCP_SWI;
        }

        arm7->excp_dirty = false;
    }
}

static uint32_t do_fetch_inst(struct arm7 *arm7, uint32_t addr) {
    if (addr <= 0x007fffff)
        return aica_wave_mem_read_32(addr & 0x001fffff, arm7->inst_mem);
    return ~0;
    /* return memory_map_read_32(arm7->map, addr); */
}

/*
 * call this when something like a branch or exception happens that invalidates
 * instructions in the pipeline.
 *
 * This won't effect the PC, but it will clear out anything already in the
 * pipeline.  What that means is that anything in the pipeline which hasn't
 * been executed yet will get trashed.  The upshot of this is that it's only
 * safe to call reset_pipeline when the PC has actually changed.
 */
static void reset_pipeline(struct arm7 *arm7) {
    arm7->pipeline_full = false;
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
    bool carry = (bool)(arm7->reg[ARM7_REG_CPSR] & ARM7_CPSR_C_MASK);

    uint32_t offs;

    if (offs_reg) {
        offs = decode_shift_ldr_str(arm7, inst, &carry);
    } else {
        offs = inst & ((1 << 12) - 1);
    }

    // TODO: should this instruction update the carry flag?

    uint32_t addr = *arm7_gen_reg(arm7, rn);

    if (pre) {
        if (sign < 0)
            addr -= offs;
        else
            addr += offs;
    }


    if (len == 4) {
        if (addr % 4) {
            /*
             * Log this case, it's got some pretty fucked up handling for loads
             * (see below).  Stores appear to only clear the lower two bits, but I
             * must tread carefully; this would not be the first time I
             * misinterpreted an obscure corner-case in ARM7DI's CPU manual.
             */
            LOG_DBG("ARM7 Unaligned memory %s at PC=0x%08x\n",
                    to_mem ? "store" : "load",
                    (unsigned)arm7->reg[ARM7_REG_PC]);
        }
        if (to_mem) {
            uint32_t val = *arm7_gen_reg(arm7, rd);
            if (rd == 15)
                val += 4;
            addr &= ~3;
            memory_map_write_32(arm7->map, addr, val);
        } else {
            uint32_t addr_read = addr & ~3;
            uint32_t val = memory_map_read_32(arm7->map, addr_read);

            /*
             * Deal with unaligned offsets.  It does the load from the aligned
             * address (ie address with bits 0 and 1 cleared) and then
             * right-rotates so that the LSB corresponds to the original
             * unaligned address.
             */
            switch (addr % 4) {
            case 3:
                val = ((val >> 24) & 0xffffff) | (val << 8);
                break;
            case 2:
                val = ((val >> 16) & 0xffffff) | (val << 16);
                break;
            case 1:
                val = ((val >> 8) & 0xffffff) | (val << 24);
                break;
            }
            *arm7_gen_reg(arm7, rd) = val;
        }
    } else {
        if (to_mem) {
            uint32_t val = *arm7_gen_reg(arm7, rd);
            if (rd == 15)
                val += 4;
            memory_map_write_8(arm7->map, addr, val);
        } else {
            *arm7_gen_reg(arm7, rd) = (uint32_t)memory_map_read_8(arm7->map, addr);
        }
    }

    if (!pre) {
        if (writeback) {
            /*
             * docs say the writeback is implied when the pre bit is not set,
             * and that the writeback bit should be zero in this case.
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

    if (!to_mem && rd == 15)
        reset_pipeline(arm7);
    else
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

    if (psr_user_force && (!(reg_list & (1 << 15)) || !load)) {
        error_set_arm7_inst(inst);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    // docs say you cant do this
    if (rn == 15)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    uint32_t *baseptr = arm7_gen_reg(arm7, rn);
    uint32_t base = *baseptr;

    /*
     * This is actually not illegal, but there are some weird corner cases I
     * have to consider first.
     */
    if (writeback && (reg_list & (1 << rn)))
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (!reg_list)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    if (base % 4) {
        error_set_feature("unaligned ARM7 block transfers");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    int reg_no;
    if (up) {
        if (load) {
            for (reg_no = 0; reg_no < 15; reg_no++)
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base += 4;
                    *arm7_gen_reg(arm7, reg_no) =
                        memory_map_read_32(arm7->map, base);
                    if (!pre)
                        base += 4;
                }
            if (reg_list & (1 << 15)) {
                if (psr_user_force)
                    arm7->reg[ARM7_REG_CPSR] = arm7->reg[arm7_spsr_idx(arm7)];
                if (pre)
                    base += 4;
                arm7->reg[ARM7_REG_PC] =
                    memory_map_read_32(arm7->map, base);
                if (!pre)
                    base += 4;
            }
        } else {
            // store
            if (psr_user_force)
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            for (reg_no = 0; reg_no < 15; reg_no++)
                if (reg_list & (1 << reg_no)) {
                    if (pre)
                        base += 4;
                    memory_map_write_32(arm7->map, base,
                                        *arm7_gen_reg(arm7, reg_no));
                    if (!pre)
                        base += 4;
                }

            if (reg_list & (1 << 15)) {
                if (pre)
                    base += 4;
                memory_map_write_32(arm7->map, base,
                                    arm7->reg[ARM7_REG_PC] + 4);
                if (!pre)
                    base += 4;
            }
        }
    } else {
        if (psr_user_force)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
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
            if (reg_list & (1 << 15)) {
                if (psr_user_force)
                    RAISE_ERROR(ERROR_UNIMPLEMENTED);
                if (pre)
                    base -= 4;
                memory_map_write_32(arm7->map, base,
                                    arm7->reg[ARM7_REG_PC] + 4);
                if (!pre)
                    base -= 4;
            }

            for (reg_no = 14; reg_no >= 0; reg_no--) {
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
        *baseptr = base;

    if (load && (reg_list & (1 << 15)))
        reset_pipeline(arm7);
    else
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

static void arm7_inst_mul(struct arm7 *arm7, arm7_inst inst) {
    bool accum = (bool)(inst & (1 << 21));
    bool set_flags = (bool)(inst & (1 << 20));
    unsigned rd = (BIT_RANGE(16, 19) & inst) >> 16;
    unsigned rn = (BIT_RANGE(12, 15) & inst) >> 12;
    unsigned rs = (BIT_RANGE(8, 11) & inst) >> 8;
    unsigned rm = BIT_RANGE(0, 3) & inst;

#ifdef INVARIANTS
    if ((BIT_RANGE(22, 27) & inst) || (((BIT_RANGE(4, 7) & inst) >> 4) != 9))
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    // doc says you can't do this
    if (rd == rm)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    // doc says you can't do this either
    if (rd == 15 || rn == 15 || rs == 15 || rm == 15)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    uint32_t val = *arm7_gen_reg(arm7, rm) * *arm7_gen_reg(arm7, rs);
    if (accum)
        val += *arm7_gen_reg(arm7, rn);

    *arm7_gen_reg(arm7, rd) = val;

    if (set_flags) {
        uint32_t cpsr = arm7->reg[ARM7_REG_CPSR];
        if (val & (1 << 31))
            cpsr |= ARM7_CPSR_N_MASK;
        else
            cpsr &= ~ARM7_CPSR_N_MASK;

        if (!val)
            cpsr |= ARM7_CPSR_Z_MASK;
        else
            cpsr &= ~ARM7_CPSR_Z_MASK;

        // apparently the value of C is undefined
        cpsr &= ~ARM7_CPSR_C_MASK;

        // V flag is unaffected by this instruction

        arm7->reg[ARM7_REG_CPSR] = cpsr;
    }

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

static uint32_t do_decode_shift(struct arm7 *arm7, unsigned shift_fn,
                                uint32_t src_val, unsigned shift_amt,
                                bool *carry) {
    uint32_t ret_val;
    /*
     * For all cases except logical left-shift, a shift of 0 is actually a
     * shift of 32.  For now I've chosen to raise an ERROR_UNIMPLEMENTED when
     * that happens because I'd rather not think about it.
     */
    switch (shift_fn) {
    case 0:
        // logical left-shift
        if (shift_amt) {
            // LSL 0 doesn't effect the carry flag
            if (shift_amt < 32)
                *carry = (bool)(1 << (31 - shift_amt + 1) & src_val);
            else
                *carry = false;
        }
        return src_val << shift_amt;
    case 1:
        // logical right-shift
        if (shift_amt) {
            if (shift_amt < 32)
                *carry = ((1 << (shift_amt - 1)) & src_val);
            else
                *carry = (1 << 31) & src_val;
        }
        return src_val >> shift_amt;
    case 2:
        // arithmetic right-shift
        if (shift_amt == 0)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        if (shift_amt < 32)
            *carry = ((1 << (shift_amt - 1)) & src_val);
        else
            *carry = (1 << 31) & src_val;
        return ((int32_t)src_val) >> shift_amt;
    case 3:
        // right-rotate
        if (shift_amt == 0)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        ret_val = ror(src_val, shift_amt);
        *carry = (1 << 31) & ret_val;
        return ret_val;
    }

    RAISE_ERROR(ERROR_INTEGRITY);
}

static uint32_t
decode_shift_ldr_str(struct arm7 *arm7, arm7_inst inst, bool *carry) {
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

    return do_decode_shift(arm7, shift_fn, src_val, shift_amt, carry);
}

static uint32_t
decode_shift(struct arm7 *arm7, arm7_inst inst, bool *carry) {
    bool amt_in_reg = inst & (1 << 4);
    unsigned shift_fn = (inst & BIT_RANGE(5, 6)) >> 5;
    uint32_t shift_amt;

    /*
     * For all cases except logical left-shift, a shift of 0 is actually a
     * shift of 32.  For now I've chosen to raise an ERROR_UNIMPLEMENTED when
     * that happens because I'd rather not think about it.
     */
    if (amt_in_reg) {
        if (inst & (1 << 7)) {
            /*
             * setting bit 7 and bit 4 is illegal.  If this happens, it means
             * we have a decoder error.
             */
            RAISE_ERROR(ERROR_INTEGRITY);
        }

        unsigned reg_no = (inst & BIT_RANGE(8, 11)) >> 8;
        if (reg_no == 15)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        shift_amt = *arm7_gen_reg(arm7, reg_no) & 0xff;
    } else {
        shift_amt = (inst & BIT_RANGE(7, 11)) >> 7;
    }

    unsigned src_reg = inst & 0xf;
    uint32_t src_val = *arm7_gen_reg(arm7, src_reg);

    return do_decode_shift(arm7, shift_fn, src_val, shift_amt, carry);
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

static void arm7_inst_swi(struct arm7 *arm7, arm7_inst inst) {
    LOG_WARN("Untested ARM7 SWI instruction used\n");
    arm7->excp |= ARM7_EXCP_SWI;
    arm7->excp_dirty = true;
    // it is not a mistake that I have chosen to not call next_inst here
}

void arm7_get_regs(struct arm7 *arm7, void *dat_out) {
    memcpy(dat_out, arm7->reg, sizeof(uint32_t) * ARM7_REGISTER_COUNT);
}

static DEF_ERROR_U32_ATTR(arm7_reg_r0)
static DEF_ERROR_U32_ATTR(arm7_reg_r1)
static DEF_ERROR_U32_ATTR(arm7_reg_r2)
static DEF_ERROR_U32_ATTR(arm7_reg_r3)
static DEF_ERROR_U32_ATTR(arm7_reg_r4)
static DEF_ERROR_U32_ATTR(arm7_reg_r5)
static DEF_ERROR_U32_ATTR(arm7_reg_r6)
static DEF_ERROR_U32_ATTR(arm7_reg_r7)
static DEF_ERROR_U32_ATTR(arm7_reg_r8)
static DEF_ERROR_U32_ATTR(arm7_reg_r9)
static DEF_ERROR_U32_ATTR(arm7_reg_r10)
static DEF_ERROR_U32_ATTR(arm7_reg_r11)
static DEF_ERROR_U32_ATTR(arm7_reg_r12)
static DEF_ERROR_U32_ATTR(arm7_reg_r13)
static DEF_ERROR_U32_ATTR(arm7_reg_r14)
static DEF_ERROR_U32_ATTR(arm7_reg_r15)

// putthing this here even though it's just an alias for r15
static DEF_ERROR_U32_ATTR(arm7_reg_pc)

static DEF_ERROR_U32_ATTR(arm7_reg_r8_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r9_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r10_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r11_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r12_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r13_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r14_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_r13_svc)
static DEF_ERROR_U32_ATTR(arm7_reg_r14_svc)
static DEF_ERROR_U32_ATTR(arm7_reg_r13_abt)
static DEF_ERROR_U32_ATTR(arm7_reg_r14_abt)
static DEF_ERROR_U32_ATTR(arm7_reg_r13_irq)
static DEF_ERROR_U32_ATTR(arm7_reg_r14_irq)
static DEF_ERROR_U32_ATTR(arm7_reg_r13_und)
static DEF_ERROR_U32_ATTR(arm7_reg_r14_und)

static DEF_ERROR_U32_ATTR(arm7_reg_cpsr)

static DEF_ERROR_U32_ATTR(arm7_reg_spsr_fiq)
static DEF_ERROR_U32_ATTR(arm7_reg_spsr_svc)
static DEF_ERROR_U32_ATTR(arm7_reg_spsr_abt)
static DEF_ERROR_U32_ATTR(arm7_reg_spsr_irq)
static DEF_ERROR_U32_ATTR(arm7_reg_spsr_und)

static void arm7_error_set_regs(void *argptr) {
    struct arm7 *arm7 = (struct arm7*)argptr;

    error_set_arm7_reg_r0(arm7->reg[ARM7_REG_R0]);
    error_set_arm7_reg_r1(arm7->reg[ARM7_REG_R1]);
    error_set_arm7_reg_r2(arm7->reg[ARM7_REG_R2]);
    error_set_arm7_reg_r3(arm7->reg[ARM7_REG_R3]);
    error_set_arm7_reg_r4(arm7->reg[ARM7_REG_R4]);
    error_set_arm7_reg_r5(arm7->reg[ARM7_REG_R5]);
    error_set_arm7_reg_r6(arm7->reg[ARM7_REG_R6]);
    error_set_arm7_reg_r7(arm7->reg[ARM7_REG_R7]);
    error_set_arm7_reg_r8(arm7->reg[ARM7_REG_R8]);
    error_set_arm7_reg_r9(arm7->reg[ARM7_REG_R9]);
    error_set_arm7_reg_r10(arm7->reg[ARM7_REG_R10]);
    error_set_arm7_reg_r11(arm7->reg[ARM7_REG_R11]);
    error_set_arm7_reg_r12(arm7->reg[ARM7_REG_R12]);
    error_set_arm7_reg_r13(arm7->reg[ARM7_REG_R13]);
    error_set_arm7_reg_r14(arm7->reg[ARM7_REG_R14]);
    error_set_arm7_reg_r15(arm7->reg[ARM7_REG_R15]);

    // putting this here even though it's just an alias for r15
    error_set_arm7_reg_pc(arm7->reg[ARM7_REG_PC]);

    error_set_arm7_reg_r8_fiq(arm7->reg[ARM7_REG_R8_FIQ]);
    error_set_arm7_reg_r9_fiq(arm7->reg[ARM7_REG_R9_FIQ]);
    error_set_arm7_reg_r10_fiq(arm7->reg[ARM7_REG_R10_FIQ]);
    error_set_arm7_reg_r11_fiq(arm7->reg[ARM7_REG_R11_FIQ]);
    error_set_arm7_reg_r12_fiq(arm7->reg[ARM7_REG_R12_FIQ]);
    error_set_arm7_reg_r13_fiq(arm7->reg[ARM7_REG_R13_FIQ]);
    error_set_arm7_reg_r14_fiq(arm7->reg[ARM7_REG_R14_FIQ]);
    error_set_arm7_reg_r13_svc(arm7->reg[ARM7_REG_R13_SVC]);
    error_set_arm7_reg_r14_svc(arm7->reg[ARM7_REG_R14_SVC]);
    error_set_arm7_reg_r13_abt(arm7->reg[ARM7_REG_R13_ABT]);
    error_set_arm7_reg_r14_abt(arm7->reg[ARM7_REG_R14_ABT]);
    error_set_arm7_reg_r13_irq(arm7->reg[ARM7_REG_R13_IRQ]);
    error_set_arm7_reg_r14_irq(arm7->reg[ARM7_REG_R14_IRQ]);
    error_set_arm7_reg_r13_und(arm7->reg[ARM7_REG_R13_UND]);
    error_set_arm7_reg_r14_und(arm7->reg[ARM7_REG_R14_UND]);

    error_set_arm7_reg_cpsr(arm7->reg[ARM7_REG_CPSR]);

    error_set_arm7_reg_spsr_fiq(arm7->reg[ARM7_REG_SPSR_FIQ]);
    error_set_arm7_reg_spsr_svc(arm7->reg[ARM7_REG_SPSR_SVC]);
    error_set_arm7_reg_spsr_abt(arm7->reg[ARM7_REG_SPSR_ABT]);
    error_set_arm7_reg_spsr_irq(arm7->reg[ARM7_REG_SPSR_IRQ]);
    error_set_arm7_reg_spsr_und(arm7->reg[ARM7_REG_SPSR_UND]);
}

void arm7_set_fiq(struct arm7 *arm7) {
    arm7->fiq_line = true;
    arm7->excp_dirty = true;
}

void arm7_clear_fiq(struct arm7 *arm7) {
    arm7->fiq_line = false;
}
