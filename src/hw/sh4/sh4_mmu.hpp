/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef SH4_MMU_HPP_
#define SH4_MMU_HPP_

#include <boost/cstdint.hpp>

#include "sh4_reg.hpp"

static const unsigned SH4_MMUPTEH_ASID_SHIFT = 0;
static const unsigned SH4_MMUPTEH_ASID_MASK = 0xff << SH4_MMUPTEH_ASID_SHIFT;

static const unsigned SH4_MMUPTEH_VPN_SHIFT = 10;
static const unsigned SH4_MMUPTEH_VPN_MASK = 0x3fffff << SH4_MMUPTEH_VPN_SHIFT;

static const unsigned SH4_MMUCR_AT_SHIFT = 0;
static const unsigned SH4_MMUCR_AT_MASK = 1 << SH4_MMUCR_AT_SHIFT;

static const unsigned SH4_MMUCR_TI_SHIFT = 2;
static const unsigned SH4_MMUCR_TI_MASK = 1 << SH4_MMUCR_TI_SHIFT;

// Single (=1)/Multiple(=0) Virtual Memory switch bit
static const unsigned SH4_MMUCR_SV_SHIFT = 8;
static const unsigned SH4_MMUCR_SV_MASK = 1 << SH4_MMUCR_SV_SHIFT;

static const unsigned SH4_MMUCR_SQMD_SHIFT = 9;
static const unsigned SH4_MMUCR_SQMD_MASK = 1 << SH4_MMUCR_SQMD_SHIFT;

static const unsigned SH4_MMUCR_URC_SHIFT = 10;
static const unsigned SH4_MMUCR_URC_MASK = 0x3f << SH4_MMUCR_URC_SHIFT;

static const unsigned SH4_MMUCR_URB_SHIFT = 18;
static const unsigned SH4_MMUCR_URB_MASK = 0x3f << SH4_MMUCR_URB_SHIFT;

static const unsigned SH4_MMUCR_LRUI_SHIFT = 26;
static const unsigned SH4_MMUCR_LRUI_MASK = 0x3f << SH4_MMUCR_LRUI_SHIFT;

// UTLB Valid bit
static const unsigned SH4_UTLB_KEY_VALID_SHIFT = 0;
static const unsigned SH4_UTLB_KEY_VALID_MASK = 1 << SH4_UTLB_KEY_VALID_SHIFT;

// UTLB Virtual Page Number
static const unsigned SH4_UTLB_KEY_VPN_SHIFT = 1;
static const unsigned SH4_UTLB_KEY_VPN_MASK = 0x3fffff << SH4_UTLB_KEY_VPN_SHIFT;

// UTLB Address-Space Identifier
static const unsigned SH4_UTLB_KEY_ASID_SHIFT = 23;
static const unsigned SH4_UTLB_KEY_ASID_MASK = 0xff << SH4_UTLB_KEY_ASID_SHIFT;

// UTLB Timing Control - I have no idea what this is
// (see page 41 of the sh7750 hardware manual)
static const unsigned SH4_UTLB_ENT_TC_SHIFT = 0;
static const unsigned SH4_UTLB_ENT_TC_MASK = 1 << SH4_UTLB_ENT_TC_SHIFT;

// UTLB Space Attribute
static const unsigned SH4_UTLB_ENT_SA_SHIFT = 1;
static const unsigned SH4_UTLB_ENT_SA_MASK = 0x7 << SH4_UTLB_ENT_SA_SHIFT;

// UTLB Write-Through
static const unsigned SH4_UTLB_ENT_WT_SHIFT = 4;
static const unsigned SH4_UTLB_ENT_WT_MASK = 1 << SH4_UTLB_ENT_WT_SHIFT;

// UTLB Dirty Bit
static const unsigned SH4_UTLB_ENT_D_SHIFT = 5;
static const unsigned SH4_UTLB_ENT_D_MASK = 1 << SH4_UTLB_ENT_D_SHIFT;

