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

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

/* Hitachi SuperH-4 interpreter */

class Sh4 {
    /*
     * This is kinda lame, but I have to declare all the testing classes as
     * friends so they can access the sh4's private members.  I consider this to
     * be unfortunate, but still better than writing a get/set method for
     * everything in this class
     */
    template<typename ValType, class Generator>
    friend class BasicMemTest;

    template<typename ValType, class Generator>
    friend class BasicMemTestWithIndexEnable;

    template <typename ValType, class Generator>
    friend class MmuUtlbMissTest;

    friend class Sh4InstTests;

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

    typedef struct TmuChannel {
        uint32_t tcor; // Timer Constant Register
        uint32_t tcnt; // Timer Counter
        uint16_t tcr;  // Timer control register
    } TmuChannel;

    struct TmuReg {
        uint8_t tocr; // Timer Output Control Register
        uint8_t tstr; // Timer Start Register

        TmuChannel channels[3];

        uint32_t tcpr2; // input capture register (Channel 2 only?)
    };
    TmuReg tmu;

    void setTmuTocr(uint8_t new_val);
    uint8_t getTmuTocr();
    void setTmuTstr(uint8_t new_val);
    uint8_t getTmuTstr();
    void setTmuTcor(unsigned chan, uint32_t new_val);
    uint32_t getTmuTcor(unsigned chan);
    void setTmuTcnt(unsigned chan, uint32_t new_val);
    uint32_t getTmuTcnt(unsigned chan);
    void setTmuTcr(unsigned chan, uint16_t new_val);
    uint32_t getTmuTcr(unsigned chan);
    void setTmuTcpr2(uint32_t new_val);
    uint32_t getTmuTcpr2();

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

    void set_exception(unsigned excp_code);
    void set_interrupt(unsigned intp_code);

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

    template<typename val_t>
    int write_mem(val_t const *val, addr32_t addr, unsigned len) {
        return do_write_mem(val, addr, len);
    }

    template<typename val_t>
    int read_mem(val_t *val, addr32_t addr, unsigned len) {
        int err;
        if ((err = do_read_mem(val, addr, len)) != 0)
            return err;
        return 0;
    }

    enum ExceptionCode {
        // reset-type exceptions
        EXCP_POWER_ON_RESET           = 0x000,
        EXCP_MANUAL_RESET             = 0x020,
        EXCP_HUDI_RESET               = 0x000,
        EXCP_INST_TLB_MULT_HIT        = 0x140,
        EXCP_DATA_TLB_MULT_HIT        = 0x140,

        // general exceptions (re-execution type)
        EXCP_USER_BREAK_BEFORE        = 0x1e0,
        EXCP_INST_ADDR_ERR            = 0x0e0,
        EXCP_INST_TLB_MISS            = 0x040,
        EXCP_INST_TLB_PROT_VIOL       = 0x0a0,
        EXCP_GEN_ILLEGAL_INST         = 0x180,
        EXCP_SLOT_ILLEGAL_INST        = 0x1a0,
        EXCP_GEN_FPU_DISABLE          = 0x800,
        EXCP_SLOT_FPU_DISABLE         = 0x820,
        EXCP_DATA_ADDR_READ           = 0x0e0,
        EXCP_DATA_ADDR_WRITE          = 0x100,
        EXCP_DATA_TLB_READ_MISS       = 0x040,
        EXCP_DATA_TLB_WRITE_MISS      = 0x060,
        EXCP_DATA_TLB_READ_PROT_VIOL  = 0x0a0,
        EXCP_DATA_TLB_WRITE_PROT_VIOL = 0x0c0,
        EXCP_FPU                      = 0x120,
        EXCP_INITIAL_PAGE_WRITE       = 0x080,

        // general exceptions (completion type)
        EXCP_UNCONDITIONAL_TRAP       = 0x160,
        EXCP_USER_BREAK_AFTER         = 0x1e0,

        // interrupt (completion type)
        EXCP_NMI                      = 0x1c0,
        EXCP_EXT_0                    = 0x200,
        EXCP_EXT_1                    = 0x220,
        EXCP_EXT_2                    = 0x240,
        EXCP_EXT_3                    = 0x260,
        EXCP_EXT_4                    = 0x280,
        EXCP_EXT_5                    = 0x2a0,
        EXCP_EXT_6                    = 0x2c0,
        EXCP_EXT_7                    = 0x2e0,
        EXCP_EXT_8                    = 0x300,
        EXCP_EXT_9                    = 0x320,
        EXCP_EXT_A                    = 0x340,
        EXCP_EXT_B                    = 0x360,
        EXCP_EXT_C                    = 0x380,
        EXCP_EXT_D                    = 0x3a0,
        EXCP_EXT_E                    = 0x3c0,

