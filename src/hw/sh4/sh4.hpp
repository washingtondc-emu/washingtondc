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

#ifndef SH4_HPP_
#define SH4_HPP_

#include <cassert>

#include <boost/cstdint.hpp>
#include <boost/static_assert.hpp>

#include "types.hpp"
#include "MemoryMap.hpp"
#include "sh4_inst.hpp"
#include "sh4_mmu.hpp"
#include "sh4_reg.hpp"
#include "sh4_mem.hpp"

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

/* Hitachi SuperH-4 interpreter */

class Sh4 {
public:
    reg32_t reg[SH4_REGISTER_COUNT];

#ifdef ENABLE_SH4_MMU
    struct sh4_mmu mmu;
#endif

    // true/false condition or carry/borrow bit
    static const unsigned SR_FLAG_T_SHIFT = 0;
    static const unsigned SR_FLAG_T_MASK = 1 << SR_FLAG_T_SHIFT;

    // saturation operation for MAC instructions
    static const unsigned SR_FLAG_S_SHIFT = 1;
    static const unsigned SR_FLAG_S_MASK = 1 << SR_FLAG_S_SHIFT;

    // interrupt mask level
    static const unsigned SR_IMASK_SHIFT = 4;
    static const unsigned SR_IMASK_MASK = 0xf << SR_IMASK_SHIFT;

    static const unsigned SR_Q_SHIFT = 8;
    static const unsigned SR_Q_MASK = 1 << SR_Q_SHIFT;

    static const unsigned SR_M_SHIFT = 9;
    static const unsigned SR_M_MASK = 1 << SR_M_SHIFT;

    // FPU disable bit
    static const unsigned SR_FD_SHIFT = 15;
    static const unsigned SR_FD_MASK = 1 << SR_FD_SHIFT;

    // IRQ mask (1 == masked)
    static const unsigned SR_BL_SHIFT = 28;
    static const unsigned SR_BL_MASK = 1 << SR_BL_SHIFT;

    // general register bank switch
    static const unsigned SR_RB_SHIFT = 29;
    static const unsigned SR_RB_MASK = 1 << SR_RB_SHIFT;

    // processor mode (0 = user, 1 = priveleged)
    static const unsigned SR_MD_SHIFT = 30;
    static const unsigned SR_MD_MASK = 1 << SR_MD_SHIFT;

    // floating-point rounding mode
    static const unsigned FPSCR_RM_SHIFT = 0;
    static const unsigned FPSCR_RM_MASK = 3 << FPSCR_RM_SHIFT;

    // FPU exception flags
    static const unsigned FPSCR_FLAG_SHIFT = 2;
    static const unsigned FPSCR_FLAG_MASK = 0x1f << FPSCR_FLAG_SHIFT;

    // FPU exception enable
    static const unsigned FPSCR_ENABLE_SHIFT = 7;
    static const unsigned FPSCR_ENABLE_MASK = 0x1f << FPSCR_FLAG_SHIFT;

    // FPU exception cause
    static const unsigned FPSCR_CAUSE_SHIFT = 12;
    static const unsigned FPSCR_CAUSE_MASK = 0x1f << FPSCR_CAUSE_SHIFT;

    // FPU Denormalization mode
    static const unsigned FPSCR_DN_SHIFT = 18;
    static const unsigned FPSCR_DN_MASK = 1 << FPSCR_DN_SHIFT;

    // FPU Precision mode
    static const unsigned FPSCR_PR_SHIFT = 19;
    static const unsigned FPSCR_PR_MASK = 1 << FPSCR_PR_SHIFT;

    // FPU Transfer size mode
    static const unsigned FPSCR_SZ_SHIFT = 20;
    static const unsigned FPSCR_SZ_MASK = 1 << FPSCR_SZ_SHIFT;

    // FPU bank switch
    static const unsigned FPSCR_FR_SHIFT = 21;
    static const unsigned FPSCR_FR_MASK = 1 << FPSCR_FR_SHIFT;

