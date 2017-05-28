/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#include <fenv.h>
#include <stdlib.h>
#include <string.h>

#include "sh4_mmu.h"
#include "sh4_excp.h"
#include "sh4_reg.h"
#include "sh4_inst.h"
#include "error.h"
#include "dreamcast.h"

#include "sh4.h"

static void sh4_error_set_regs(void *argptr);

static struct error_callback sh4_error_callback;

void sh4_init(Sh4 *sh4) {
    memset(sh4, 0, sizeof(*sh4));
    sh4->reg_area = (uint8_t*)malloc(sizeof(uint8_t) * (SH4_P4_REGEND - SH4_P4_REGSTART));

#ifdef ENABLE_SH4_MMU
    sh4_mmu_init(sh4);
#endif

    sh4->cycles_accum = 0;
    memset(sh4->reg, 0, sizeof(sh4->reg));

    sh4_ocache_init(&sh4->ocache);

    sh4_tmu_init(sh4);

    sh4_scif_init(&sh4->scif);

    sh4_init_regs(sh4);

    sh4_compile_instructions(sh4);

    sh4_on_hard_reset(sh4);

    sh4_init_inst_lut();

    /*
     * TODO: in the future dynamically allocate the sh4_error_callback so I can
     * have one for each CPU (on multi-cpu systems like the hikaru)
     */
    sh4_error_callback.arg = sh4;
    sh4_error_callback.callback_fn = sh4_error_set_regs;
    error_add_callback(&sh4_error_callback);
}

void sh4_cleanup(Sh4 *sh4) {
    error_rm_callback(&sh4_error_callback);

    sh4_tmu_cleanup(sh4);

    sh4_ocache_cleanup(&sh4->ocache);

    free(sh4->reg_area);
}

void sh4_on_hard_reset(Sh4 *sh4) {
    memset(sh4->reg, 0, sizeof(sh4->reg));
    sh4_init_regs(sh4);
    sh4->reg[SH4_REG_SR] = SH4_SR_MD_MASK | SH4_SR_RB_MASK | SH4_SR_BL_MASK |
        SH4_SR_FD_MASK | SH4_SR_IMASK_MASK;
    sh4->reg[SH4_REG_VBR] = 0;
    sh4->reg[SH4_REG_PC] = 0xa0000000;

    sh4->fpu.fpscr = 0x41;

    unsigned idx;
    for (idx = 0; idx < SH4_N_FLOAT_REGS; idx++) {
        sh4->fpu.reg_bank0.fr[idx] = 0.0f;
        sh4->fpu.reg_bank1.fr[idx] = 0.0f;
    }

    sh4->delayed_branch = false;
    sh4->delayed_branch_addr = 0;

    /*
     * default to CO-type instructions so that the first instruction executed
     * costs a CPU cycle.
     */
    sh4->last_inst_type = SH4_GROUP_CO;

    sh4_ocache_clear(&sh4->ocache);

    sh4->exec_state = SH4_EXEC_STATE_NORM;
}

reg32_t sh4_get_pc(Sh4 *sh4) {
    return sh4->reg[SH4_REG_PC];
}

void sh4_get_regs(Sh4 *sh4, reg32_t reg_out[SH4_REGISTER_COUNT]) {
    memcpy(reg_out, sh4->reg, sizeof(reg_out[0]) * SH4_REGISTER_COUNT);
}

FpuReg sh4_get_fpu(Sh4 *sh4) {
    return sh4->fpu;
}

void sh4_set_regs(Sh4 *sh4, reg32_t const reg_in[SH4_REGISTER_COUNT]) {
    memcpy(sh4->reg, reg_in, sizeof(sh4->reg[0]) * SH4_REGISTER_COUNT);
}

void sh4_set_fpu(Sh4 *sh4, FpuReg src) {
    sh4->fpu = src;
}