// UTLB Protection-Key data
static const unsigned SH4_UTLB_ENT_PR_SHIFT = 6;
static const unsigned SH4_UTLB_ENT_PR_MASK = 3 << SH4_UTLB_ENT_PR_SHIFT;

// UTLB Cacheability bit
static const unsigned SH4_UTLB_ENT_C_SHIFT = 8;
static const unsigned SH4_UTLB_ENT_C_MASK = 1 << SH4_UTLB_ENT_C_SHIFT;

// UTLB Share status bit
static const unsigned SH4_UTLB_ENT_SH_SHIFT = 9;
static const unsigned SH4_UTLB_ENT_SH_MASK = 1 << SH4_UTLB_ENT_SH_SHIFT;

// UTLB Page size (see enum PageSize definition)
static const unsigned SH4_UTLB_ENT_SZ_SHIFT = 10;
static const unsigned SH4_UTLB_ENT_SZ_MASK = 3 << SH4_UTLB_ENT_SZ_SHIFT;

// UTLB Physical Page Number
static const unsigned SH4_UTLB_ENT_PPN_SHIFT = 12;
static const unsigned SH4_UTLB_ENT_PPN_MASK = 0x7ffff << SH4_UTLB_ENT_PPN_SHIFT;

// ITLB Valid bit
static const unsigned SH4_ITLB_KEY_VALID_SHIFT = 0;
static const unsigned SH4_ITLB_KEY_VALID_MASK = 1 << SH4_ITLB_KEY_VALID_SHIFT;

// ITLB Virtual Page Number
static const unsigned SH4_ITLB_KEY_VPN_SHIFT = 1;
static const unsigned SH4_ITLB_KEY_VPN_MASK = 0x3fffff << SH4_ITLB_KEY_VPN_SHIFT;

// ITLB Address-Space Identifier
static const unsigned SH4_ITLB_KEY_ASID_SHIFT = 23;
static const unsigned SH4_ITLB_KEY_ASID_MASK = 0xff << SH4_ITLB_KEY_ASID_SHIFT;

// ITLB Timing Control - I have no idea what this is
// (see page 41 of the sh7750 hardware manual)
static const unsigned SH4_ITLB_ENT_TC_SHIFT = 0;
static const unsigned SH4_ITLB_ENT_TC_MASK = 1 << SH4_ITLB_ENT_TC_SHIFT;

// ITLB Space Attribute
static const unsigned SH4_ITLB_ENT_SA_SHIFT = 1;
static const unsigned SH4_ITLB_ENT_SA_MASK = 0x7 << SH4_ITLB_ENT_SA_SHIFT;

// ITLB Protection Key data (0=priveleged, 1=user or priveleged)
static const unsigned SH4_ITLB_ENT_PR_SHIFT = 4;
static const unsigned SH4_ITLB_ENT_PR_MASK = 1 << SH4_ITLB_ENT_PR_SHIFT;

// ITLB Cacheability flag
static const unsigned SH4_ITLB_ENT_C_SHIFT = 5;
static const unsigned SH4_ITLB_ENT_C_MASK = 1 << SH4_ITLB_ENT_C_SHIFT;

// ITLB Share status Bit
static const unsigned SH4_ITLB_ENT_SH_SHIFT = 6;
static const unsigned SH4_ITLB_ENT_SH_MASK = 1 << SH4_ITLB_ENT_SH_SHIFT;

// ITLB Page size (see enum PageSize definition)
static const unsigned SH4_ITLB_ENT_SZ_SHIFT = 7;
static const unsigned SH4_ITLB_ENT_SZ_MASK = 0x3 << SH4_ITLB_ENT_SZ_SHIFT;

// ITLB Physical Page Number
static const unsigned SH4_ITLB_ENT_PPN_SHIFT = 9;
static const unsigned SH4_ITLB_ENT_PPN_MASK = 0x7ffff << SH4_ITLB_ENT_PPN_SHIFT;

enum PageSize {
    SH4_MMU_ONE_KILO = 0,
    SH4_MMU_FOUR_KILO = 1,
    SH4_MMU_SIXTYFOUR_KILO = 2,
    SH4_MMU_ONE_MEGA = 3
};

