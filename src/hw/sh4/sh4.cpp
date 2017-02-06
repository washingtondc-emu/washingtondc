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

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#include "BaseException.hpp"
#include "sh4_mmu.hpp"
#include "sh4_excp.hpp"
#include "sh4_reg.hpp"
#include "sh4.hpp"

void sh4_init(Sh4 *sh4) {
    sh4->reg_area = new uint8_t[SH4_P4_REGEND - SH4_P4_REGSTART];

#ifdef ENABLE_SH4_MMU
    sh4_mmu_init(sh4);
#endif

    memset(sh4->reg, 0, sizeof(sh4->reg));

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_init(&sh4->inst_cache);
#endif

#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_init(&sh4->op_cache);
#else
    sh4->oc_ram_area = new uint8_t[SH4_OC_RAM_AREA_SIZE];
#endif

    sh4_init_regs(sh4);

    sh4_compile_instructions();

    sh4_on_hard_reset(sh4);
}

void sh4_cleanup(Sh4 *sh4) {
#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_cleanup(&sh4->op_cache);
#else
    delete[] sh4->oc_ram_area;
#endif

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_cleanup(&sh4->inst_cache);
#endif

    delete[] sh4->reg_area;
}

void sh4_on_hard_reset(Sh4 *sh4) {
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

#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_reset(&sh4->op_cache);
#else
    memset(sh4->oc_ram_area, 0, sizeof(uint8_t) * SH4_OC_RAM_AREA_SIZE);
#endif

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_reset(&sh4->inst_cache);
#endif
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