void sh4_enter(Sh4 *sh4) {
    if (sh4->fpu.fpscr & SH4_FPSCR_RM_MASK)
        fesetround(FE_TOWARDZERO);
    else
        fesetround(FE_TONEAREST);
}

void sh4_set_fpscr(Sh4 *sh4, reg32_t new_val) {
    sh4->fpu.fpscr = new_val;
    if (sh4->fpu.fpscr & SH4_FPSCR_RM_MASK)
        fesetround(FE_TOWARDZERO);
    else
        fesetround(FE_TONEAREST);
}

void sh4_run_cycles(Sh4 *sh4, unsigned n_cycles) {
    inst_t inst;
    int exc_pending;

    n_cycles += sh4->cycles_accum;

mulligan:
    sh4_check_interrupts(sh4);
    do {
        if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
            if (exc_pending == MEM_ACCESS_EXC) {
                // TODO: some sort of logic to detect infinite loops here
                goto mulligan;
            } else {
                RAISE_ERROR(get_error_pending());
            }
        }

        InstOpcode const *op = sh4_inst_lut[inst];

        /*
         * The reason why this function subtracts sh4->cycles_accum both
         * times that can call dc_cycle_advance is that sh4->cycles_accum
         * would have been included in a previous call to dc_cycle_advance.
         */
        if (op->issue > n_cycles) {
            dc_cycle_advance(n_cycles - sh4->cycles_accum);
            sh4->cycles_accum = n_cycles;
            return;
        }

        n_cycles -= op->issue;
        if (sh4->cycles_accum >= op->issue) {
            sh4->cycles_accum -= op->issue;
        } else {
            dc_cycle_advance(op->issue - sh4->cycles_accum);
            sh4->cycles_accum = 0;
        }

        sh4_do_exec_inst(sh4, inst, op);

        if (op->group != SH4_GROUP_CO) {
            /*
             * fetch the next instruction and potentially execute it.
             * the rule is that CO can never execute in parallel with anything,
             * MT can execute in parallel with anything but CO, and every other
             * group can execute in parallel with anything but itself and CO.
             */

            /*
             * if there's an exception we'll deal with it next time this
             * function gets called
             */
            if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
                if (exc_pending == MEM_ACCESS_EXC)
                    goto mulligan;
                else
                    RAISE_ERROR(get_error_pending());
            }

            InstOpcode const *second_op = sh4_inst_lut[inst];

            if (second_op->group == SH4_GROUP_CO)
                continue;

            if ((op->group != second_op->group) ||
                (op->group == SH4_GROUP_MT)) {
                sh4_do_exec_inst(sh4, inst, second_op);
            }
        }
    } while (n_cycles);

    sh4->cycles_accum = 0;
}

/* executes a single instruction and maybe ticks the clock. */
void sh4_single_step(Sh4 *sh4) {
    inst_t inst;
    int exc_pending;

mulligan:
    sh4_check_interrupts(sh4);

    if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
        // TODO: some sort of logic to detect infinite loops here
        if (exc_pending == MEM_ACCESS_EXC)
            goto mulligan;
        else
            RAISE_ERROR(get_error_pending());
    }

    InstOpcode const *op = sh4_inst_lut[inst];

    dc_cycle_stamp_t tgt_stamp = dc_cycle_stamp();
    if ((op->group == SH4_GROUP_CO) ||
        (sh4->last_inst_type == SH4_GROUP_CO) ||
        ((sh4->last_inst_type != op->group) && (op->group != SH4_GROUP_MT))) {
        tgt_stamp += op->issue;
    }

    // I *wish* I could find a way to keep  this code in Dreamcast.cpp...
    SchedEvent *next_event;
    while ((next_event = peek_event()) &&
           (next_event->when <= tgt_stamp)) {
        pop_event();
        dc_cycle_advance(next_event->when - dc_cycle_stamp());
        next_event->handler(next_event);
    }

    sh4_do_exec_inst(sh4, inst, op);

    sh4->last_inst_type = op->group;

    dc_cycle_advance(tgt_stamp - dc_cycle_stamp());
}

