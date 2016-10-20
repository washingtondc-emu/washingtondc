/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include <boost/cstdint.hpp>

#include "types.hpp"
#include "Memory.hpp"

/* Hitachi SuperH-4 interpreter */

class Sh4 {
public:
    Sh4(Memory *mem);
    ~Sh4();

    void set_exception(unsigned excp_code);
    void set_interrupt(unsigned intp_code);
private:

    enum PhysMemArea {
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

    enum PageSize {
        ONE_KILO = 0,
        FOUR_KILO = 1,
        SIXTYFOUR_KILO = 2,
        ONE_MEGA = 3
    };

    // UTLB Valid bit
    static const unsigned UTLB_KEY_VALID_SHIFT = 0;
    static const unsigned UTLB_KEY_VALID_MASK = 1 << UTLB_KEY_VALID_SHIFT;

    // UTLB Virtual Page Number
    static const unsigned UTLB_KEY_VPN_SHIFT = 1;
    static const unsigned UTLB_KEY_VPN_MASK = 0x3fffff << UTLB_KEY_VPN_SHIFT;

    // UTLB Address-Space Identifier
    static const unsigned UTLB_KEY_ASID_SHIFT = 23;
    static const unsigned UTLB_KEY_ASID_MASK = 0xff << UTLB_KEY_ASID_SHIFT;

    // UTLB Timing Control - I have no idea what this is
    // (see page 41 of the sh7750 hardware manual)
    static const unsigned UTLB_ENT_TC_SHIFT = 0;
    static const unsigned UTLB_ENT_TC_MASK = 1 << UTLB_ENT_TC_SHIFT;

    // UTLB Space Attribute
    static const unsigned UTLB_ENT_SA_SHIFT = 1;
    static const unsigned UTLB_ENT_SA_MASK = 0x7 << UTLB_ENT_SA_SHIFT;

    // UTLB Write-Through
    static const unsigned UTLB_ENT_WT_SHIFT = 4;
    static const unsigned UTLB_ENT_WT_MASK = 1 << UTLB_ENT_WT_SHIFT;

    // UTLB Dirty Bit
    static const unsigned UTLB_ENT_D_SHIFT = 5;
    static const unsigned UTLB_ENT_D_MASK = 1 << UTLB_ENT_D_SHIFT;

    // UTLB Protection-Key data
    static const unsigned UTLB_ENT_PR_SHIFT = 6;
    static const unsigned UTLB_ENT_PR_MASK = 3 << UTLB_ENT_PR_SHIFT;

    // UTLB Cacheability bit
    static const unsigned UTLB_ENT_C_SHIFT = 9;
    static const unsigned UTLB_ENT_C_MASK = 1 << UTLB_ENT_C_SHIFT;

    // UTLB Share status bit
    static const unsigned UTLB_ENT_SH_SHIFT = 10;
    static const unsigned UTLB_ENT_SH_MASK = 1 << UTLB_ENT_SH_SHIFT;

    // UTLB Page size (see enum PageSize definition)
    static const unsigned UTLB_ENT_SZ_SHIFT = 11;
    static const unsigned UTLB_ENT_SZ_MASK = 3 << UTLB_ENT_SZ_SHIFT;

    // UTLB Physical Page Number
    static const unsigned UTLB_ENT_PPN_SHIFT = 14;
    static const unsigned UTLB_ENT_PPN_MASK = 0x7ffff << UTLB_ENT_PPN_SHIFT;

    // ITLB Valid bit
    static const unsigned ITLB_KEY_VALID_SHIFT = 0;
    static const unsigned ITLB_KEY_VALID_MASK = 1 << ITLB_KEY_VALID_SHIFT;

    // ITLB Virtual Page Number
    static const unsigned ITLB_KEY_VPN_SHIFT = 1;
    static const unsigned ITLB_KEY_VPN_MASK = 0x3fffff << ITLB_KEY_VPN_SHIFT;

    // ITLB Address-Space Identifier
    static const unsigned ITLB_KEY_ASID_SHIFT = 23;
    static const unsigned ITLB_KEY_ASID_MASK = 0xff << ITLB_KEY_ASID_SHIFT;

    // ITLB Timing Control - I have no idea what this is
    // (see page 41 of the sh7750 hardware manual)
    static const unsigned ITLB_ENT_TC_SHIFT = 0;
    static const unsigned ITLB_ENT_TC_MASK = 1 << ITLB_ENT_TC_SHIFT;

    // ITLB Space Attribute
    static const unsigned ITLB_ENT_SA_SHIFT = 1;
    static const unsigned ITLB_ENT_SA_MASK = 0x7 << ITLB_ENT_SA_SHIFT;

    // ITLB Protection Key data (0=priveleged, 1=user or priveleged)
    static const unsigned ITLB_ENT_PR_SHIFT = 4;
    static const unsigned IRLB_ENT_PR_MASK = 1 << ITLB_ENT_PR_SHIFT;

    // ITLB Cacheability flag
    static const unsigned ITLB_ENT_C_SHIFT = 5;
    static const unsigned ITLB_ENT_C_MASK = 1 << ITLB_ENT_C_SHIFT;

    // ITLB Share status Bit
    static const unsigned ITLB_ENT_SH_SHIFT = 6;
    static const unsigned ITLB_ENT_SH_MASK = 1 << ITLB_ENT_SH_SHIFT;

