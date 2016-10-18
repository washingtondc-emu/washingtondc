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

#include "Memory.hpp"

/* Hitachi SuperH-4 interpreter */

class Sh4 {
public:
    Sh4(Memory *mem);
private:
    typedef boost::uint32_t reg32_t;
    typedef boost::uint32_t addr32_t;
    typedef boost::uint32_t page_no_t;

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

    // The valid flag
    static const unsigned OPCACHE_KEY_VALID_SHIFT = 0;
    static const unsigned OPCACHE_KEY_VALID_MASK = 1 << OPCACHE_KEY_VALID_SHIFT;

    // the dirty flag
    static const unsigned OPCACHE_KEY_DIRTY_SHIFT = 1;
    static const unsigned OPCACHE_KEY_DIRTY_MASK = 1 << OPCACHE_KEY_DIRTY_SHIFT;

    // the tag represents bits 28:10 (inclusive) of a 29-bit address.
    static const unsigned OPCACHE_KEY_TAG_SHIFT = 2;
    static const unsigned OPCACHE_KEY_TAG_MASK = 0x7ffff << OPCACHE_KEY_TAG_SHIFT;

    static const unsigned LONGS_PER_OPCACHE_LINE = 8;
    static const unsigned OPCACHE_ENTRY_COUNT = 512;

    struct op_cache_line {
        // contains the tag, dirty bit and valid bit
        boost::uint32_t key;

        // cache line data array
        boost::uint32_t lw[LONGS_PER_OPCACHE_LINE];
    };

    static const unsigned LONGS_PER_INSTCACHE_LINE = 8;
    static const unsigned INSTCACHE_ENTRY_COUNT = 256;

    // the valid bit of the instruction cache keys
    static const unsigned INSTCACHE_KEY_VALID_SHIFT = 0;
    static const unsigned INSTCACHE_KEY_VALID_MASK = 1 << INSTCACHE_KEY_VALID_SHIFT;

    // 19-bit tag of the instruction cache keys
    static const unsigned INSTCACHE_KEY_TAG_SHIFT = 1;
    static const unsigned INSTCACHE_KEY_TAG_MASK = 0x7ffff << INSTCACHE_KEY_TAG_SHIFT;

    struct inst_cache_line {
        // contains the tag and valid bit
        boost::uint32_t key;

        // cache line instruction array
        boost::uint32_t lw[LONGS_PER_INSTCACHE_LINE];
    };

    // 8 KB instruction cache
    struct inst_cache_line *inst_cache;

    // 16 KB ("Operand Cache" in the hardware manual)
    struct op_cache_line *op_cache;

    /*
     * Return true if line matches paddr; else return false.
     *
     * This function does not verify that the cache is enabled; nor does it
     * verify that paddr is even in an area which can be cached.  The callee
     * should do that before calling this function.
     *
     * This function does not check the valid bit.
     */
    bool op_cache_check(struct op_cache_line const *line, addr32_t paddr);

    /*
     * returns the index into the op-cache where paddr
     * would go if it had an entry.
     */
    addr32_t op_cache_selector(addr32_t paddr) const;

    /*
     * returns the index into the inst-cache where paddr
     * would go if it had an entry.
     */
    addr32_t inst_cache_selector(addr32_t paddr) const;

    /*
     * Return true if line matches paddr; else return false.
     *
     * This function does not verify that the cache is enabled; nor does it
     * verify that paddr is even in an area which can be cached.  The callee
     * should do that before calling this function.
     *
     * This function does not check the valid bit.
     */
    bool inst_cache_check(struct inst_cache_line const *line, addr32_t paddr);

    // Returns: zero on success, nonzero on failure.
    int op_cache_read4(boost::uint32_t *out, addr32_t paddr);

    // Returns: zero on success, nonzero on failur.
    int inst_cache_read4(boost::uint32_t *out, addr32_t paddr);

    /*
     * Write the 4-byte value pointed to by data to memory through the cache in
     * copy-back mode.
     * Returns: zero on success, nonzero on failure.
     */
    int op_cache_write4_cb(boost::uint32_t const *data, addr32_t paddr);

    /*
     * Write the 4-byte value pointed to by data to memory through the cache in
     * write-through mode.
     * Returns: zero on success, nonzero on failure.
     */
    int op_cache_write4_wt(boost::uint32_t const *data, addr32_t paddr);

    /*
     * Load the cache-line corresponding to paddr into line.
     * Returns non-zero on failure.
     */
    int op_cache_load(struct op_cache_line *line, addr32_t paddr);

    /*
     * Load the cache-line corresponding to paddr into line.
     * Returns non-zero on failure.
     */
    int inst_cache_load(struct inst_cache_line *line, addr32_t paddr);

    /*
     * Write the cache-line into memory and clear its dirty-bit.
     * returns non-zero on failure.
     *
     * paddr should be an address that falls within the cache-line.
     * It is needed because the entry selector is not saved within the
     * cache-line (although there are enough unused bits that this *may* be
     * possible to implement), so the paddr is the only way to figure out where
     * exactly this line goes in memory.
     */
    int op_cache_write_back(struct op_cache_line *line, addr32_t paddr);

    static addr32_t
    op_cache_line_get_tag(struct op_cache_line const *line);

    static addr32_t
    inst_cache_line_get_tag(struct inst_cache_line const *line);

    // sets the line's tag to tag.
    void op_cache_line_set_tag(struct op_cache_line *line,
                               addr32_t tag);
    // sets the line's tag to tag.
    void inst_cache_line_set_tag(struct inst_cache_line *line,
                                 addr32_t tag);


    // extract the tag from the upper 19 bits of the lower 29 bits of paddr
    static addr32_t op_cache_tag_from_paddr(addr32_t paddr);

    // extract the tag from the upper 19 bits of the lower 29 bits of paddr
    static addr32_t inst_cache_tag_from_paddr(addr32_t paddr);
};

inline Sh4::addr32_t
Sh4::op_cache_line_get_tag(struct op_cache_line const *line) {
    return (OPCACHE_KEY_TAG_MASK & line->key) >> OPCACHE_KEY_TAG_SHIFT;
}

inline Sh4::addr32_t
Sh4::inst_cache_line_get_tag(struct inst_cache_line const *line) {
    return (INSTCACHE_KEY_TAG_MASK & line->key) >> INSTCACHE_KEY_TAG_SHIFT;
}

inline Sh4::addr32_t Sh4::op_cache_tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

inline Sh4::addr32_t Sh4::inst_cache_tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

inline void Sh4::op_cache_line_set_tag(struct op_cache_line *line,
                                       addr32_t tag) {
    line->key &= ~OPCACHE_KEY_TAG_MASK;
    line->key |= tag << OPCACHE_KEY_TAG_SHIFT;
}

inline void Sh4::inst_cache_line_set_tag(struct inst_cache_line *line,
                                         addr32_t tag) {
    line->key &= ~INSTCACHE_KEY_TAG_MASK;
    line->key |= tag << INSTCACHE_KEY_TAG_SHIFT;
}

#endif