struct sh4_utlb_entry {
    uint32_t key;
    uint32_t ent;
};

struct sh4_itlb_entry {
    uint32_t key;
    uint32_t ent;
};

static const size_t SH4_UTLB_SIZE = 64;
static const size_t SH4_ITLB_SIZE = 4;

struct sh4_mmu {
#ifdef ENABLE_SH4_MMU
    struct sh4_utlb_entry utlb[SH4_UTLB_SIZE];
    struct sh4_itlb_entry itlb[SH4_ITLB_SIZE];
#endif
};

/*
 * Parameter to sh4_utlb_search that tells it what kind of exception to raise
 * in the event of a sh4_utlb miss.  This does not have any effect on what it
 * does for a multiple hit (which is to raise EXCP_DATA_TLB_MULT_HIT).  Even
 * SH4_UTLB_READ_ITLB doesn not stop it from raising EXCP_DATA_TLB_MULT_HIT if
 * there is a multiple-hit.
 */
typedef enum sh4_utlb_access {
    SH4_UTLB_READ,     // generate EXCP_DATA_TLB_READ_MISS
    SH4_UTLB_WRITE,    // generate EXCP_DATA_TLB_WRITE_MISS
    SH4_UTLB_READ_ITLB // do not generate exceptions for TLB misses
} sh4_utlb_access_t;

/*
 * return the sh4_utlb entry for vaddr.
 * On failure, this will return NULL and set the appropriate CPU
 * flags to signal an exception of some sort.
 *
 * access_type should be either SH4_UTLB_READ, SH4_UTLB_WRITE or
 * SH4_UTLB_READ_ITLB.
 * It is only used for setting the appropriate exception type in the event
 * of a sh4_utlb cache miss.  Other than that, it has no real effect on what
 * this function does.
 *
 * This function does not check to see if the CPU actually has privelege to
 * access the page referenced by the returned sh4_utlb_entry.
 */
struct sh4_utlb_entry *sh4_utlb_search(Sh4 *sh4, addr32_t vaddr,
                                       sh4_utlb_access_t access_type);

addr32_t sh4_utlb_ent_get_vpn(struct sh4_utlb_entry const *ent);
addr32_t sh4_utlb_ent_get_ppn(struct sh4_utlb_entry const *ent);
addr32_t sh4_utlb_ent_get_addr_offset(struct sh4_utlb_entry const *ent,
                                  addr32_t addr);
addr32_t sh4_utlb_ent_translate(struct sh4_utlb_entry const *ent,
                                addr32_t vaddr);

addr32_t sh4_itlb_ent_get_vpn(struct sh4_itlb_entry const *ent);
addr32_t sh4_itlb_ent_get_ppn(struct sh4_itlb_entry const *ent);
addr32_t sh4_itlb_ent_get_addr_offset(struct sh4_itlb_entry const *ent,
                                      addr32_t addr);
addr32_t sh4_itlb_ent_translate(struct sh4_itlb_entry const *ent,
                                addr32_t vaddr);

/*
 * Return the sh4_itlb entry for vaddr.
 * On failure this will return NULL and set the appropriate CPU
 * flags to signal an exception of some sort.
 * On miss, this function will search the sh4_utlb and if it finds what it was
 * looking for there, it will replace one of the sh4_itlb entries with the sh4_utlb
 * entry as outlined on pade 44 of the SH7750 Hardware Manual.
 *
 * This function does not check to see if the CPU actually has privelege to
 * access the page referenced by the returned sh4_itlb_entry.
 */
struct sh4_itlb_entry *sh4_itlb_search(Sh4 *sh4, addr32_t vaddr);

void sh4_mmu_init(Sh4 *sh4);

/* memory-mapped register read/write handlers for MMUCR */
int Sh4MmucrRegReadHandler(Sh4 *sh4, void *buf,
                           struct Sh4MemMappedReg const *reg_info);
int Sh4MmucrRegWriteHandler(Sh4 *sh4, void const *buf,
                            struct Sh4MemMappedReg const *reg_info);


#endif