    // ITLB Page size (see enum PageSize definition)
    static const unsigned ITLB_ENT_SZ_SHIFT = 7;
    static const unsigned ITLB_ENT_SZ_MASK = 0x3 << ITLB_ENT_SZ_SHIFT;

    // ITLB Physical Page Number
    static const unsigned ITLB_ENT_PPN_SHIFT = 9;
    static const unsigned ITLB_ENT_PPN_MASK = 0x7ffff << ITLB_ENT_PPN_SHIFT;

    struct utlb_entry {
        boost::uint32_t key;
        boost::uint32_t ent;
    };

    struct itlb_entry {
        boost::uint32_t key;
        boost::uint32_t ent;
    };

    static const size_t UTLB_SIZE = 64;
    struct utlb_entry utlb[UTLB_SIZE];

    static const size_t ITLB_SIZE = 4;
    struct itlb_entry itlb[ITLB_SIZE];

    Icache *inst_cache;
    Ocache *op_cache;

    enum PhysMemArea get_mem_area(addr32_t addr);

    /*
     * return the utlb entry for vaddr.
     * On failure, this will return NULL and set the appropriate CPU
     * flags to signal an exception of some sort.
     */
    struct utlb_entry *utlb_search(addr32_t vaddr);

    /*
     * Return the itlb entry for vaddr.
     * On failure this will return NULL and set the appropriate CPU
     * flags to signal an exception of some sort.
     * On miss, this function will search the utlb and if it finds what it was
     * looking for there, it will replace one of the itlb entries with the utlb
     * entry as outlined on pade 44 of the SH7750 Hardware Manual.
     */
    struct itlb_entry *itlb_search(addr32_t vaddr);

    /*
    * From within the CPU, these functions should be called instead of
    * the memory's read/write functions because these implement the MMU
    * functionality.  In the event of a failure, these functions will set the
    * appropriate CPU flags for an exception and return non-zero.  On success
    * they will return zero.
    */
    int write_mem(void const *out, addr32_t addr, size_t len);
    int read_mem(void *out, addr32_t addr, size_t len);

    Memory *mem;

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
    static const unsigned FR_RM_SHIFT = 0;
    static const unsigned FR_RM_MASK = 3 << FR_RM_SHIFT;

    // FPU exception flags
    static const unsigned FR_FLAG_SHIFT = 2;
    static const unsigned FR_FLAG_MASK = 0x1f << FR_FLAG_SHIFT;

    // FPU exception enable
    static const unsigned FR_ENABLE_SHIFT = 7;
    static const unsigned FR_ENABLE_MASK = 0x1f << FR_FLAG_SHIFT;

    // FPU exception cause
    static const unsigned FR_CAUSE_SHIFT = 12;
    static const unsigned FR_CAUSE_MASK = 0x1f << FR_CAUSE_SHIFT;

    // FPU Denormalization mode
    static const unsigned FR_DN_SHIFT = 18;
    static const unsigned FR_DN_MASK = 1 << FR_DN_SHIFT;

    // FPU Precision mode
    static const unsigned FR_PR_SHIFT = 19;
    static const unsigned FR_PR_MASK = 1 << FR_PR_SHIFT;

    // FPU Transfer size mode
    static const unsigned FR_SZ_SHIFT = 20;
    static const unsigned FR_SZ_MASK = 1 << FR_SZ_SHIFT;

    // FPU bank switch
    static const unsigned FR_FR_SHIFT = 21;
    static const unsigned FR_FR_MASK = 1 << FR_FR_SHIFT;

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

        // floating point status/control register
        reg32_t fpscr;

        // floating-point communication register
        reg32_t fpul;
    } reg;

    static const unsigned MMUPTEH_ASID_SHIFT = 0;
    static const unsigned MMUPTEH_ASID_MASK = 0xff << MMUPTEH_ASID_SHIFT;

    static const unsigned MMUCR_AT_SHIFT = 0;
    static const unsigned MMUCR_AT_MASK = 1 << MMUCR_AT_SHIFT;

    static const unsigned MMUCR_TI_SHIFT = 2;
    static const unsigned MMUCR_TI_MASK = 1 << MMUCR_TI_SHIFT;

    // Single (=1)/Multiple(=0) Virtual Memory switch bit
    static const unsigned MMUCR_SV_SHIFT = 8;
    static const unsigned MMUCR_SV_MASK = 1 << MMUCR_SV_SHIFT;

    static const unsigned MMUCR_SQMD_SHIFT = 9;
    static const unsigned MMUCR_SQMD_MASK = 1 << MMUCR_SQMD_SHIFT;

    static const unsigned MMUCR_URC_SHIFT = 10;
    static const unsigned MMUCR_URC_MASK = 0x3f << MMUCR_URC_SHIFT;

    static const unsigned MMUCR_URB_SHIFT = 18;
    static const unsigned MMUCR_URB_MASK = 0x3f << MMUCR_URB_SHIFT;

    static const unsigned MMUCR_LRUI_SHIFT = 26;
    static const unsigned MMUCR_LRUI_MASK = 0x3f << MMUCR_LRUI_SHIFT;

    struct Mmu {
        // Page table entry high
        reg32_t pteh;

        // Page table entry low
        reg32_t ptel;

        // Page table entry assisstance
        reg32_t ptea;

        // Translation table base
        reg32_t ttb;

        // TLB exception address
        reg32_t tea;

        // MMU control
        reg32_t mmucr;
    } mmu;

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
};

#endif
