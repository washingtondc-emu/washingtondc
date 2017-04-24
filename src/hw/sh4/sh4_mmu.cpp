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

#include "sh4_reg.h"
#include "sh4_excp.h"
#include "error.h"
#include "MemoryMap.hpp"

#include "sh4.hpp"

#ifdef ENABLE_SH4_MMU

extern "C"
void sh4_mmu_init(Sh4 *sh4) {
    memset(&sh4->mmu, 0, sizeof(sh4->mmu));
}

extern "C"
addr32_t sh4_utlb_ent_get_vpn(struct sh4_utlb_entry const *ent) {
    switch ((ent->ent & SH4_UTLB_ENT_SZ_MASK) >> SH4_UTLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // upper 22 bits
        return ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
    case SH4_MMU_FOUR_KILO:
        // upper 20 bits
        return ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfffff000;
    case SH4_MMU_SIXTYFOUR_KILO:
        // upper 16 bits
        return ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xffff0000;
    case SH4_MMU_ONE_MEGA:
        // upper 12 bits
        return ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfff00000;
    default:
        error_set_param_name("UTLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_utlb_ent_get_addr_offset(struct sh4_utlb_entry const *ent,
                                      addr32_t addr) {
    switch ((ent->ent & SH4_UTLB_ENT_SZ_MASK) >> SH4_UTLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // lower 10 bits
        return addr & 0x3ff;
    case SH4_MMU_FOUR_KILO:
        // lower 12 bits
        return addr & 0xfff;
    case SH4_MMU_SIXTYFOUR_KILO:
        // lower 16 bits
        return addr & 0xffff;
    case SH4_MMU_ONE_MEGA:
        // lowr 20 bits
        return addr & 0xfffff;
    default:
        error_set_param_name("UTLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_utlb_ent_get_ppn(struct sh4_utlb_entry const *ent) {
    switch ((ent->ent & SH4_UTLB_ENT_SZ_MASK) >> SH4_UTLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // upper 19 bits (of upper 29 bits)
        return ent->ent & SH4_UTLB_ENT_PPN_MASK & 0xfffffc00;
    case SH4_MMU_FOUR_KILO:
        // upper 17 bits (of upper 29 bits)
        return ent->ent & SH4_UTLB_ENT_PPN_MASK & 0xfffff000;
    case SH4_MMU_SIXTYFOUR_KILO:
        // upper 13 bits (of upper 29 bits)
        return ent->ent & SH4_UTLB_ENT_PPN_MASK & 0xffff0000;
    case SH4_MMU_ONE_MEGA:
        // upper 9 bits (of upper 29 bits)
        return ent->ent & SH4_UTLB_ENT_PPN_MASK & 0xfff00000;
    default:
        error_set_param_name("UTLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

/*
 * TODO: if you look deep into the way this function and the functions it
 *       calls work, it becomes apparent that the exact same switch statement
 *       gets done 3 times in a row (suboptimal branching).
 */
extern "C"
addr32_t sh4_utlb_ent_translate(struct sh4_utlb_entry const *ent,
                                addr32_t vaddr) {
    addr32_t ppn = sh4_utlb_ent_get_ppn(ent);
    addr32_t offset = sh4_utlb_ent_get_addr_offset(ent, vaddr);

    switch ((ent->ent & SH4_UTLB_ENT_SZ_MASK) >> SH4_UTLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        return ppn | offset;
    case SH4_MMU_FOUR_KILO:
        return ppn | offset;
    case SH4_MMU_SIXTYFOUR_KILO:
        return ppn | offset;
    case SH4_MMU_ONE_MEGA:
        return ppn | offset;
    default:
        error_set_param_name("UTLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_itlb_ent_get_vpn(struct sh4_itlb_entry const *ent) {
    switch ((ent->ent & SH4_ITLB_ENT_SZ_MASK) >> SH4_ITLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // upper 22 bits
        return ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
    case SH4_MMU_FOUR_KILO:
        // upper 20 bits
        return ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfffff000;
    case SH4_MMU_SIXTYFOUR_KILO:
        // upper 16 bits
        return ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xffff0000;
    case SH4_MMU_ONE_MEGA:
        // upper 12 bits
        return ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfff00000;
    default:
        error_set_param_name("ITLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_itlb_ent_get_ppn(struct sh4_itlb_entry const *ent) {
    switch ((ent->ent & SH4_ITLB_ENT_SZ_MASK) >> SH4_ITLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // upper 19 bits (of upper 29 bits)
        return ((ent->ent & SH4_ITLB_ENT_PPN_MASK) >> SH4_ITLB_ENT_PPN_SHIFT) &
            0x1ffffc00;
    case SH4_MMU_FOUR_KILO:
        // upper 17 bits (of upper 29 bits)
        return ((ent->ent & SH4_ITLB_ENT_PPN_MASK) >> SH4_ITLB_ENT_PPN_SHIFT) &
            0x1ffff000;
    case SH4_MMU_SIXTYFOUR_KILO:
        // upper 13 bits (of upper 29 bits)
        return ((ent->ent & SH4_ITLB_ENT_PPN_MASK) >> SH4_ITLB_ENT_PPN_SHIFT) &
            0x1fff0000;
    case SH4_MMU_ONE_MEGA:
        // upper 9 bits (of upper 29 bits)
        return ((ent->ent & SH4_ITLB_ENT_PPN_MASK) >> SH4_ITLB_ENT_PPN_SHIFT) &
            0x1ff00000;
    default:
        error_set_param_name("ITLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_itlb_ent_get_addr_offset(struct sh4_itlb_entry const *ent,
                                      addr32_t addr) {
    switch ((ent->ent & SH4_ITLB_ENT_SZ_MASK) >> SH4_ITLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        // lower 10 bits
        return addr & 0x3ff;
    case SH4_MMU_FOUR_KILO:
        // lower 12 bits
        return addr & 0xfff;
    case SH4_MMU_SIXTYFOUR_KILO:
        // lower 16 bits
        return addr & 0xffff;
    case SH4_MMU_ONE_MEGA:
        // lowr 20 bits
        return addr & 0xfffff;
    default:
        error_set_param_name("ITLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}

extern "C"
addr32_t sh4_itlb_ent_translate(struct sh4_itlb_entry const *ent,
                                addr32_t vaddr) {
    addr32_t ppn = sh4_itlb_ent_get_ppn(ent);
    addr32_t offset = sh4_itlb_ent_get_addr_offset(ent, vaddr);

    switch ((ent->ent & SH4_ITLB_ENT_SZ_MASK) >> SH4_ITLB_ENT_SZ_SHIFT) {
    case SH4_MMU_ONE_KILO:
        return ppn << 10 | offset;
    case SH4_MMU_FOUR_KILO:
        return ppn << 12 | offset;
    case SH4_MMU_SIXTYFOUR_KILO:
        return ppn << 16 | offset;
    case SH4_MMU_ONE_MEGA:
        return ppn << 20 | offset;
    default:
        error_set_param_name("ITLB size value");
        RAISE_ERROR(ERROR_INVALID_PARAM);
        return 0; // never happens
    }
}


// find entry with matching VPN
extern "C"
struct sh4_utlb_entry *sh4_utlb_search(Sh4 *sh4, addr32_t vaddr,
                                       sh4_utlb_access_t access_type) {
    struct sh4_utlb_entry *ret = NULL;
    addr32_t vpn_vaddr;
    struct sh4_mmu *mmu = &sh4->mmu;

    for (unsigned i = 0; i < SH4_UTLB_SIZE; i++) {
        struct sh4_utlb_entry *ent = mmu->utlb + i;
        addr32_t vpn_ent;

        switch ((ent->ent & SH4_UTLB_ENT_SZ_MASK) >> SH4_UTLB_ENT_SZ_SHIFT) {
        case SH4_MMU_ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vpn_ent = ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case SH4_MMU_FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vpn_ent = ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SH4_MMU_SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vpn_ent = ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case SH4_MMU_ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vpn_ent = ((ent->key & SH4_UTLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            error_set_param_name("UTLB size value");
            RAISE_ERROR(ERROR_INVALID_PARAM);
            return 0; // never happens
        }

        if (!(SH4_UTLB_ENT_SH_MASK & ent->ent) &&
            (!(sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SV_MASK) ||
             !(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)

            unsigned utlb_asid = (ent->key & SH4_UTLB_KEY_ASID_MASK) >>
                SH4_UTLB_KEY_ASID_SHIFT;
            unsigned mmu_asid =
                (sh4->reg[SH4_REG_PTEH] & SH4_MMUPTEH_ASID_MASK) >>
                SH4_MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & SH4_UTLB_KEY_VALID_MASK) &&
                utlb_asid == mmu_asid) {
                // UTLB hit
                if (ret) {
                    sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & SH4_UTLB_KEY_VALID_MASK)) {
                // UTLB hit
                if (ret) {
                    sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        }
    }

    /*
     * TODO: Make sure the vpn is being set properly for
     *       SH4_UTLB_READ and SH4_UTLB_WRITE below.  I wonder if I am confused because
     *       it seems weird to me that different VPN pages can have different
     *       sizes.
     */
    if (!ret) {
        switch (access_type) {
        case SH4_UTLB_READ:
            sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_READ_MISS);
            sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
            sh4->reg[SH4_REG_PTEH] |= vpn_vaddr << SH4_MMUPTEH_VPN_SHIFT;
            sh4->reg[SH4_REG_PTEA] = vaddr;
            return NULL;
        case SH4_UTLB_WRITE:
            sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_WRITE_MISS);
            sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
            sh4->reg[SH4_REG_PTEH] |= vpn_vaddr << SH4_MMUPTEH_VPN_SHIFT;
            sh4->reg[SH4_REG_PTEA] = vaddr;
            return NULL;
        case SH4_UTLB_READ_ITLB:
            return NULL;
        default:
            error_set_param_name("Unknown access type in utlb_search");
            RAISE_ERROR(ERROR_INVALID_PARAM);
        }
    }

    return ret;
}

extern "C"
struct sh4_itlb_entry *sh4_itlb_search(struct Sh4 *sh4, addr32_t vaddr) {
    struct sh4_itlb_entry *ret = NULL;
    addr32_t vpn_vaddr;
    struct sh4_mmu *mmu = &sh4->mmu;

    for (unsigned i = 0; i < SH4_ITLB_SIZE; i++) {
        struct sh4_itlb_entry *ent = mmu->itlb + i;
        addr32_t vpn_ent;

        switch ((ent->ent & SH4_ITLB_ENT_SZ_MASK) >> SH4_ITLB_ENT_SZ_SHIFT) {
        case SH4_MMU_ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vpn_ent = ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case SH4_MMU_FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vpn_ent = ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SH4_MMU_SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vpn_ent = ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case SH4_MMU_ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vpn_ent = ((ent->key & SH4_ITLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            error_set_param_name("ITLB size value");
            RAISE_ERROR(ERROR_INVALID_PARAM);
        }

        if (!(SH4_ITLB_ENT_SH_MASK & ent->ent) &&
            (!(sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SV_MASK) ||
             !(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)
            unsigned itlb_asid = (ent->key & SH4_ITLB_KEY_ASID_MASK) >>
                SH4_ITLB_KEY_ASID_SHIFT;
            unsigned mmu_asid =
                (sh4->reg[SH4_REG_PTEH] & SH4_MMUPTEH_ASID_MASK) >>
                SH4_MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & SH4_ITLB_KEY_VALID_MASK) &&
                itlb_asid == mmu_asid) {
                // ITLB hit
                if (ret) {
                    /*
                     *  TODO: set_exception may be setting more flags than is
                     *        necessary in this scenario; the manual is a
                     *        little vague on how this is supposed to work.
                     */
                    sh4->reg[SH4_REG_TEA] = vaddr;
                    sh4_set_exception(sh4, SH4_EXCP_INST_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & SH4_ITLB_KEY_VALID_MASK)) {
                // ITLB hit
                if (ret) {
                    /*
                     *  TODO: set_exception may be setting more flags than is
                     *        necessary in this scenario; the manual is a
                     *        little vague on how this is supposed to work.
                     */
                    sh4->reg[SH4_REG_TEA] = vaddr;
                    sh4_set_exception(sh4, SH4_EXCP_INST_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        }
    }

    if (ret)
        return ret;

    // ITLB miss - check the UTLB
    struct sh4_utlb_entry *utlb_ent;
    utlb_ent = sh4_utlb_search(sh4, vaddr, SH4_UTLB_READ_ITLB);

    if (!utlb_ent) {
        sh4_set_exception(sh4, SH4_EXCP_INST_TLB_MISS);
        sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
        sh4->reg[SH4_REG_PTEH] |= vpn_vaddr << SH4_MMUPTEH_VPN_SHIFT;
        sh4->reg[SH4_REG_TEA] = vaddr;
        return NULL;
    }

    // now replace one of the ITLB entries.  Ideally there would be some sort
    // of Least-Recently-Used algorithm here.
    unsigned which = vaddr & (4 - 1);

    // the key formats are exactly the same, so this is safe.
    mmu->itlb[which].key = utlb_ent->key;

    // Notice how the PR gets AND'd with 2.  That is because the ITLB version of
    // PR is only 1 bit, while the UTLB version of PR is two bits.  ITLB's PR
    // corresponds to the upper bit of UTLB's PR.
    mmu->itlb[which].ent = 0;
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_PPN_MASK) >>
                             SH4_UTLB_ENT_PPN_SHIFT) << SH4_ITLB_ENT_PPN_SHIFT;
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_SZ_MASK) >>
                             SH4_UTLB_ENT_SZ_SHIFT) << SH4_ITLB_ENT_SZ_SHIFT;
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_SH_MASK) >>
                             SH4_UTLB_ENT_SH_SHIFT) << SH4_ITLB_ENT_SH_SHIFT;
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_C_MASK) >>
                             SH4_UTLB_ENT_C_SHIFT) << SH4_ITLB_ENT_C_SHIFT;
    mmu->itlb[which].ent |= ((((utlb_ent->ent & SH4_UTLB_ENT_PR_MASK) >>
                               SH4_UTLB_ENT_PR_SHIFT) & 2) << SH4_ITLB_ENT_PR_SHIFT);
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_SA_MASK) >>
                             SH4_UTLB_ENT_SA_SHIFT) << SH4_ITLB_ENT_SA_SHIFT;
    mmu->itlb[which].ent |= ((utlb_ent->ent & SH4_UTLB_ENT_TC_MASK) >>
                             SH4_UTLB_ENT_TC_SHIFT) << SH4_ITLB_ENT_TC_SHIFT;

    /*
     * The SH7750 Hardware Manual says to loop back to the beginning (see the
     * flowchart on page 44), so I implement that by recursing back into this
     * function.  Some sort of infinite-recursion detection may be warranted
     * here just in case.
     */
    return sh4_itlb_search(sh4, vaddr);
}

extern "C"
int sh4_mmu_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len) {
    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

    struct sh4_utlb_entry *utlb_ent = sh4_utlb_search(sh4, addr,
                                                      SH4_UTLB_READ);

    if (!utlb_ent)
        return 1; // exception set by sh4_utlb_search

    unsigned pr = (utlb_ent->ent & SH4_UTLB_ENT_PR_MASK) >>
        SH4_UTLB_ENT_PR_SHIFT;

    addr32_t paddr = sh4_utlb_ent_translate(utlb_ent, addr);
    if (!privileged && !(pr & 2)) {
        // we don't have permissions
        unsigned vpn = (utlb_ent->key & SH4_UTLB_KEY_VPN_MASK) >>
            SH4_UTLB_KEY_VPN_SHIFT;
        sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL);
        sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
        sh4->reg[SH4_REG_PTEH] |= vpn << SH4_MMUPTEH_VPN_SHIFT;
        sh4->reg[SH4_REG_TEA] = addr;
        return 1;
    }

    // handle the case where OCE is enabled and ORA is
    // enabled but we don't have Ocache available
    if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
        (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
        sh4_ocache_in_ram_area(paddr)) {
        sh4_ocache_do_read_ora(sh4, data, paddr, len);
        return 0;
    }

    // don't use the cache
    return memory_map_read(data, paddr & 0x1fffffff, len);
}

extern "C"
int sh4_mmu_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {
    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

    struct sh4_utlb_entry *utlb_ent = sh4_utlb_search(sh4, addr,
                                                      SH4_UTLB_WRITE);

    if (!utlb_ent)
        return 1; // exception set by sh4_utlb_search

    unsigned pr = (utlb_ent->ent & SH4_UTLB_ENT_PR_MASK) >>
        SH4_UTLB_ENT_PR_SHIFT;

    addr32_t paddr = sh4_utlb_ent_translate(utlb_ent, addr);
    if (privileged) {
        if (pr & 1) {
            // page is marked as read-write

            if (utlb_ent->ent & SH4_UTLB_ENT_D_MASK) {
                // handle the case where OCE is enabled and ORA is
                // enabled but we don't have Ocache available
                if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                    (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                    sh4_ocache_in_ram_area(paddr)) {
                    sh4_ocache_do_write_ora(sh4, data, paddr, len);
                    return 0;
                }
                return memory_map_write(data, paddr & 0x1fffffff,
                                        len);
            } else {
                sh4_set_exception(sh4, SH4_EXCP_INITIAL_PAGE_WRITE);
                sh4->reg[SH4_REG_TEA] = addr;
                return 1;
            }
        } else {
            // page is marked as read-only
            unsigned vpn = (utlb_ent->key & SH4_UTLB_KEY_VPN_MASK) >>
                SH4_UTLB_KEY_VPN_SHIFT;
            sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL);
            sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
            sh4->reg[SH4_REG_PTEH] |= vpn << SH4_MMUPTEH_VPN_SHIFT;
            sh4->reg[SH4_REG_TEA] = addr;
            return 1;
        }
    } else {
        if (pr != 3) {
            // page is marked as read-only OR we don't have permissions
            unsigned vpn = (utlb_ent->key & SH4_UTLB_KEY_VPN_MASK) >>
                SH4_UTLB_KEY_VPN_SHIFT;
            sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL);
            sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
            sh4->reg[SH4_REG_PTEH] |= vpn << SH4_MMUPTEH_VPN_SHIFT;
            sh4->reg[SH4_REG_TEA] = addr;
            return 1;
        }

        if (utlb_ent->ent & SH4_UTLB_ENT_D_MASK) {
            // handle the case where OCE is enabled and ORA is
            // enabled but we don't have Ocache available
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                sh4_ocache_in_ram_area(paddr)) {
                sh4_ocache_do_write_ora(sh4, data, paddr, len);
                return 0;
            }

            // don't use the cache
            return memory_map_write(data, paddr & 0x1fffffff, len);
        } else {
            sh4_set_exception(sh4, SH4_EXCP_INITIAL_PAGE_WRITE);
            sh4->reg[SH4_REG_TEA] = addr;
            return 1;
        }
    }

    error_set_wtf("I don't believe it should be possible to get here");
    RAISE_ERROR(ERROR_INTEGRITY);
}

extern "C"
int sh4_mmu_read_inst(Sh4 *sh4, inst_t *out, addr32_t addr) {
    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

    struct sh4_itlb_entry *itlb_ent = sh4_itlb_search(sh4, addr);

    if (!itlb_ent)
        return 1;  // exception set by sh4_itlb_search

    if (privileged || (itlb_ent->ent & SH4_ITLB_ENT_PR_MASK)) {
        addr32_t paddr = sh4_itlb_ent_translate(itlb_ent, addr);

        // don't use the cache
        return memory_map_read(out, paddr & 0x1fffffff,
                               sizeof(*out));
    }

    // we don't have permissions
    unsigned vpn = (itlb_ent->key & SH4_ITLB_KEY_VPN_MASK) >>
        SH4_ITLB_KEY_VPN_SHIFT;
    sh4_set_exception(sh4, SH4_EXCP_INST_TLB_PROT_VIOL);
    sh4->reg[SH4_REG_PTEH] &= ~SH4_MMUPTEH_VPN_MASK;
    sh4->reg[SH4_REG_PTEH] |= vpn << SH4_MMUPTEH_VPN_SHIFT;
    sh4->reg[SH4_REG_TEA] = addr;
    return 1;
}

#endif
