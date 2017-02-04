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

#include "sh4.hpp"

Sh4::Sh4() {
    reg_area = new uint8_t[P4_REGEND - P4_REGSTART];

#ifdef ENABLE_SH4_MMU
    sh4_mmu_init(this);
#endif

    memset(reg, 0, sizeof(reg));

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_init(&this->inst_cache);
#endif

#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_init(&this->op_cache);
#else
    this->oc_ram_area = new uint8_t[OC_RAM_AREA_SIZE];
#endif

    init_regs();

    sh4_compile_instructions();

    on_hard_reset();
}

Sh4::~Sh4() {
#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_cleanup(&op_cache);
#else
    delete[] oc_ram_area;
#endif

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_cleanup(&inst_cache);
#endif

    delete[] reg_area;
}

void Sh4::on_hard_reset() {
    reg[SH4_REG_SR] = SR_MD_MASK | SR_RB_MASK | SR_BL_MASK |
        SR_FD_MASK | SR_IMASK_MASK;
    reg[SH4_REG_VBR] = 0;
    reg[SH4_REG_PC] = 0xa0000000;

    fpu.fpscr = 0x41;

    std::fill(fpu.reg_bank0.fr, fpu.reg_bank0.fr + N_FLOAT_REGS, 0.0f);
    std::fill(fpu.reg_bank1.fr, fpu.reg_bank1.fr + N_FLOAT_REGS, 0.0f);

    delayed_branch = false;
    delayed_branch_addr = 0;

#ifdef ENABLE_SH4_OCACHE
    sh4_ocache_reset(&op_cache);
#else
    memset(oc_ram_area, 0, sizeof(uint8_t) * OC_RAM_AREA_SIZE);
#endif

#ifdef ENABLE_SH4_ICACHE
    sh4_icache_reset(&inst_cache);
#endif
}

reg32_t Sh4::get_pc() const {
    return reg[SH4_REG_PC];
}

void Sh4::get_regs(reg32_t reg_out[SH4_REGISTER_COUNT]) const {
    memcpy(reg_out, reg, sizeof(reg_out[0]) * SH4_REGISTER_COUNT);
}

Sh4::FpuReg Sh4::get_fpu() const {
    return fpu;
}

void Sh4::set_regs(reg32_t const reg_in[SH4_REGISTER_COUNT]) {
    memcpy(reg, reg_in, sizeof(reg[0]) * SH4_REGISTER_COUNT);
}

void Sh4::set_fpu(const Sh4::FpuReg& src) {
    this->fpu = src;
}

void Sh4::sh4_enter() {
    if (fpu.fpscr & FPSCR_RM_MASK)
        fesetround(FE_TOWARDZERO);
    else
        fesetround(FE_TONEAREST);
}

void Sh4::set_fpscr(reg32_t new_val) {
    fpu.fpscr = new_val;
    if (fpu.fpscr & FPSCR_RM_MASK)
        fesetround(FE_TOWARDZERO);
    else
        fesetround(FE_TONEAREST);
}
