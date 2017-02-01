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
#include "Ocache.hpp"
#include "Icache.hpp"
#include "sh4_mmu.hpp"

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
    struct RegFile {
        // general-purpose registers R0_BANK0-R7_BANK0
        reg32_t r_bank0[8];

        // general-purpose registers R0_BANK1-R7_BANK1
        reg32_t r_bank1[8];

        // general-purpose registers R8-R15
        reg32_t rgen[8];

        // floating point bank0 registers FPR0_BANK0-FPR15_BANK0
        reg32_t fpr_bank0;

        // floating point bank1 registers FPR0_BANK1-FPR15_BANK1
        reg32_t fpr_bank1;

        // status register
        reg32_t sr;

        // saved-status register
        reg32_t ssr;

        // saved program counter
        reg32_t spc;

        // global base register
        reg32_t gbr;

        // vector base register
        reg32_t vbr;

        // saved general register 15
        reg32_t sgr;

        // debug base register
        reg32_t dbr;

        // Multiply-and-accumulate register high
        reg32_t mach;

        // multiply-and-accumulate register low
        reg32_t macl;

        // procedure register
        reg32_t pr;

        // program counter
        reg32_t pc;
    };
    RegFile reg;

    struct sh4_mmu mmu;

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
        reg.pc += 2;
    }

    void do_exec_inst(inst_t inst);

    // runs inst as a delay slot.
    void exec_delay_slot(addr32_t addr);

    // returns the program counter
    reg32_t get_pc() const;

    // these four APIs are intended primarily for debuggers to use
    RegFile get_regs() const;
    FpuReg get_fpu() const;
    void set_regs(const RegFile& src);
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

