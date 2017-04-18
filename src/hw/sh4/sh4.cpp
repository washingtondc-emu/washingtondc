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

#include <cstring>

#include "hw/pvr2/spg.hpp"
#include "BaseException.hpp"
#include "Dreamcast.hpp"
#include "sh4_mmu.hpp"
#include "sh4_excp.hpp"
#include "sh4_reg.hpp"
#include "sh4.hpp"

// struct RegFile
typedef boost::error_info<struct tag_sr_error_info, reg32_t> errinfo_reg_sr;
typedef boost::error_info<struct tag_ssr_error_info, reg32_t> errinfo_reg_ssr;
typedef boost::error_info<struct tag_pc_error_info, reg32_t> errinfo_reg_pc;
typedef boost::error_info<struct tag_spc_error_info, reg32_t> errinfo_reg_spc;
typedef boost::error_info<struct tag_gbr_error_info, reg32_t> errinfo_reg_gbr;
typedef boost::error_info<struct tag_vbr_error_info, reg32_t> errinfo_reg_vbr;
typedef boost::error_info<struct tag_sgr_error_info, reg32_t> errinfo_reg_sgr;
typedef boost::error_info<struct tag_dbr_error_info, reg32_t> errinfo_reg_dbr;
typedef boost::error_info<struct tag_mach_error_info, reg32_t> errinfo_reg_mach;
typedef boost::error_info<struct tag_macl_error_info, reg32_t> errinfo_reg_macl;
typedef boost::error_info<struct tag_pr_error_info, reg32_t> errinfo_reg_pr;
typedef boost::error_info<struct tag_fpscr_error_info, reg32_t>
errinfo_reg_fpscr;
typedef boost::error_info<struct tag_fpul_error_info, reg32_t> errinfo_reg_fpul;

// general-purpose registers within struct RegFile
typedef boost::tuple<reg32_t, reg32_t, reg32_t, reg32_t,
                     reg32_t, reg32_t, reg32_t, reg32_t> RegBankTuple;
typedef boost::error_info<struct tag_bank0_error_info, RegBankTuple> errinfo_reg_bank0;
typedef boost::error_info<struct tag_bank1_error_info, RegBankTuple> errinfo_reg_bank1;
typedef boost::error_info<struct tag_rgen_error_info, RegBankTuple> errinfo_reg_rgen;

// struct CacheReg
typedef boost::error_info<struct tag_ccr_error_info, reg32_t> errinfo_reg_ccr;
typedef boost::error_info<struct tag_qacr0_error_info, reg32_t>
errinfo_reg_qacr0;
typedef boost::error_info<struct tag_qacr1_error_info, reg32_t>
errinfo_reg_qacr1;

// struct Mmu
typedef boost::error_info<struct tag_pteh_error_info, reg32_t> errinfo_reg_pteh;
typedef boost::error_info<struct tag_ptel_error_info, reg32_t> errinfo_reg_ptel;
typedef boost::error_info<struct tag_ptea_error_info, reg32_t> errinfo_reg_ptea;
typedef boost::error_info<struct tag_ttb_error_info, reg32_t> errinfo_reg_ttb;
typedef boost::error_info<struct tag_tea_error_info, reg32_t> errinfo_reg_tea;
typedef boost::error_info<struct tag_mmucr_error_info, reg32_t>
errinfo_reg_mmucr;

void sh4_init(Sh4 *sh4) {
    memset(sh4, 0, sizeof(*sh4));
    sh4->reg_area = new uint8_t[SH4_P4_REGEND - SH4_P4_REGSTART];

#ifdef ENABLE_SH4_MMU
    sh4_mmu_init(sh4);
#endif

    sh4->cycles_accum = 0;
    memset(sh4->reg, 0, sizeof(sh4->reg));

    sh4_ocache_init(&sh4->ocache);

    sh4_tmu_init(sh4);

    sh4_scif_init(&sh4->scif);

    sh4_init_regs(sh4);

    sh4_compile_instructions();

    sh4_on_hard_reset(sh4);
}

void sh4_cleanup(Sh4 *sh4) {
    sh4_tmu_cleanup(sh4);

    sh4_ocache_cleanup(&sh4->ocache);

    delete[] sh4->reg_area;
}

void sh4_on_hard_reset(Sh4 *sh4) {
    memset(sh4->reg, 0, sizeof(sh4->reg));
    sh4_init_regs(sh4);
    sh4->reg[SH4_REG_SR] = SH4_SR_MD_MASK | SH4_SR_RB_MASK | SH4_SR_BL_MASK |
        SH4_SR_FD_MASK | SH4_SR_IMASK_MASK;
    sh4->reg[SH4_REG_VBR] = 0;
    sh4->reg[SH4_REG_PC] = 0xa0000000;

    sh4->fpu.fpscr = 0x41;

    std::fill(sh4->fpu.reg_bank0.fr, sh4->fpu.reg_bank0.fr + SH4_N_FLOAT_REGS,
              0.0f);
    std::fill(sh4->fpu.reg_bank1.fr, sh4->fpu.reg_bank1.fr + SH4_N_FLOAT_REGS,
              0.0f);

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

Sh4::FpuReg sh4_get_fpu(Sh4 *sh4) {
    return sh4->fpu;
}

void sh4_set_regs(Sh4 *sh4, reg32_t const reg_in[SH4_REGISTER_COUNT]) {
    memcpy(sh4->reg, reg_in, sizeof(sh4->reg[0]) * SH4_REGISTER_COUNT);
}

void sh4_set_fpu(Sh4 *sh4, const Sh4::FpuReg& src) {
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
    try {
        sh4_check_interrupts(sh4);
        do {
            if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
                // TODO: some sort of logic to detect infinite loops here
                goto mulligan;
            }

            InstOpcode const *op = sh4_decode_inst(sh4, inst);

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
                if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC])))
                    goto mulligan;

                InstOpcode const *second_op = sh4_decode_inst(sh4, inst);

                if (second_op->group == SH4_GROUP_CO)
                    continue;

                if ((op->group != second_op->group) ||
                    (op->group == SH4_GROUP_MT)) {
                    sh4_do_exec_inst(sh4, inst, second_op);
                }
            }
        } while (n_cycles);
    } catch (BaseException& exc) {
        sh4_add_regs_to_exc(sh4, exc);
        sh4->cycles_accum = 0;
        throw;
    }

    sh4->cycles_accum = 0;
}