    /*
     * If the CPU is executing a delayed branch instruction, then
     * delayed_branch will be true and delayed_branch_addr will point to the
     * address to branch to.  After executing one instruction, delayed_branch
     * will be set to false and the CPU will jump to delayed_branch_addr.
     *
     * If the branch instruction evaluates to false (ie, there is not a delayed
     * branch) then delayed_branch will never be set to true.  This means that
     * the interpreter will not raise any exceptions caused by executing a
     * branch instruction in a delay slot; this is an inaccuracy which may need
     * to be revisited in the future.
     */
    bool delayed_branch;
    addr32_t delayed_branch_addr;

    static const size_t N_FLOAT_REGS = 16;
    static const size_t N_DOUBLE_REGS = 8;

    union FpuRegFile {
        float fr[N_FLOAT_REGS];
        double dr[N_DOUBLE_REGS];
    };
    BOOST_STATIC_ASSERT(sizeof(FpuRegFile) == (N_FLOAT_REGS * sizeof(float)));

    struct FpuReg {
        // floating point status/control register
        reg32_t fpscr;

        // floating-point communication register
        reg32_t fpul;

        FpuRegFile reg_bank0;
        FpuRegFile reg_bank1;
    };
    FpuReg fpu;

    /*
     * call this function instead of setting the value directly to make sure
     * that any state changes are immediately processed.
     */
    void set_fpscr(reg32_t new_val);

    /*
     * access single-precision floating-point register,
     * taking bank-switching into account
     */
    float *fpu_fr(unsigned reg_no) {
        assert(reg_no < N_FLOAT_REGS);

        if (fpu.fpscr & FPSCR_FR_MASK)
            return fpu.reg_bank1.fr + reg_no;
        return fpu.reg_bank0.fr + reg_no;
    }

    /*
     * access double-precision floating-point register,
     * taking bank-switching into account
     */
    double *fpu_dr(unsigned reg_no) {
        assert(reg_no < N_DOUBLE_REGS);

        if (fpu.fpscr & FPSCR_FR_MASK)
            return fpu.reg_bank1.dr + reg_no;
        return fpu.reg_bank0.dr + reg_no;
    }

    Sh4();
    ~Sh4();

    /*
     * This function should be called every time the emulator is about to
     * start emulating sh4 code after having emulated something else.
     * The purpose is to make sure that the host CPU's state is in sync with
     * the virtual sh4's state for operations which have some associated state.
     *
     * For example, one of the things  this function does is make sure that the
     * host CPU's floating-point rounding mode matches the FPSCR register's
     * RM bit.
     */
    void sh4_enter();

    // runs the next instruction, modifies CPU state and sets flags accordingly
    void exec_inst();

    inline void next_inst() {
        reg[SH4_REG_PC] += 2;
    }

    void do_exec_inst(inst_t inst);

    // runs inst as a delay slot.
    void exec_delay_slot(addr32_t addr);

    // returns the program counter
    reg32_t get_pc() const;

    // these four APIs are intended primarily for debuggers to use
    void get_regs(reg32_t reg_out[SH4_REGISTER_COUNT]) const;
    FpuReg get_fpu() const;
    void set_regs(reg32_t const reg_out[SH4_REGISTER_COUNT]);
    void set_fpu(const FpuReg& src);

    /*
     * if ((addr & OC_RAM_AREA_MASK) == OC_RAM_AREA_VAL) and the ORA bit is set
     * in CCR, then addr is part of the Operand Cache's RAM area
     */
    static const addr32_t OC_RAM_AREA_MASK = 0xfc000000;
    static const addr32_t OC_RAM_AREA_VAL = 0x7c000000;
    static inline bool in_oc_ram_area(addr32_t addr) {
        return (addr & OC_RAM_AREA_MASK) == OC_RAM_AREA_VAL;
    }

    /*
     * return the index of the given general-purpose register.
     * This function takes bank-switching into account.
     */
    sh4_reg_idx_t gen_reg_idx(int reg_no) {
        assert(!(reg_no & ~0xf));

        if (reg_no <= 7) {
            if (reg[SH4_REG_SR] & SR_RB_MASK)
                return (sh4_reg_idx_t)(SH4_REG_R0_BANK1 + reg_no);
            else
                return (sh4_reg_idx_t)(SH4_REG_R0_BANK0 + reg_no);
        } else {
            return (sh4_reg_idx_t)(SH4_REG_R8 + (reg_no - 8));
        }
    }

