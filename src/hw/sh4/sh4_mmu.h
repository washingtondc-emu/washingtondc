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

#ifndef SH4_MMU_H_
#define SH4_MMU_H_

#include <stdint.h>
#include <stddef.h>

#include "sh4_reg.h"
#include "sh4_reg_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH4_MMUPTEH_ASID_SHIFT 0
#define SH4_MMUPTEH_ASID_MASK (0xff << SH4_MMUPTEH_ASID_SHIFT)

#define SH4_MMUPTEH_VPN_SHIFT 10
#define SH4_MMUPTEH_VPN_MASK (0x3fffff << SH4_MMUPTEH_VPN_SHIFT)

// UTLB Valid bit
#define SH4_UTLB_KEY_VALID_SHIFT 0
#define SH4_UTLB_KEY_VALID_MASK (1 << SH4_UTLB_KEY_VALID_SHIFT)

// UTLB Virtual Page Number
#define SH4_UTLB_KEY_VPN_SHIFT 1
#define SH4_UTLB_KEY_VPN_MASK (0x3fffff << SH4_UTLB_KEY_VPN_SHIFT)

// UTLB Address-Space Identifier
#define SH4_UTLB_KEY_ASID_SHIFT 23
#define SH4_UTLB_KEY_ASID_MASK (0xff << SH4_UTLB_KEY_ASID_SHIFT)

// UTLB Timing Control - I have no idea what this is
// (see page 41 of the sh7750 hardware manual)
#define SH4_UTLB_ENT_TC_SHIFT 0
#define SH4_UTLB_ENT_TC_MASK (1 << SH4_UTLB_ENT_TC_SHIFT)

// UTLB Space Attribute
#define SH4_UTLB_ENT_SA_SHIFT 1
#define SH4_UTLB_ENT_SA_MASK (0x7 << SH4_UTLB_ENT_SA_SHIFT)

// UTLB Write-Through
#define SH4_UTLB_ENT_WT_SHIFT 4
#define SH4_UTLB_ENT_WT_MASK (1 << SH4_UTLB_ENT_WT_SHIFT)

// UTLB Dirty Bit
#define SH4_UTLB_ENT_D_SHIFT 5
#define SH4_UTLB_ENT_D_MASK (1 << SH4_UTLB_ENT_D_SHIFT)

// UTLB Protection-Key data
#define SH4_UTLB_ENT_PR_SHIFT 6
#define SH4_UTLB_ENT_PR_MASK (3 << SH4_UTLB_ENT_PR_SHIFT)

// UTLB Cacheability bit
#define SH4_UTLB_ENT_C_SHIFT 8
#define SH4_UTLB_ENT_C_MASK (1 << SH4_UTLB_ENT_C_SHIFT)

// UTLB Share status bit
#define SH4_UTLB_ENT_SH_SHIFT 9
#define SH4_UTLB_ENT_SH_MASK (1 << SH4_UTLB_ENT_SH_SHIFT)

// UTLB Page size (see enum PageSize definition)
#define SH4_UTLB_ENT_SZ_SHIFT 10
#define SH4_UTLB_ENT_SZ_MASK (3 << SH4_UTLB_ENT_SZ_SHIFT)

// UTLB Physical Page Number
#define SH4_UTLB_ENT_PPN_SHIFT 12
#define SH4_UTLB_ENT_PPN_MASK (0x7ffff << SH4_UTLB_ENT_PPN_SHIFT)

// ITLB Valid bit
#define SH4_ITLB_KEY_VALID_SHIFT 0
#define SH4_ITLB_KEY_VALID_MASK (1 << SH4_ITLB_KEY_VALID_SHIFT)

// ITLB Virtual Page Number
#define SH4_ITLB_KEY_VPN_SHIFT 1
#define SH4_ITLB_KEY_VPN_MASK (0x3fffff << SH4_ITLB_KEY_VPN_SHIFT)

// ITLB Address-Space Identifier
#define SH4_ITLB_KEY_ASID_SHIFT 23
#define SH4_ITLB_KEY_ASID_MASK (0xff << SH4_ITLB_KEY_ASID_SHIFT)

// ITLB Timing Control - I have no idea what this is
// (see page 41 of the sh7750 hardware manual)
#define SH4_ITLB_ENT_TC_SHIFT 0
#define SH4_ITLB_ENT_TC_MASK (1 << SH4_ITLB_ENT_TC_SHIFT)

// ITLB Space Attribute
#define SH4_ITLB_ENT_SA_SHIFT 1
#define SH4_ITLB_ENT_SA_MASK (0x7 << SH4_ITLB_ENT_SA_SHIFT)

// ITLB Protection Key data (0=priveleged, 1=user or priveleged)
#define SH4_ITLB_ENT_PR_SHIFT 4
#define SH4_ITLB_ENT_PR_MASK (1 << SH4_ITLB_ENT_PR_SHIFT)

// ITLB Cacheability flag
#define SH4_ITLB_ENT_C_SHIFT 5
#define SH4_ITLB_ENT_C_MASK (1 << SH4_ITLB_ENT_C_SHIFT)

// ITLB Share status Bit
#define SH4_ITLB_ENT_SH_SHIFT 6
#define SH4_ITLB_ENT_SH_MASK (1 << SH4_ITLB_ENT_SH_SHIFT)

// ITLB Page size (see enum PageSize definition)
#define SH4_ITLB_ENT_SZ_SHIFT 7
#define SH4_ITLB_ENT_SZ_MASK (0x3 << SH4_ITLB_ENT_SZ_SHIFT)

// ITLB Physical Page Number
#define SH4_ITLB_ENT_PPN_SHIFT 9
#define SH4_ITLB_ENT_PPN_MASK (0x7ffff << SH4_ITLB_ENT_PPN_SHIFT)

enum PageSize {
    SH4_MMU_ONE_KILO = 0,
    SH4_MMU_FOUR_KILO = 1,
    SH4_MMU_SIXTYFOUR_KILO = 2,
    SH4_MMU_ONE_MEGA = 3
};

#ifdef ENABLE_SH4_MMU

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
    struct sh4_utlb_entry utlb[SH4_UTLB_SIZE];
    struct sh4_itlb_entry itlb[SH4_ITLB_SIZE];
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

/*
 * impelements MMU functionaliry of the
 * sh4_read_mem/sh4_write_mem/sh4_read_inst functions for areas P0 and P3
 */
int sh4_mmu_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len);
int sh4_mmu_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len);
int sh4_mmu_read_inst(Sh4 *sh4, inst_t *out, addr32_t addr);

#endif

#ifdef __cplusplus
}
#endif

#endif