        //peripheral module interrupts (completion type)
        EXCP_TMU0_TUNI0               = 0x400,
        EXCP_TMU1_TUNI1               = 0x420,
        EXCP_TMU2_TUNI2               = 0x440,
        EXCP_TMU2_TICPI2              = 0x460,
        EXCP_RTC_ATI                  = 0x480,
        EXCP_RTC_PRI                  = 0x4a0,
        EXCP_RTC_CUI                  = 0x4c0,
        EXCP_SCI_ERI                  = 0x4e0,
        EXCP_SCI_RXI                  = 0x500,
        EXCP_SCI_TXI                  = 0x520,
        EXCP_SCI_TEI                  = 0x540,
        EXCP_WDT_ITI                  = 0x560,
        EXCP_REF_RCMI                 = 0x580,
        EXCP_REF_ROVI                 = 0x5a0,
        EXCP_HUDI_HUDI                = 0x600,
        EXCP_GPIO_GPIOI               = 0x620,

        // Peripheral module interrupt
        EXCP_DMAC_DMTE0               = 0x640,
        EXCP_DMAC_DMTE1               = 0x660,
        EXCP_DMAC_DMTE2               = 0x680,
        EXCP_DMAC_DMTE3               = 0x6a0,
        EXCP_DMAC_DMAE                = 0x6c0,
        EXCP_SCIF_ERI                 = 0x700,
        EXCP_SCIF_RXI                 = 0x720,
        EXCP_SCIF_BRI                 = 0x740,
        EXCP_SCIF_TXI                 = 0x760
    };

    const static unsigned EXCP_COUNT = 9 + 16 + 16 + 2 + 16 + 5;

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

    struct CacheReg {
        // Cache control register
        reg32_t ccr;

        // Queue address control register 0
        reg32_t qacr0;

        // Queue address control register 1
        reg32_t qacr1;
    } cache_reg;

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

    /*
     * read to/write from the operand cache's RAM-space in situations where we
     * don't actually have a real operand cache available.  It is up to the
     * caller to make sure that the operand cache is enabled (OCE in the CCR),
     * that the Operand Cache's RAM switch is enabled (ORA in the CCR) and that
     * paddr lies within the Operand Cache RAM mapping (in_oc_ram_area returns
     * true).
     */
    void do_write_ora(void const *dat, addr32_t paddr, unsigned len);
    void do_read_ora(void *dat, addr32_t paddr, unsigned len);

    void *get_ora_ram_addr(addr32_t paddr);
#endif

private:

    // Physical memory aread boundaries
    static const size_t AREA_P0_FIRST = 0x00000000;
    static const size_t AREA_P0_LAST  = 0x7fffffff;
    static const size_t AREA_P1_FIRST = 0x80000000;
    static const size_t AREA_P1_LAST  = 0x9fffffff;
    static const size_t AREA_P2_FIRST = 0xa0000000;
    static const size_t AREA_P2_LAST  = 0xbfffffff;
    static const size_t AREA_P3_FIRST = 0xc0000000;
    static const size_t AREA_P3_LAST  = 0xdfffffff;
    static const size_t AREA_P4_FIRST = 0xe0000000;
    static const size_t AREA_P4_LAST  = 0xffffffff;

    /*
     * P4_REGSTART is the addr of the first memory-mapped
     *     register in area 7
     * P4_REGEND is the first addr *after* the last memory-mapped
     *     register in the p4 area.
     * AREA7_REGSTART is the addr of the first memory-mapped
     *     register in area 7
     * AREA7_REGEND is the first addr *after* the last memory-mapped
     *     register in area 7
     */
    static const size_t P4_REGSTART = 0xff000000;
    static const size_t P4_REGEND = 0xfff00008;
    static const size_t AREA7_REGSTART = 0x1f000000;
    static const size_t AREA7_REGEND = 0x1ff00008;
    BOOST_STATIC_ASSERT((P4_REGEND - P4_REGSTART) == (AREA7_REGEND - AREA7_REGSTART));

    // reset all values to their power-on-reset values
    void on_hard_reset();

    enum VirtMemArea {
        AREA_P0 = 0,
        AREA_P1,
        AREA_P2,
        AREA_P3,
        AREA_P4
    };

    enum VirtMemArea get_mem_area(addr32_t addr);

    /*
     * From within the CPU, these functions should be called instead of
     * the memory's read/write functions because these implement the MMU
     * functionality.  In the event of a failure, these functions will set the
     * appropriate CPU flags for an exception and return non-zero.  On success
     * they will return zero.
     */
    int do_write_mem(void const *dat, addr32_t addr, unsigned len);
    int do_read_mem(void *dat, addr32_t addr, unsigned len);

    int read_inst(inst_t *out, addr32_t addr);

    /*
     * generally you'll call these functions through do_read_mem/do_write_mem
     * instead of calling these functions directly
     */
    int do_read_p4(void *dat, addr32_t addr, unsigned len);
    int do_write_p4(void const *dat, addr32_t addr, unsigned len);

    struct ExcpMeta {
        /*
         * there's no field for the vector base address because I couldn't
         * figure out an elegant way to express that (since it can be either a
         * constant or a register) and also because it's pretty easy to
         * hardcode this into enter_exception (since there's only one constant
         * and two registers that can be used)
         */

        enum ExceptionCode code;
        int prio_level;
        int prio_order;
        addr32_t offset;
    };

    static const struct ExcpMeta excp_meta[EXCP_COUNT];