void sh4_run_until(Sh4 *sh4, addr32_t stop_addr) {
    while (sh4->reg[SH4_REG_PC] != stop_addr)
        sh4_single_step(sh4);
}

void sh4_bank_switch(Sh4 *sh4) {
    reg32_t tmp[8];
    memcpy(tmp, sh4->reg + SH4_REG_R0, 8 * sizeof(reg32_t));
    memcpy(sh4->reg + SH4_REG_R0, sh4->reg + SH4_REG_R0_BANK, 8 * sizeof(reg32_t));
    memcpy(sh4->reg + SH4_REG_R0_BANK, tmp, 8 * sizeof(reg32_t));
}

void sh4_bank_switch_maybe(Sh4 *sh4, reg32_t old_sr, reg32_t new_sr) {
    if ((old_sr & SH4_SR_RB_MASK) != (new_sr & SH4_SR_RB_MASK))
        sh4_bank_switch(sh4);
}

static DEF_ERROR_U32_ATTR(sh4_reg_sr)
static DEF_ERROR_U32_ATTR(sh4_reg_ssr)
static DEF_ERROR_U32_ATTR(sh4_reg_pc)
static DEF_ERROR_U32_ATTR(sh4_reg_spc)
static DEF_ERROR_U32_ATTR(sh4_reg_gbr)
static DEF_ERROR_U32_ATTR(sh4_reg_vbr)
static DEF_ERROR_U32_ATTR(sh4_reg_sgr)
static DEF_ERROR_U32_ATTR(sh4_reg_dbr)
static DEF_ERROR_U32_ATTR(sh4_reg_mach)
static DEF_ERROR_U32_ATTR(sh4_reg_macl)
static DEF_ERROR_U32_ATTR(sh4_reg_pr)
static DEF_ERROR_U32_ATTR(sh4_reg_fpscr)
static DEF_ERROR_U32_ATTR(sh4_reg_fpul)
static DEF_ERROR_U32_ATTR(sh4_reg_r0_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r1_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r2_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r3_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r4_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r5_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r6_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r7_bank0);
static DEF_ERROR_U32_ATTR(sh4_reg_r0_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r1_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r2_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r3_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r4_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r5_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r6_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r7_bank1);
static DEF_ERROR_U32_ATTR(sh4_reg_r8);
static DEF_ERROR_U32_ATTR(sh4_reg_r9);
static DEF_ERROR_U32_ATTR(sh4_reg_r10);
static DEF_ERROR_U32_ATTR(sh4_reg_r11);
static DEF_ERROR_U32_ATTR(sh4_reg_r12);
static DEF_ERROR_U32_ATTR(sh4_reg_r13);
static DEF_ERROR_U32_ATTR(sh4_reg_r14);
static DEF_ERROR_U32_ATTR(sh4_reg_r15);
static DEF_ERROR_U32_ATTR(sh4_reg_ccr);
static DEF_ERROR_U32_ATTR(sh4_reg_qacr0);
static DEF_ERROR_U32_ATTR(sh4_reg_qacr1);
static DEF_ERROR_U32_ATTR(sh4_reg_pteh);
static DEF_ERROR_U32_ATTR(sh4_reg_ptel);
static DEF_ERROR_U32_ATTR(sh4_reg_ptea);
static DEF_ERROR_U32_ATTR(sh4_reg_ttb);
static DEF_ERROR_U32_ATTR(sh4_reg_tea);
static DEF_ERROR_U32_ATTR(sh4_reg_mmucr);