    /*
     * return a pointer to the given general-purpose register.
     * This function takes bank-switching into account.
     */
    reg32_t *gen_reg(int idx) {
        return reg + gen_reg_idx(idx);
    }

    /* return an index to the given banked general-purpose register */
    sh4_reg_idx_t bank_reg_idx(int idx) {
        assert(!(idx & ~0x7));

        if (reg[SH4_REG_SR] & SR_RB_MASK)
            return (sh4_reg_idx_t)(SH4_REG_R0_BANK0 + idx);
        else
            return (sh4_reg_idx_t)(SH4_REG_R0_BANK1 + idx);
    }

    // return a pointer to the given banked general-purpose register
    reg32_t *bank_reg(int idx) {
        return reg + bank_reg_idx(idx);
    }

    // IC index enable
    static const unsigned CCR_IIX_SHIFT = 15;
    static const unsigned CCR_IIX_MASK = 1 << CCR_IIX_SHIFT;

    // IC invalidation
    static const unsigned CCR_ICI_SHIFT = 11;
    static const unsigned CCR_ICI_MASK = 1 << CCR_ICI_SHIFT;

    // IC enable
    static const unsigned CCR_ICE_SHIFT = 8;
    static const unsigned CCR_ICE_MASK = 1 << CCR_ICE_SHIFT;

    // OC index enable
    static const unsigned CCR_OIX_SHIFT = 7;
    static const unsigned CCR_OIX_MASK = 1 << CCR_OIX_SHIFT;

    // OC RAM enable
    static const unsigned CCR_ORA_SHIFT = 5;
    static const unsigned CCR_ORA_MASK = 1 << CCR_ORA_SHIFT;

    // OC invalidation
    static const unsigned CCR_OCI_SHIFT = 3;
    static const unsigned CCR_OCI_MASK = 1 << CCR_OCI_SHIFT;

    // copy-back enable
    static const unsigned CCR_CB_SHIFT = 2;
    static const unsigned CCR_CB_MASK = 1 << CCR_CB_SHIFT;

    // Write-through
    static const unsigned CCR_WT_SHIFT = 1;
    static const unsigned CCR_WT_MASK = 1 << CCR_WT_SHIFT;

    // OC enable
    static const unsigned CCR_OCE_SHIFT = 0;
    static const unsigned CCR_OCE_MASK = 1 << CCR_OCE_SHIFT;

    // exception code in the expevt register
    static const unsigned EXPEVT_CODE_SHIFT = 0;
    static const unsigned EXPEVT_CODE_MASK = 0xfff << EXPEVT_CODE_SHIFT;

    // exception code in the intevt register
    static const unsigned INTEVT_CODE_SHIFT = 0;
    static const unsigned INTEVT_CODE_MASK = 0xfff << INTEVT_CODE_SHIFT;

    // immediate value in the tra register
    static const unsigned TRA_IMM_SHIFT = 2;
    static const unsigned TRA_IMM_MASK = 0xff << TRA_IMM_SHIFT;

#ifdef ENABLE_SH4_ICACHE
    struct Sh4Icache inst_cache;
#endif

#ifdef ENABLE_SH4_OCACHE
    struct Sh4Ocache op_cache;
#else
    /*
     * without an operand cache, we need to supply some other area
     * to serve as RAM when the ORA bit is enabled.
     */
    static const size_t LONGS_PER_OP_CACHE_LINE = 8;
    static const size_t OP_CACHE_LINE_SIZE = LONGS_PER_OP_CACHE_LINE * 4;
    static const size_t OC_RAM_AREA_SIZE = 8 * 1024;
    uint8_t *oc_ram_area;
#endif

    // reset all values to their power-on-reset values
    void on_hard_reset();

    /*
     * pointer to place where memory-mapped registers are stored.
     * RegReadHandlers and RegWriteHandlers do not need to use this as long as
     * they are consistent.
     */
    uint8_t *reg_area;
};

#endif