private:
    enum VirtMemArea {
        AREA_P0 = 0,
        AREA_P1,
        AREA_P2,
        AREA_P3,
        AREA_P4
    };

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

    /*
     * return a pointer to the given general-purpose register.
     * This function takes bank-switching into account.
     */
    reg32_t *gen_reg(int idx) {
        assert(!(idx & ~0xf));

        if (idx <= 7) {
            if (reg.sr & SR_RB_MASK)
                return &reg.r_bank1[idx];
            else
                return &reg.r_bank0[idx];
        } else {
            return &reg.rgen[idx - 8];
        }
    }

    // return a pointer to the given banked general-purpose register
    reg32_t *bank_reg(int idx) {
        assert(!(idx & ~0x7));

        if (reg.sr & SR_RB_MASK)
            return &reg.r_bank0[idx];
        else
            return &reg.r_bank1[idx];
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

    union OpArgs {
        inst_t inst;

        // operators that take a single general-purpose register and/or
        // an 8-bit immediate value
        struct {
            inst_t imm8 : 8;
            inst_t gen_reg : 4;
        inst_t : 4;
        };

        // signed 8-bit immediate value
        struct {
            int8_t simm8 : 8;
        inst_t : 8;
        };

        // operators that take a base register and a 4-bit displacement
        struct {
            inst_t imm4 : 4;

            /* the nomenclature here is kind of confusing:
             * base_reg_src can be a source *or* a dest for opcodes that only
             * take one register.  For opcodes that need two registers,
             * base_reg_src is the source and base_reg_dst is the destination.
             */
            inst_t base_reg_src : 4;
            inst_t base_reg_dst : 4;
        };

        // operators that take a 12-bit immediate value
        struct {
            inst_t imm12 : 12;
        inst_t : 4;
        };

        // signed 12-bit immediate value
        struct {
            int16_t simm12 : 12;
        inst_t : 4;
        };

        // operators that take two general-purpose registers
        struct {
        inst_t : 4;
            inst_t src_reg : 4;
            inst_t dst_reg : 4;
        inst_t : 4;
        };

        // opcodes that take a single floating-point register
        struct {
        inst_t : 8;
            inst_t fr_reg : 4;
        inst_t : 4;
        };

        // opcodes that take two floating-point registers
        struct {
        inst_t : 4;
            inst_t fr_src : 4;
            inst_t fr_dst : 4;
        inst_t : 4;
        };

        // opcodes that take a single double-precision floating-point register
        struct {
        inst_t : 9;
            inst_t dr_reg : 3;
        inst_t : 4;
        };

        // opcodes that take two double-precision floating-point registers
        struct {
        inst_t : 5;
            inst_t dr_src : 3;
        inst_t : 1;
            inst_t dr_dst : 3;
        inst_t : 4;
        };

        // opcodes that take a single double-precision floating-point register
        struct {
        inst_t : 9;
            inst_t xd_reg : 3;
        inst_t : 4;
        };

        // opcodes that take two double-precision floating-point registers
        struct {
        inst_t : 5;
            inst_t xd_src : 3;
        inst_t : 1;
            inst_t xd_dst : 3;
        inst_t : 4;
        };

        // opcodes that take two floating-point vector registers
        struct {
        inst_t : 8;
            inst_t fv_src : 2;
            inst_t fv_dst : 2;
        inst_t : 4;
        };

        // opcodes that take a single floating-point vector register
        struct {
        inst_t : 10;
            inst_t fv_reg : 2;
        inst_t : 4;
        };

        // operators that take in a banked register
        struct {
        inst_t : 4;
            inst_t bank_reg : 3;
        inst_t : 9;
        };
    };

    BOOST_STATIC_ASSERT(sizeof(OpArgs) == 2);

    typedef void (Sh4::*opcode_func_t)(OpArgs oa);

    // RTS
    // 0000000000001011
    void inst_rts(OpArgs inst);

    // CLRMAC
    // 0000000000101000
    void inst_clrmac(OpArgs inst);

    // CLRS
    // 0000000001001000
    void inst_clrs(OpArgs inst);

    // CLRT
    // 0000000000001000
    void inst_clrt(OpArgs inst);

    // LDTLB
    // 0000000000111000
    void inst_ldtlb(OpArgs inst);

    // NOP
    // 0000000000001001
    void inst_nop(OpArgs inst);

    // RTE
    // 0000000000101011
    void inst_rte(OpArgs inst);

    // SETS
    // 0000000001011000
    void inst_sets(OpArgs inst);

    // SETT
    // 0000000000011000
    void inst_sett(OpArgs inst);

    // SLEEP
    // 0000000000011011
    void inst_sleep(OpArgs inst);

    // FRCHG
    // 1111101111111101
    void inst_frchg(OpArgs inst);

    // FSCHG
    // 1111001111111101
    void inst_fschg(OpArgs inst);

    // MOVT Rn
    // 0000nnnn00101001
    void inst_unary_movt_gen(OpArgs inst);

    // CMP/PZ Rn
    // 0100nnnn00010001
    void inst_unary_cmppz_gen(OpArgs inst);

    // CMP/PL Rn
    // 0100nnnn00010101
    void inst_unary_cmppl_gen(OpArgs inst);

    // DT Rn
    // 0100nnnn00010000
    void inst_unary_dt_gen(OpArgs inst);

    // ROTL Rn
    // 0100nnnn00000100
    void inst_unary_rotl_gen(OpArgs inst);

    // ROTR Rn
    // 0100nnnn00000101
    void inst_unary_rotr_gen(OpArgs inst);

    // ROTCL Rn
    // 0100nnnn00100100
    void inst_unary_rotcl_gen(OpArgs inst);

    // ROTCR Rn
    // 0100nnnn00100101
    void inst_unary_rotcr_gen(OpArgs inst);

    // SHAL Rn
    // 0100nnnn00200000
    void inst_unary_shal_gen(OpArgs inst);

    // SHAR Rn
    // 0100nnnn00100001
    void inst_unary_shar_gen(OpArgs inst);

    // SHLL Rn
    // 0100nnnn00000000
    void inst_unary_shll_gen(OpArgs inst);

    // SHLR Rn
    // 0100nnnn00000001
    void inst_unary_shlr_gen(OpArgs inst);

    // SHLL2 Rn
    // 0100nnnn00001000
    void inst_unary_shll2_gen(OpArgs inst);

    // SHLR2 Rn
    // 0100nnnn00001001
    void inst_unary_shlr2_gen(OpArgs inst);

    // SHLL8 Rn
    // 0100nnnn00011000
    void inst_unary_shll8_gen(OpArgs inst);

    // SHLR8 Rn
    // 0100nnnn00011001
    void inst_unary_shlr8_gen(OpArgs inst);

    // SHLL16 Rn
    // 0100nnnn00101000
    void inst_unary_shll16_gen(OpArgs inst);

    // SHLR16 Rn
    // 0100nnnn00101001
    void inst_unary_shlr16_gen(OpArgs inst);

    // BRAF Rn
    // 0000nnnn00100011
    void inst_unary_braf_gen(OpArgs inst);

    // BSRF Rn
    // 0000nnnn00000011
    void inst_unary_bsrf_gen(OpArgs inst);

    // CMP/EQ #imm, R0
    // 10001000iiiiiiii
    void inst_binary_cmpeq_imm_r0(OpArgs inst);

    // AND.B #imm, @(R0, GBR)
    // 11001101iiiiiiii
    void inst_binary_andb_imm_r0_gbr(OpArgs inst);

    // AND #imm, R0
    // 11001001iiiiiiii
    void inst_binary_and_imm_r0(OpArgs inst);

    // OR.B #imm, @(R0, GBR)
    // 11001111iiiiiiii
    void inst_binary_orb_imm_r0_gbr(OpArgs inst);

    // OR #imm, R0
    // 11001011iiiiiiii
    void inst_binary_or_imm_r0(OpArgs inst);

    // TST #imm, R0
    // 11001000iiiiiiii
    void inst_binary_tst_imm_r0(OpArgs inst);

    // TST.B #imm, @(R0, GBR)
    // 11001100iiiiiiii
    void inst_binary_tstb_imm_r0_gbr(OpArgs inst);

    // XOR #imm, R0
    // 11001010iiiiiiii
    void inst_binary_xor_imm_r0(OpArgs inst);

    // XOR.B #imm, @(R0, GBR)
    // 11001110iiiiiiii
    void inst_binary_xorb_imm_r0_gbr(OpArgs inst);

    // BF label
    // 10001011dddddddd
    void inst_unary_bf_disp(OpArgs inst);

    // BF/S label
    // 10001111dddddddd
    void inst_unary_bfs_disp(OpArgs inst);

    // BT label
    // 10001001dddddddd
    void inst_unary_bt_disp(OpArgs inst);

    // BT/S label
    // 10001101dddddddd
    void inst_unary_bts_disp(OpArgs inst);

    // BRA label
    // 1010dddddddddddd
    void inst_unary_bra_disp(OpArgs inst);

    // BSR label
    // 1011dddddddddddd
    void inst_unary_bsr_disp(OpArgs inst);

    // TRAPA #immed
    // 11000011iiiiiiii
    void inst_unary_trapa_disp(OpArgs inst);

    // TAS.B @Rn
    // 0100nnnn00011011
    void inst_unary_tasb_gen(OpArgs inst);

    // OCBI @Rn
    // 0000nnnn10100011
    void inst_unary_ocbi_indgen(OpArgs inst);

    // OCBP @Rn
    // 0000nnnn10100011
    void inst_unary_ocbp_indgen(OpArgs inst);

    // PREF @Rn
    // 0000nnnn10000011
    void inst_unary_pref_indgen(OpArgs inst);

    // JMP @Rn
    // 0100nnnn00101011
    void inst_unary_jmp_indgen(OpArgs inst);

    // JSR @Rn
    //0100nnnn00001011
    void inst_unary_jsr_indgen(OpArgs inst);

    // LDC Rm, SR
    // 0100mmmm00001110
    void inst_binary_ldc_gen_sr(OpArgs inst);

    // LDC Rm, GBR
    // 0100mmmm00011110
    void inst_binary_ldc_gen_gbr(OpArgs inst);

    // LDC Rm, VBR
    // 0100mmmm00101110
    void inst_binary_ldc_gen_vbr(OpArgs inst);

    // LDC Rm, SSR
    // 0100mmmm00111110
    void inst_binary_ldc_gen_ssr(OpArgs inst);

    // LDC Rm, SPC
    // 0100mmmm01001110
    void inst_binary_ldc_gen_spc(OpArgs inst);

    // LDC Rm, DBR
    // 0100mmmm11111010
    void inst_binary_ldc_gen_dbr(OpArgs inst);

    // STC SR, Rn
    // 0000nnnn00000010
    void inst_binary_stc_sr_gen(OpArgs inst);

    // STC GBR, Rn
    // 0000nnnn00010010
    void inst_binary_stc_gbr_gen(OpArgs inst);

    // STC VBR, Rn
    // 0000nnnn00100010
    void inst_binary_stc_vbr_gen(OpArgs inst);

    // STC SSR, Rn
    // 0000nnnn01000010
    void inst_binary_stc_ssr_gen(OpArgs inst);

    // STC SPC, Rn
    // 0000nnnn01000010
    void inst_binary_stc_spc_gen(OpArgs inst);

    // STC SGR, Rn
    // 0000nnnn00111010
    void inst_binary_stc_sgr_gen(OpArgs inst);

    // STC DBR, Rn
    // 0000nnnn11111010
    void inst_binary_stc_dbr_gen(OpArgs inst);

    // LDC.L @Rm+, SR
    // 0100mmmm00000111
    void inst_binary_ldcl_indgeninc_sr(OpArgs inst);

    // LDC.L @Rm+, GBR
    // 0100mmmm00010111
    void inst_binary_ldcl_indgeninc_gbr(OpArgs inst);

    // LDC.L @Rm+, VBR
    // 0100mmmm00100111
    void inst_binary_ldcl_indgeninc_vbr(OpArgs inst);

    // LDC.L @Rm+, SSR
    // 0100mmmm00110111
    void inst_binary_ldcl_indgenic_ssr(OpArgs inst);

    // LDC.L @Rm+, SPC
    // 0100mmmm01000111
    void inst_binary_ldcl_indgeninc_spc(OpArgs inst);

    // LDC.L @Rm+, DBR
    // 0100mmmm11110110
    void inst_binary_ldcl_indgeninc_dbr(OpArgs inst);

    // STC.L SR, @-Rn
    // 0100nnnn00000011
    void inst_binary_stcl_sr_inddecgen(OpArgs inst);

    // STC.L GBR, @-Rn
    // 0100nnnn00010011
    void inst_binary_stcl_gbr_inddecgen(OpArgs inst);

    // STC.L VBR, @-Rn
    // 0100nnnn00100011
    void inst_binary_stcl_vbr_inddecgen(OpArgs inst);

    // STC.L SSR, @-Rn
    // 0100nnnn00110011
    void inst_binary_stcl_ssr_inddecgen(OpArgs inst);

    // STC.L SPC, @-Rn
    // 0100nnnn01000011
    void inst_binary_stcl_spc_inddecgen(OpArgs inst);

    // STC.L SGR, @-Rn
    // 0100nnnn00110010
    void inst_binary_stcl_sgr_inddecgen(OpArgs inst);

    // STC.L DBR, @-Rn
    // 0100nnnn11110010
    void inst_binary_stcl_dbr_inddecgen(OpArgs inst);

    // MOV #imm, Rn
    // 1110nnnniiiiiiii
    void inst_binary_mov_imm_gen(OpArgs inst);

    // ADD #imm, Rn
    // 0111nnnniiiiiiii
    void inst_binary_add_imm_gen(OpArgs inst);

    // MOV.W @(disp, PC), Rn
    // 1001nnnndddddddd
    void inst_binary_movw_binind_disp_pc_gen(OpArgs inst);

    // MOV.L @(disp, PC), Rn
    // 1101nnnndddddddd
    void inst_binary_movl_binind_disp_pc_gen(OpArgs inst);

    // MOV Rm, Rn
    // 0110nnnnmmmm0011
    void inst_binary_movw_gen_gen(OpArgs inst);

    // SWAP.B Rm, Rn
    // 0110nnnnmmmm1000
    void inst_binary_swapb_gen_gen(OpArgs inst);

    // SWAP.W Rm, Rn
    // 0110nnnnmmmm1001
    void inst_binary_swapw_gen_gen(OpArgs inst);

    // XTRCT Rm, Rn
    // 0110nnnnmmmm1101
    void inst_binary_xtrct_gen_gen(OpArgs inst);

    // ADD Rm, Rn
    // 0011nnnnmmmm1100
    void inst_binary_add_gen_gen(OpArgs inst);

    // ADDC Rm, Rn
    // 0011nnnnmmmm1110
    void inst_binary_addc_gen_gen(OpArgs inst);

    // ADDV Rm, Rn
    // 0011nnnnmmmm1111
    void inst_binary_addv_gen_gen(OpArgs inst);

    // CMP/EQ Rm, Rn
    // 0011nnnnmmmm0000
    void inst_binary_cmpeq_gen_gen(OpArgs inst);

    // CMP/HS Rm, Rn
    // 0011nnnnmmmm0010
    void inst_binary_cmphs_gen_gen(OpArgs inst);

    // CMP/GE Rm, Rn
    // 0011nnnnmmmm0011
    void inst_binary_cmpge_gen_gen(OpArgs inst);

    // CMP/HI Rm, Rn
    // 0011nnnnmmmm0110
    void inst_binary_cmphi_gen_gen(OpArgs inst);

    // CMP/GT Rm, Rn
    // 0011nnnnmmmm0111
    void inst_binary_cmpgt_gen_gen(OpArgs inst);

    // CMP/STR Rm, Rn
    // 0010nnnnmmmm1100
    void inst_binary_cmpstr_gen_gen(OpArgs inst);

    // DIV1 Rm, Rn
    // 0011nnnnmmmm0100
    void inst_binary_div1_gen_gen(OpArgs inst);

    // DIV0S Rm, Rn
    // 0010nnnnmmmm0111
    void inst_binary_div0s_gen_gen(OpArgs inst);

    // DMULS.L Rm, Rn
    //0011nnnnmmmm1101
    void inst_binary_dmulsl_gen_gen(OpArgs inst);

    // DMULU.L Rm, Rn
    // 0011nnnnmmmm0101
    void inst_binary_dmulul_gen_gen(OpArgs inst);

    // EXTS.B Rm, Rn
    // 0110nnnnmmmm1110
    void inst_binary_extsb_gen_gen(OpArgs inst);

    // EXTS.W Rm, Rnn
    // 0110nnnnmmmm1111
    void inst_binary_extsw_gen_gen(OpArgs inst);

    // EXTU.B Rm, Rn
    // 0110nnnnmmmm1100
    void inst_binary_extub_gen_gen(OpArgs inst);

    // EXTU.W Rm, Rn
    // 0110nnnnmmmm1101
    void inst_binary_extuw_gen_gen(OpArgs inst);

    // MUL.L Rm, Rn
    // 0000nnnnmmmm0111
    void inst_binary_mull_gen_gen(OpArgs inst);

    // MULS.W Rm, Rn
    // 0010nnnnmmmm1111
    void inst_binary_mulsw_gen_gen(OpArgs inst);

    // MULU.W Rm, Rn
    // 0010nnnnmmmm1110
    void inst_binary_muluw_gen_gen(OpArgs inst);

    // NEG Rm, Rn
    // 0110nnnnmmmm1011
    void inst_binary_neg_gen_gen(OpArgs inst);

    // NEGC Rm, Rn
    // 0110nnnnmmmm1010
    void inst_binary_negc_gen_gen(OpArgs inst);

    // SUB Rm, Rn
    // 0011nnnnmmmm1000
    void inst_binary_sub_gen_gen(OpArgs inst);

    // SUBC Rm, Rn
    // 0011nnnnmmmm1010
    void inst_binary_subc_gen_gen(OpArgs inst);

    // SUBV Rm, Rn
    // 0011nnnnmmmm1011
    void inst_binary_subv_gen_gen(OpArgs inst);

    // AND Rm, Rn
    // 0010nnnnmmmm1001
    void inst_binary_and_gen_gen(OpArgs inst);

    // NOT Rm, Rn
    // 0110nnnnmmmm0111
    void inst_binary_not_gen_gen(OpArgs inst);

    // OR Rm, Rn
    // 0010nnnnmmmm1011
    void inst_binary_or_gen_gen(OpArgs inst);

    // TST Rm, Rn
    // 0010nnnnmmmm1000
    void inst_binary_tst_gen_gen(OpArgs inst);

    // XOR Rm, Rn
    // 0010nnnnmmmm1010
    void inst_binary_xor_gen_gen(OpArgs inst);

    // SHAD Rm, Rn
    // 0100nnnnmmmm1100
    void inst_binary_shad_gen_gen(OpArgs inst);

    // SHLD Rm, Rn
    // 0100nnnnmmmm1101
    void inst_binary_shld_gen_gen(OpArgs inst);

    // LDC Rm, Rn_BANK
    // 0100mmmm1nnn1110
    void inst_binary_ldc_gen_bank(OpArgs inst);

    // LDC.L @Rm+, Rn_BANK
    // 0100mmmm1nnn0111
    void inst_binary_ldcl_indgeninc_bank(OpArgs inst);

    // STC Rm_BANK, Rn
    // 0000nnnn1mmm0010
    void inst_binary_stc_bank_gen(OpArgs inst);

    // STC.L Rm_BANK, @-Rn
    // 0100nnnn1mmm0011
    void inst_binary_stcl_bank_inddecgen(OpArgs inst);

    // LDS Rm,MACH
    // 0100mmmm00001010
    void inst_binary_lds_gen_mach(OpArgs inst);

    // LDS Rm, MACL
    // 0100mmmm00011010
    void inst_binary_lds_gen_macl(OpArgs inst);

    // STS MACH, Rn
    // 0000nnnn00001010
    void inst_binary_sts_mach_gen(OpArgs inst);

    // STS MACL, Rn
    // 0000nnnn00011010
    void inst_binary_sts_macl_gen(OpArgs inst);

    // LDS Rm, PR
    // 0100mmmm00101010
    void inst_binary_lds_gen_pr(OpArgs inst);

    // STS PR, Rn
    // 0000nnnn00101010
    void inst_binary_sts_pr_gen(OpArgs inst);

    // LDS.L @Rm+, MACH
    // 0100mmmm00000110
    void inst_binary_ldsl_indgeninc_mach(OpArgs inst);

    // LDS.L @Rm+, MACL
    // 0100mmmm00010110
    void inst_binary_ldsl_indgeninc_macl(OpArgs inst);

    // STS.L MACH, @-Rn
    // 0100mmmm00000010
    void inst_binary_stsl_mach_inddecgen(OpArgs inst);

    // STS.L MACL, @-Rn
    // 0100mmmm00010010
    void inst_binary_stsl_macl_inddecgen(OpArgs inst);

    // LDS.L @Rm+, PR
    // 0100mmmm00100110
    void inst_binary_ldsl_indgeninc_pr(OpArgs inst);

    // STS.L PR, @-Rn
    // 0100nnnn00100010
    void inst_binary_stsl_pr_inddecgen(OpArgs inst);

    // MOV.B Rm, @Rn
    // 0010nnnnmmmm0000
    void inst_binary_movb_gen_indgen(OpArgs inst);

    // MOV.W Rm, @Rn
    // 0010nnnnmmmm0001
    void inst_binary_movw_gen_indgen(OpArgs inst);

    // MOV.L Rm, @Rn
    // 0010nnnnmmmm0010
    void inst_binary_movl_gen_indgen(OpArgs inst);

    // MOV.B @Rm, Rn
    // 0110nnnnmmmm0000
    void inst_binary_movb_indgen_gen(OpArgs inst);

    // MOV.W @Rm, Rn
    // 0110nnnnmmmm0001
    void inst_binary_movw_indgen_gen(OpArgs inst);

    // MOV.L @Rm, Rn
    // 0110nnnnmmmm0010
    void inst_binary_movl_indgen_gen(OpArgs inst);

    // MOV.B Rm, @-Rn
    // 0010nnnnmmmm0100
    void inst_binary_movb_gen_inddecgen(OpArgs inst);

    // MOV.W Rm, @-Rn
    // 0010nnnnmmmm0101
    void inst_binary_movw_gen_inddecgen(OpArgs inst);

    // MOV.L Rm, @-Rn
    // 0010nnnnmmmm0110
    void inst_binary_movl_gen_inddecgen(OpArgs inst);

    // MOV.B @Rm+, Rn
    // 0110nnnnmmmm0100
    void inst_binary_movb_indgeninc_gen(OpArgs inst);

    // MOV.W @Rm+, Rn
    // 0110nnnnmmmm0101
    void inst_binary_movw_indgeninc_gen(OpArgs inst);

    // MOV.L @Rm+, Rn
    // 0110nnnnmmmm0110
    void inst_binary_movl_indgeninc_gen(OpArgs inst);

    // MAC.L @Rm+, @Rn+
    // 0000nnnnmmmm1111
    void inst_binary_macl_indgeninc_indgeninc(OpArgs inst);

    // MAC.W @Rm+, @Rn+
    // 0100nnnnmmmm1111
    void inst_binary_macw_indgeninc_indgeninc(OpArgs inst);

    // MOV.B R0, @(disp, Rn)
    // 10000000nnnndddd
    void inst_binary_movb_r0_binind_disp_gen(OpArgs inst);

    // MOV.W R0, @(disp, Rn)
    // 10000001nnnndddd
    void inst_binary_movw_r0_binind_disp_gen(OpArgs inst);

    // MOV.L Rm, @(disp, Rn)
    // 0001nnnnmmmmdddd
    void inst_binary_movl_gen_binind_disp_gen(OpArgs inst);

    // MOV.B @(disp, Rm), R0
    // 10000100mmmmdddd
    void inst_binary_movb_binind_disp_gen_r0(OpArgs inst);

    // MOV.W @(disp, Rm), R0
    // 10000101mmmmdddd
    void inst_binary_movw_binind_disp_gen_r0(OpArgs inst);

    // MOV.L @(disp, Rm), Rn
    // 0101nnnnmmmmdddd
    void inst_binary_movl_binind_disp_gen_gen(OpArgs inst);

    // MOV.B Rm, @(R0, Rn)
    // 0000nnnnmmmm0100
    void inst_binary_movb_gen_binind_r0_gen(OpArgs inst);

    // MOV.W Rm, @(R0, Rn)
    // 0000nnnnmmmm0101
    void inst_binary_movw_gen_binind_r0_gen(OpArgs inst);

    // MOV.L Rm, @(R0, Rn)
    // 0000nnnnmmmm0110
    void inst_binary_movl_gen_binind_r0_gen(OpArgs inst);

    // MOV.B @(R0, Rm), Rn
    // 0000nnnnmmmm1100
    void inst_binary_movb_binind_r0_gen_gen(OpArgs inst);

    // MOV.W @(R0, Rm), Rn
    // 0000nnnnmmmm1101
    void inst_binary_movw_binind_r0_gen_gen(OpArgs inst);

    // MOV.L @(R0, Rm), Rn
    // 0000nnnnmmmm1110
    void inst_binary_movl_binind_r0_gen_gen(OpArgs inst);

    // MOV.B R0, @(disp, GBR)
    // 11000000dddddddd
    void inst_binary_movb_r0_binind_disp_gbr(OpArgs inst);

    // MOV.W R0, @(disp, GBR)
    // 11000001dddddddd
    void inst_binary_movw_r0_binind_disp_gbr(OpArgs inst);

    // MOV.L R0, @(disp, GBR)
    // 11000010dddddddd
    void inst_binary_movl_r0_binind_disp_gbr(OpArgs inst);

    // MOV.B @(disp, GBR), R0
    // 11000100dddddddd
    void inst_binary_movb_binind_disp_gbr_r0(OpArgs inst);

    // MOV.W @(disp, GBR), R0
    // 11000101dddddddd
    void inst_binary_movw_binind_disp_gbr_r0(OpArgs inst);

    // MOV.L @(disp, GBR), R0
    // 11000110dddddddd
    void inst_binary_movl_binind_disp_gbr_r0(OpArgs inst);

    // MOVA @(disp, PC), R0
    // 11000111dddddddd
    void inst_binary_mova_binind_disp_pc_r0(OpArgs inst);

    // MOVCA.L R0, @Rn
    // 0000nnnn11000011
    void inst_binary_movcal_r0_indgen(OpArgs inst);

    // FLDI0 FRn
    // 1111nnnn10001101
    void inst_unary_fldi0_fr(OpArgs inst);

    // FLDI1 Frn
    // 1111nnnn10011101
    void inst_unary_fldi1_fr(OpArgs inst);

    // FMOV FRm, FRn
    // 1111nnnnmmmm1100
    void inst_binary_fmov_fr_fr(OpArgs inst);

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    void inst_binary_fmovs_indgen_fr(OpArgs inst);

    // FMOV.S @(R0,Rm), FRn
    // 1111nnnnmmmm0110
    void inst_binary_fmovs_binind_r0_gen_fr(OpArgs inst);

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    void inst_binary_fmovs_indgeninc_fr(OpArgs inst);

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    void inst_binary_fmovs_fr_indgen(OpArgs inst);

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    void inst_binary_fmovs_fr_inddecgen(OpArgs inst);

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    void inst_binary_fmovs_fr_binind_r0_gen(OpArgs inst);

    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    void inst_binary_fmov_dr_dr(OpArgs inst);

    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    void inst_binary_fmov_indgen_dr(OpArgs inst);

    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    void inst_binary_fmov_binind_r0_gen_dr(OpArgs inst);

    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    void inst_binary_fmov_indgeninc_dr(OpArgs inst);

    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    void inst_binary_fmov_dr_indgen(OpArgs inst);

    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    void inst_binary_fmov_dr_inddecgen(OpArgs inst);

    // FMOV DRm, @(R0,Rn)
    // 1111nnnnmmm00111
    void inst_binary_fmov_dr_binind_r0_gen(OpArgs inst);

    // FLDS FRm, FPUL
    // 1111mmmm00011101
    void inst_binary_flds_fr_fpul(OpArgs inst);

    // FSTS FPUL, FRn
    // 1111nnnn00001101
    void inst_binary_fsts_fpul_fr(OpArgs inst);

    // FABS FRn
    // 1111nnnn01011101
    void inst_unary_fabs_fr(OpArgs inst);

    // FADD FRm, FRn
    // 1111nnnnmmmm0000
    void inst_binary_fadd_fr_fr(OpArgs inst);

    // FCMP/EQ FRm, FRn
    // 1111nnnnmmmm0100
    void inst_binary_fcmpeq_fr_fr(OpArgs inst);

    // FCMP/GT FRm, FRn
    // 1111nnnnmmmm0101
    void inst_binary_fcmpgt_fr_fr(OpArgs inst);

    // FDIV FRm, FRn
    // 1111nnnnmmmm0011
    void inst_binary_fdiv_fr_fr(OpArgs inst);

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    void inst_binary_float_fpul_fr(OpArgs inst);

    // FMAC FR0, FRm, FRn
    // 1111nnnnmmmm1110
    void inst_trinary_fmac_fr0_fr_fr(OpArgs inst);

    // FMUL FRm, FRn
    // 1111nnnnmmmm0010
    void inst_binary_fmul_fr_fr(OpArgs inst);

    // FNEG FRn
    // 1111nnnn01001101
    void inst_unary_fneg_fr(OpArgs inst);

    // FSQRT FRn
    // 1111nnnn01101101
    void inst_unary_fsqrt_fr(OpArgs inst);

    // FSUB FRm, FRn
    // 1111nnnnmmmm0001
    void inst_binary_fsub_fr_fr(OpArgs inst);

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    void inst_binary_ftrc_fr_fpul(OpArgs inst);

    // FABS DRn
    // 1111nnn001011101
    void inst_unary_fabs_dr(OpArgs inst);

    // FADD DRm, DRn
    // 1111nnn0mmm00000
    void inst_binary_fadd_dr_dr(OpArgs inst);

    // FCMP/EQ DRm, DRn
    // 1111nnn0mmm00100
    void inst_binary_fcmpeq_dr_dr(OpArgs inst);

    // FCMP/GT DRm, DRn
    // 1111nnn0mmm00101
    void inst_binary_fcmpgt_dr_dr(OpArgs inst);

    // FDIV DRm, DRn
    // 1111nnn0mmm00011
    void inst_binary_fdiv_dr_dr(OpArgs inst);

    // FCNVDS DRm, FPUL
    // 1111mmm010111101
    void inst_binary_fcnvds_dr_fpul(OpArgs inst);

    // FCNVSD FPUL, DRn
    // 1111nnn010101101
    void inst_binary_fcnvsd_fpul_dr(OpArgs inst);

    // FLOAT FPUL, DRn
    // 1111nnn000101101
    void inst_binary_float_fpul_dr(OpArgs inst);

    // FMUL DRm, DRn
    // 1111nnn0mmm00010
    void inst_binary_fmul_dr_dr(OpArgs inst);

    // FNEG DRn
    // 1111nnn001001101
    void inst_unary_fneg_dr(OpArgs inst);

    // FSQRT DRn
    // 1111nnn001101101
    void inst_unary_fsqrt_dr(OpArgs inst);

    // FSUB DRm, DRn
    // 1111nnn0mmm00001
    void inst_binary_fsub_dr_dr(OpArgs inst);

    // FTRC DRm, FPUL
    // 1111mmm000111101
    void inst_binary_ftrc_dr_fpul(OpArgs inst);

    // LDS Rm, FPSCR
    // 0100mmmm01101010
    void inst_binary_lds_gen_fpscr(OpArgs inst);

    // LDS Rm, FPUL
    // 0100mmmm01011010
    void inst_binary_gen_fpul(OpArgs inst);

    // LDS.L @Rm+, FPSCR
    // 0100mmmm01100110
    void inst_binary_ldsl_indgeninc_fpscr(OpArgs inst);

    // LDS.L @Rm+, FPUL
    // 0100mmmm01010110
    void inst_binary_ldsl_indgeninc_fpul(OpArgs inst);

    // STS FPSCR, Rn
    // 0000nnnn01101010
    void inst_binary_sts_fpscr_gen(OpArgs inst);

    // STS FPUL, Rn
    // 0000nnnn01011010
    void inst_binary_sts_fpul_gen(OpArgs inst);

    // STS.L FPSCR, @-Rn
    // 0100nnnn01100010
    void inst_binary_stsl_fpscr_inddecgen(OpArgs inst);

    // STS.L FPUL, @-Rn
    // 0100nnnn01010010
    void inst_binary_stsl_fpul_inddecgen(OpArgs inst);

    // FMOV DRm, XDn
    // 1111nnn1mmm01100
    void inst_binary_fmove_dr_xd(OpArgs inst);

    // FMOV XDm, DRn
    // 1111nnn0mmm11100
    void inst_binary_fmov_xd_dr(OpArgs inst);

    // FMOV XDm, XDn
    // 1111nnn1mmm11100
    void inst_binary_fmov_xd_xd(OpArgs inst);

    // FMOV @Rm, XDn
    // 1111nnn1mmmm1000
    void inst_binary_fmov_indgen_xd(OpArgs inst);

    // FMOV @Rm+, XDn
    // 1111nnn1mmmm1001
    void inst_binary_fmov_indgeninc_xd(OpArgs inst);

    // FMOV @(R0, Rn), XDn
    // 1111nnn1mmmm0110
    void inst_binary_fmov_binind_r0_gen_xd(OpArgs inst);

    // FMOV XDm, @Rn
    // 1111nnnnmmm11010
    void inst_binary_fmov_xd_indgen(OpArgs inst);

    // FMOV XDm, @-Rn
    // 1111nnnnmmm11011
    void inst_binary_fmov_xd_inddecgen(OpArgs inst);

    // FMOV XDm, @(R0, Rn)
    // 1111nnnnmmm10111
    void inst_binary_fmov_xs_binind_r0_gen(OpArgs inst);

    // FIPR FVm, FVn - vector dot product
    // 1111nnmm11101101
    void inst_binary_fipr_fv_fv(OpArgs inst);

    // FTRV MXTRX, FVn - multiple vector by matrix
    // 1111nn0111111101
    void inst_binary_fitrv_mxtrx_fv(OpArgs inst);

    static struct InstOpcode {
        // format string compiled to make mask and val
        char const *fmt;

        // opcode handler function
        opcode_func_t func;

        // if this is true, this inst cant be called from a delay slot
        bool is_branch;

        /*
         * These two fields are used for the sake of differemtiating the float
         * and double versions of opcodes from each other in the fpu.
         *
         * In order for a given opcode to match fpscr, fpscr & fpscr_mask must
         * equal fpscr_val.
         *
         * instructions that don't care should set them both to zero.
         */
        reg32_t fpscr_mask;
        reg32_t fpscr_val;

        // instructions are matched to this opcode
        // by anding with mask and checking for equality with val
        inst_t mask;
        inst_t val;
    } opcode_list[];

    static void compile_instructions();
    static void compile_instruction(struct Sh4::InstOpcode *op);

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
    typedef int(Sh4::*RegReadHandler)(void *buf, addr32_t addr, unsigned len);
    typedef int(Sh4::*RegWriteHandler)(void const *buf, addr32_t addr,
                                       unsigned len);

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

    // default read/write handler callbacks
    int DefaultRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int DefaultRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    /*
     * read handle callback that always fails (although currently it throws an
     * UnimplementedError because I don't know what the proper response is when
     * the software tries to read from an unreadable register).
     *
     * This is used for certain registers which are write-only.
     */
    int WriteOnlyRegReadHandler(void *buf, addr32_t addr, unsigned len);

    /*
     * likewise, this is a write handler for read-only registers.
     * It will also raise an exception whenever it is invokled.
     */
    int ReadOnlyRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int MmucrRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int MmucrRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int CcrRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int CcrRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int PtehRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int PtehRegWriteHandler(void const *buf, addr32_t addr, unsigned len);
    int PtelRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int PtelRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int TtbRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int TtbRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int TeaRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int TeaRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int PteaRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int PteaRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int TraRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int TraRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int ExpevtRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int ExpevtRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int IntevtRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int IntevtRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Qacr0RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Qacr0RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Qacr1RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Qacr1RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int TocrRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int TocrRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int TstrRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int TstrRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcor0RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcor0RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcnt0RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcnt0RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcr0RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcr0RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcor1RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcor1RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcnt1RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcnt1RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcr1RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcr1RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcor2RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcor2RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcnt2RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcnt2RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcr2RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcr2RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    int Tcpr2RegReadHandler(void *buf, addr32_t addr, unsigned len);
    int Tcpr2RegWriteHandler(void const *buf, addr32_t addr, unsigned len);

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