static void sh4_error_set_regs(void *argptr) {
    Sh4 *sh4 = (Sh4*)argptr;

    error_set_sh4_reg_sr(sh4->reg[SH4_REG_SR]);
    error_set_sh4_reg_ssr(sh4->reg[SH4_REG_SSR]);
    error_set_sh4_reg_pc(sh4->reg[SH4_REG_PC]);
    error_set_sh4_reg_spc(sh4->reg[SH4_REG_SPC]);
    error_set_sh4_reg_gbr(sh4->reg[SH4_REG_GBR]);
    error_set_sh4_reg_vbr(sh4->reg[SH4_REG_VBR]);
    error_set_sh4_reg_sgr(sh4->reg[SH4_REG_SGR]);
    error_set_sh4_reg_dbr(sh4->reg[SH4_REG_DBR]);
    error_set_sh4_reg_mach(sh4->reg[SH4_REG_MACH]);
    error_set_sh4_reg_macl(sh4->reg[SH4_REG_MACL]);
    error_set_sh4_reg_pr(sh4->reg[SH4_REG_PR]);
    error_set_sh4_reg_fpscr(sh4->fpu.fpscr);
    error_set_sh4_reg_fpul(sh4->fpu.fpul);
    error_set_sh4_reg_r0_bank0(*sh4_bank0_reg(sh4, 0));
    error_set_sh4_reg_r1_bank0(*sh4_bank0_reg(sh4, 1));
    error_set_sh4_reg_r2_bank0(*sh4_bank0_reg(sh4, 2));
    error_set_sh4_reg_r3_bank0(*sh4_bank0_reg(sh4, 3));
    error_set_sh4_reg_r4_bank0(*sh4_bank0_reg(sh4, 4));
    error_set_sh4_reg_r5_bank0(*sh4_bank0_reg(sh4, 5));
    error_set_sh4_reg_r6_bank0(*sh4_bank0_reg(sh4, 6));
    error_set_sh4_reg_r7_bank0(*sh4_bank0_reg(sh4, 7));
    error_set_sh4_reg_r0_bank1(*sh4_bank1_reg(sh4, 0));
    error_set_sh4_reg_r1_bank1(*sh4_bank1_reg(sh4, 1));
    error_set_sh4_reg_r2_bank1(*sh4_bank1_reg(sh4, 2));
    error_set_sh4_reg_r3_bank1(*sh4_bank1_reg(sh4, 3));
    error_set_sh4_reg_r4_bank1(*sh4_bank1_reg(sh4, 4));
    error_set_sh4_reg_r5_bank1(*sh4_bank1_reg(sh4, 5));
    error_set_sh4_reg_r6_bank1(*sh4_bank1_reg(sh4, 6));
    error_set_sh4_reg_r7_bank1(*sh4_bank1_reg(sh4, 7));
    error_set_sh4_reg_r8(sh4->reg[SH4_REG_R8]);
    error_set_sh4_reg_r9(sh4->reg[SH4_REG_R9]);
    error_set_sh4_reg_r10(sh4->reg[SH4_REG_R10]);
    error_set_sh4_reg_r11(sh4->reg[SH4_REG_R11]);
    error_set_sh4_reg_r12(sh4->reg[SH4_REG_R12]);
    error_set_sh4_reg_r13(sh4->reg[SH4_REG_R13]);
    error_set_sh4_reg_r14(sh4->reg[SH4_REG_R14]);
    error_set_sh4_reg_r15(sh4->reg[SH4_REG_R15]);
    error_set_sh4_reg_ccr(sh4->reg[SH4_REG_CCR]);
    error_set_sh4_reg_qacr0(sh4->reg[SH4_REG_QACR0]);
    error_set_sh4_reg_qacr1(sh4->reg[SH4_REG_QACR1]);
    error_set_sh4_reg_pteh(sh4->reg[SH4_REG_PTEH]);
    error_set_sh4_reg_ptel(sh4->reg[SH4_REG_PTEL]);
    error_set_sh4_reg_ptea(sh4->reg[SH4_REG_PTEA]);
    error_set_sh4_reg_ttb(sh4->reg[SH4_REG_TTB]);
    error_set_sh4_reg_tea(sh4->reg[SH4_REG_TEA]);
    error_set_sh4_reg_mmucr(sh4->reg[SH4_REG_MMUCR]);
}