/* executes a single instruction and maybe ticks the clock. */
void sh4_single_step(Sh4 *sh4) {
    inst_t inst;
    int exc_pending;

mulligan:
    try {
        sh4_check_interrupts(sh4);

        if ((exc_pending = sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]))) {
            // TODO: some sort of logic to detect infinite loops here
            goto mulligan;
        }

        InstOpcode const *op = sh4_decode_inst(sh4, inst);

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
    } catch (BaseException& exc) {
        sh4_add_regs_to_exc(sh4, exc);
        exc << errinfo_cycle_stamp(dc_cycle_stamp());
        throw;
    }
}

void sh4_run_until(Sh4 *sh4, addr32_t stop_addr) {
    while (sh4->reg[SH4_REG_PC] != stop_addr)
        sh4_single_step(sh4);
}

void sh4_add_regs_to_exc(Sh4 *sh4, BaseException& exc) {
    exc << errinfo_reg_sr(sh4->reg[SH4_REG_SR]);
    exc << errinfo_reg_ssr(sh4->reg[SH4_REG_SSR]);
    exc << errinfo_reg_pc(sh4->reg[SH4_REG_PC]);
    exc << errinfo_reg_spc(sh4->reg[SH4_REG_SPC]);
    exc << errinfo_reg_gbr(sh4->reg[SH4_REG_GBR]);
    exc << errinfo_reg_vbr(sh4->reg[SH4_REG_VBR]);
    exc << errinfo_reg_sgr(sh4->reg[SH4_REG_SGR]);
    exc << errinfo_reg_dbr(sh4->reg[SH4_REG_DBR]);
    exc << errinfo_reg_mach(sh4->reg[SH4_REG_MACH]);
    exc << errinfo_reg_macl(sh4->reg[SH4_REG_MACL]);
    exc << errinfo_reg_pr(sh4->reg[SH4_REG_PR]);
    exc << errinfo_reg_fpscr(sh4->fpu.fpscr);
    exc << errinfo_reg_fpul(sh4->fpu.fpul);
    exc << errinfo_reg_bank0(RegBankTuple(sh4->reg[SH4_REG_R0_BANK0],
                                          sh4->reg[SH4_REG_R1_BANK0],
                                          sh4->reg[SH4_REG_R2_BANK0],
                                          sh4->reg[SH4_REG_R3_BANK0],
                                          sh4->reg[SH4_REG_R4_BANK0],
                                          sh4->reg[SH4_REG_R5_BANK0],
                                          sh4->reg[SH4_REG_R6_BANK0],
                                          sh4->reg[SH4_REG_R7_BANK0]));
    exc << errinfo_reg_bank1(RegBankTuple(sh4->reg[SH4_REG_R0_BANK1],
                                          sh4->reg[SH4_REG_R1_BANK1],
                                          sh4->reg[SH4_REG_R2_BANK1],
                                          sh4->reg[SH4_REG_R3_BANK1],
                                          sh4->reg[SH4_REG_R4_BANK1],
                                          sh4->reg[SH4_REG_R5_BANK1],
                                          sh4->reg[SH4_REG_R6_BANK1],
                                          sh4->reg[SH4_REG_R7_BANK1]));
    exc << errinfo_reg_rgen(RegBankTuple(sh4->reg[SH4_REG_R8],
                                         sh4->reg[SH4_REG_R9],
                                         sh4->reg[SH4_REG_R10],
                                         sh4->reg[SH4_REG_R11],
                                         sh4->reg[SH4_REG_R12],
                                         sh4->reg[SH4_REG_R13],
                                         sh4->reg[SH4_REG_R14],
                                         sh4->reg[SH4_REG_R15]));
    exc << errinfo_reg_ccr(sh4->reg[SH4_REG_CCR]);
    exc << errinfo_reg_qacr0(sh4->reg[SH4_REG_QACR0]);
    exc << errinfo_reg_qacr1(sh4->reg[SH4_REG_QACR1]);

    // struct Mmu
    exc << errinfo_reg_pteh(sh4->reg[SH4_REG_PTEH]);
    exc << errinfo_reg_ptel(sh4->reg[SH4_REG_PTEL]);
    exc << errinfo_reg_ptea(sh4->reg[SH4_REG_PTEA]);
    exc << errinfo_reg_ttb(sh4->reg[SH4_REG_TTB]);
    exc << errinfo_reg_tea(sh4->reg[SH4_REG_TEA]);
    exc << errinfo_reg_mmucr(sh4->reg[SH4_REG_MMUCR]);
}