    struct ExceptionReg {
        // TRAPA immediate data     - 0xff000020
        reg32_t tra;

        // exception event register - 0xff000024
        reg32_t expevt;

        // interrupt event register - 0xff000028
        reg32_t intevt;
    } excp_reg;

    /*
     * called by set_exception and set_interrupt.  This function configures
     * the CPU registers to enter an exception state.
     */
    void enter_exception(enum ExceptionCode vector);

    /*
     * pointer to place where memory-mapped registers are stored.
     * RegReadHandlers and RegWriteHandlers do not need to use this as long as
     * they are consistent.
     */
    uint8_t *reg_area;

    /*
     * for the purpose of these handlers, you may assume that the caller has
     * already checked the permissions.
     */
    struct MemMappedReg;
    typedef int(Sh4::*RegReadHandler)(void *buf,
                                      struct Sh4::MemMappedReg const *reg_info);
    typedef int(Sh4::*RegWriteHandler)(void const *buf,
                                       struct Sh4::MemMappedReg const *reg_info);

    /*
     * TODO: turn this into a radix tree of some sort.
     *
     * Alternatively, I could turn this into a simple lookup array; this
     * would incur a huge memory overhead (hundreds of MB), but it looks like
     * it would be feasible in the $CURRENT_YEAR and it would net a
     * beautiful O(1) mapping from addr32_t to MemMappedReg.
     */
    static struct MemMappedReg {
        char const *reg_name;

        /*
         * Some registers can be referenced over a range of addresses.
         * To check for equality between this register and a given physical
         * address, AND the address with addr_mask and then check for equality
         * with addr
         */
        addr32_t addr;  // addr shoud be the p4 addr, not the area7 addr
        addr32_t addr_mask;

        unsigned len;

        /* index of the register in the register file */
        sh4_reg_idx_t reg_idx;

        /*
         * if true, the value will be preserved during a manual ("soft") reset
         * and manual_reset_val will be ignored; else value will be set to
         * manual_reset_val during a manual reset.
         */
        bool hold_on_reset;

        Sh4::RegReadHandler on_p4_read;
        Sh4::RegWriteHandler on_p4_write;

        /*
         * if len < 4, then only the lower "len" bytes of
         * these values will be used.
         */
        reg32_t poweron_reset_val;
        reg32_t manual_reset_val;
    } mem_mapped_regs[];

    struct MemMappedReg *find_reg_by_addr(addr32_t addr);

    /* read/write handler callbacks for when you don't give a fuck */
    int IgnoreRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int IgnoreRegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info);

    /* default reg reg/write handler callbacks */
    int DefaultRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int DefaultRegWriteHandler(void const *buf,
                               struct MemMappedReg const *reg_info);

    /*
     * read handle callback that always fails (although currently it throws an
     * UnimplementedError because I don't know what the proper response is when
     * the software tries to read from an unreadable register).
     *
     * This is used for certain registers which are write-only.
     */
    int WriteOnlyRegReadHandler(void *buf,
                                struct MemMappedReg const *reg_info);

    /*
     * likewise, this is a write handler for read-only registers.
     * It will also raise an exception whenever it is invokled.
     */
    int ReadOnlyRegWriteHandler(void const *buf,
                                struct MemMappedReg const *reg_info);

    int MmucrRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int MmucrRegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int CcrRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int CcrRegWriteHandler(void const *buf,
                           struct MemMappedReg const *reg_info);

    int TraRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int TraRegWriteHandler(void const *buf,
                           struct MemMappedReg const *reg_info);

    int ExpevtRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int ExpevtRegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info);

    int IntevtRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int IntevtRegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info);

    int Qacr0RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Qacr0RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Qacr1RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Qacr1RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int TocrRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int TocrRegWriteHandler(void const *buf,
                            struct MemMappedReg const *reg_info);

    int TstrRegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int TstrRegWriteHandler(void const *buf,
                            struct MemMappedReg const *reg_info);

    int Tcor0RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcor0RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcnt0RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcnt0RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcr0RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcr0RegWriteHandler(void const *buf,
                            struct MemMappedReg const *reg_info);

    int Tcor1RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcor1RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcnt1RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcnt1RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcr1RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcr1RegWriteHandler(void const *buf,
                            struct MemMappedReg const *reg_info);

    int Tcor2RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcor2RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcnt2RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcnt2RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    int Tcr2RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcr2RegWriteHandler(void const *buf,
                            struct MemMappedReg const *reg_info);

    int Tcpr2RegReadHandler(void *buf, struct MemMappedReg const *reg_info);
    int Tcpr2RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info);

    /*
     * called for P4 area read/write ops that
     * fall in the memory-mapped register range
     */
    int read_mem_mapped_reg(void *buf, addr32_t addr, unsigned len);
    int write_mem_mapped_reg(void const *buf, addr32_t addr, unsigned len);

    /*
     * this is called from the sh4 constructor to
     * initialize all memory-mapped registers
     */
    void init_regs();

    // set up the memory-mapped registers for a reset;
    void poweron_reset_regs();
    void manual_reset_regs();
};

#endif
