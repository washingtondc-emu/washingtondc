/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include "sh4_excp.h"
#include "sh4_reg.h"
#include "sh4_inst.h"
#include "sh4_mem.h"
#include "washdc/error.h"
#include "dreamcast.h"

#include "sh4.h"

static void sh4_error_set_regs(void *argptr);

static struct error_callback sh4_error_callback;

void sh4_init(Sh4 *sh4, struct dc_clock *clk) {
    memset(sh4, 0, sizeof(*sh4));
    sh4->reg_area = (uint8_t*)malloc(sizeof(uint8_t) * (SH4_P4_REGEND - SH4_P4_REGSTART));
    sh4->clk = clk;

    memset(sh4->reg, 0, sizeof(sh4->reg));

    sh4_mem_init(sh4);

    sh4_ocache_init(&sh4->ocache);

    sh4_tmu_init(sh4);

    sh4_scif_init(&sh4->scif);

    sh4_init_regs(sh4);

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

    sh4_mem_cleanup(sh4);

    free(sh4->reg_area);
}

void sh4_on_hard_reset(Sh4 *sh4) {
    memset(sh4->reg, 0, sizeof(sh4->reg));
    sh4_init_regs(sh4);
    sh4->reg[SH4_REG_SR] = SH4_SR_MD_MASK | SH4_SR_RB_MASK | SH4_SR_BL_MASK |
        SH4_SR_FD_MASK | SH4_SR_IMASK_MASK;
    sh4->reg[SH4_REG_VBR] = 0;
    sh4->reg[SH4_REG_PC] = 0xa0000000;

    sh4_set_fpscr(sh4, 0x41);

    unsigned idx;
    for (idx = 0; idx < SH4_N_FLOAT_REGS; idx++) {
        *sh4_fpu_fr(sh4, idx) = 0.0f;
        *sh4_fpu_xf(sh4, idx) = 0.0f;
    }

    sh4->delayed_branch = false;
    sh4->delayed_branch_addr = 0;

    /*
     * default to CO-type instructions so that the first instruction executed
     * costs a CPU cycle.
     */
    sh4->last_inst_type = SH4_GROUP_NONE;

    sh4_ocache_clear(&sh4->ocache);

    sh4->exec_state = SH4_EXEC_STATE_NORM;
}

reg32_t sh4_get_pc(Sh4 *sh4) {
    return sh4->reg[SH4_REG_PC];
}

void sh4_get_regs(Sh4 *sh4, reg32_t reg_out[SH4_REGISTER_COUNT]) {
    memcpy(reg_out, sh4->reg, sizeof(reg_out[0]) * SH4_REGISTER_COUNT);
}

void sh4_set_regs(Sh4 *sh4, reg32_t const reg_in[SH4_REGISTER_COUNT]) {
    unsigned reg_no;

    /*
     * handle sr and fpscr first as a special case because
     * they can cause bank-switching
     */
    sh4_set_individual_reg(sh4, SH4_REG_SR, reg_in[SH4_REG_SR]);
    sh4_set_individual_reg(sh4, SH4_REG_FPSCR, reg_in[SH4_REG_FPSCR]);

    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++) {
        if (reg_no != SH4_REG_SR && reg_no != SH4_REG_FPSCR)
            sh4_set_individual_reg(sh4, reg_no, reg_in[reg_no]);
    }
}

void sh4_set_individual_reg(Sh4 *sh4, unsigned reg_no, reg32_t reg_val) {
    if (reg_no == SH4_REG_FPSCR) {
        sh4_set_fpscr(sh4, reg_val);
    } else if (reg_no == SH4_REG_SR) {
        reg32_t old_sr_val = sh4->reg[SH4_REG_SR];
        sh4->reg[SH4_REG_SR] = reg_val;
        sh4_on_sr_change(sh4, old_sr_val);
    } else {
        sh4->reg[reg_no] = reg_val;
    }
}

// this function should be called every time sr is written to
void sh4_on_sr_change(Sh4 *sh4, reg32_t old_sr) {
    reg32_t new_sr = sh4->reg[SH4_REG_SR];
    sh4_bank_switch_maybe(sh4, old_sr, new_sr);

    if ((old_sr & SH4_INTC_SR_BITS) != (new_sr & SH4_INTC_SR_BITS))
        sh4_refresh_intc_deferred(sh4);

    if (!(new_sr & SH4_SR_MD_MASK)) {
        error_set_feature("unprivileged mode");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void sh4_set_fpscr(Sh4 *sh4, reg32_t new_val) {
    /*
     * XXX This function allows the FPU rounding mode to "bleed" out of the SH4's
     * state and effect any other component that needs to use the FPU.
     *
     * Ideally we'd be maintaining a separate FPU context for the CPU, but in
     * practice calling fesetenv/fegetenv every time WashingtonDC enters/leaves
     * SH4 code incurs a massive performance penalty (greater than 50%).  This
     * might be because of branching or it might be because fegetenv/fesetenv
     * are really slow or it might just be the result of calling the same
     * function very often.  Either way, there's way too much overhead.
     *
     * I'm expecting that the only "real" victims of this will be PVR2 and
     * *maybe* AICA.  Generally speaking, there isn't much feedback from
     * graphics/sound into the game state, so this shouldn't cause anything
     * worse than very slightly glitched graphics/sound.
     */

    sh4_fpu_bank_switch_maybe(sh4, sh4->reg[SH4_REG_FPSCR], new_val);

    sh4->reg[SH4_REG_FPSCR] = new_val;
    if (sh4->reg[SH4_REG_FPSCR] & SH4_FPSCR_RM_MASK)
        fesetround(FE_TOWARDZERO);
    else
        fesetround(FE_TONEAREST);
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

void sh4_fpu_bank_switch(Sh4 *sh4) {
    uint32_t tmp[SH4_N_FLOAT_REGS];
    memcpy(tmp, sh4->reg + SH4_REG_FR0, sizeof(tmp));
    memcpy(sh4->reg + SH4_REG_FR0, sh4->reg + SH4_REG_XF0, sizeof(tmp));
    memcpy(sh4->reg + SH4_REG_XF0, tmp, sizeof(tmp));
}

void sh4_fpu_bank_switch_maybe(Sh4 *sh4, reg32_t old_fpscr, reg32_t new_fpscr) {
    if ((old_fpscr & SH4_FPSCR_FR_MASK) != (new_fpscr & SH4_FPSCR_FR_MASK))
        sh4_fpu_bank_switch(sh4);
}

dc_cycle_stamp_t sh4_get_cycles(struct Sh4 *sh4) {
    return clock_cycle_stamp(sh4->clk) / SH4_CLOCK_SCALE;
}

uint32_t sh4_pc_next(struct Sh4 *sh4) {
    return sh4->reg[SH4_REG_PC];
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
    error_set_sh4_reg_fpscr(sh4->reg[SH4_REG_FPSCR]);
    error_set_sh4_reg_fpul(sh4->reg[SH4_REG_FPUL]);
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
