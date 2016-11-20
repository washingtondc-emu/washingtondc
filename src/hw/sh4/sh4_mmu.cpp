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

#include "BaseException.hpp"

#include "sh4.hpp"

addr32_t Sh4::utlb_ent_get_vpn(struct utlb_entry *ent) const {
    switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // upper 22 bits
        return ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
    case FOUR_KILO:
        // upper 20 bits
        return ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffff000;
    case SIXTYFOUR_KILO:
        // upper 16 bits
        return ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xffff0000;
    case ONE_MEGA:
        // upper 12 bits
        return ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfff00000;
    default:
        throw IntegrityError("Unrecognized UTLB size value");
    }
}

addr32_t Sh4::utlb_ent_get_addr_offset(struct utlb_entry *ent,
                                       addr32_t addr) const {
    switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // lower 10 bits
        return addr & 0x3ff;
    case FOUR_KILO:
        // lower 12 bits
        return addr & 0xfff;
    case SIXTYFOUR_KILO:
        // lower 16 bits
        return addr & 0xffff;
    case ONE_MEGA:
        // lowr 20 bits
        return addr & 0xfffff;
    default:
        throw IntegrityError("Unrecognized UTLB size value");
    }
}

addr32_t Sh4::utlb_ent_get_ppn(struct utlb_entry *ent) const {
    switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // upper 19 bits (of upper 29 bits)
        return ((ent->ent & UTLB_ENT_PPN_MASK) >> UTLB_ENT_PPN_SHIFT) &
            0x1ffffc00;
    case FOUR_KILO:
        // upper 17 bits (of upper 29 bits)
        return ((ent->ent & UTLB_ENT_PPN_MASK) >> UTLB_ENT_PPN_SHIFT) &
            0x1ffff000;
    case SIXTYFOUR_KILO:
        // upper 13 bits (of upper 29 bits)
        return ((ent->ent & UTLB_ENT_PPN_MASK) >> UTLB_ENT_PPN_SHIFT) &
            0x1fff0000;
    case ONE_MEGA:
        // upper 9 bits (of upper 29 bits)
        return ((ent->ent & UTLB_ENT_PPN_MASK) >> UTLB_ENT_PPN_SHIFT) &
            0x1ff00000;
    default:
        throw IntegrityError("Unrecognized UTLB size value");
    }
}

/*
 * TODO: if you look deep into the way this function and the functions it
 *       calls work, it becomes apparent that the exact same switch statement
 *       gets done 3 times in a row (suboptimal branching).
 */
addr32_t Sh4::utlb_ent_translate(struct utlb_entry *ent, addr32_t vaddr) const {
    addr32_t ppn = utlb_ent_get_ppn(ent);
    addr32_t offset = utlb_ent_get_addr_offset(ent, vaddr);

    switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        return ppn << 10 | offset;
    case FOUR_KILO:
        return ppn << 12 | offset;
    case SIXTYFOUR_KILO:
        return ppn << 16 | offset;
    case ONE_MEGA:
        return ppn << 20 | offset;
    default:
        throw IntegrityError("Unrecognized UTLB size value");
    }
}

addr32_t Sh4::itlb_ent_get_vpn(struct itlb_entry *ent) const {
    switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // upper 22 bits
        return ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
    case FOUR_KILO:
        // upper 20 bits
        return ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffff000;
    case SIXTYFOUR_KILO:
        // upper 16 bits
        return ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xffff0000;
    case ONE_MEGA:
        // upper 12 bits
        return ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfff00000;
    default:
        throw IntegrityError("Unrecognized ITLB size value");
    }
}

addr32_t Sh4::itlb_ent_get_ppn(struct itlb_entry *ent) const {
    switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // upper 19 bits (of upper 29 bits)
        return ((ent->ent & ITLB_ENT_PPN_MASK) >> ITLB_ENT_PPN_SHIFT) &
            0x1ffffc00;
    case FOUR_KILO:
        // upper 17 bits (of upper 29 bits)
        return ((ent->ent & ITLB_ENT_PPN_MASK) >> ITLB_ENT_PPN_SHIFT) &
            0x1ffff000;
    case SIXTYFOUR_KILO:
        // upper 13 bits (of upper 29 bits)
        return ((ent->ent & ITLB_ENT_PPN_MASK) >> ITLB_ENT_PPN_SHIFT) &
            0x1fff0000;
    case ONE_MEGA:
        // upper 9 bits (of upper 29 bits)
        return ((ent->ent & ITLB_ENT_PPN_MASK) >> ITLB_ENT_PPN_SHIFT) &
            0x1ff00000;
    default:
        throw IntegrityError("Unrecognized ITLB size value");
    }
}

addr32_t Sh4::itlb_ent_get_addr_offset(struct itlb_entry *ent,
                                       addr32_t addr) const {
    switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        // lower 10 bits
        return addr & 0x3ff;
    case FOUR_KILO:
        // lower 12 bits
        return addr & 0xfff;
    case SIXTYFOUR_KILO:
        // lower 16 bits
        return addr & 0xffff;
    case ONE_MEGA:
        // lowr 20 bits
        return addr & 0xfffff;
    default:
        throw IntegrityError("Unrecognized OTLB size value");
    }
}

addr32_t Sh4::itlb_ent_translate(struct itlb_entry *ent, addr32_t vaddr) const {
    addr32_t ppn = itlb_ent_get_ppn(ent);
    addr32_t offset = itlb_ent_get_addr_offset(ent, vaddr);

    switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
    case ONE_KILO:
        return ppn << 10 | offset;
    case FOUR_KILO:
        return ppn << 12 | offset;
    case SIXTYFOUR_KILO:
        return ppn << 16 | offset;
    case ONE_MEGA:
        return ppn << 20 | offset;
    default:
        throw IntegrityError("Unrecognized ITLB size value");
    }
}


// find entry with matching VPN
struct Sh4::utlb_entry *Sh4::utlb_search(addr32_t vaddr,
                                         utlb_access_t access_type) {
    struct Sh4::utlb_entry *ret = NULL;
    addr32_t vpn_vaddr;

    for (unsigned i = 0; i < UTLB_SIZE; i++) {
        struct utlb_entry *ent = utlb + i;
        addr32_t vpn_ent;

        switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
        case ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vpn_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vpn_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vpn_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vpn_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            throw IntegrityError("Unrecognized UTLB size value");
        }

        if (!(UTLB_ENT_SH_MASK & ent->ent) &&
            (!(mmu.mmucr & MMUCR_SV_MASK) || !(reg.sr & SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)

            unsigned utlb_asid = (ent->key & UTLB_KEY_ASID_MASK) >>
                UTLB_KEY_ASID_SHIFT;
            unsigned mmu_asid = (mmu.pteh & MMUPTEH_ASID_MASK) >>
                MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & UTLB_KEY_VALID_MASK) &&
                utlb_asid == mmu_asid) {
                // UTLB hit
                if (ret) {
                    set_exception(EXCP_DATA_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & UTLB_KEY_VALID_MASK)) {
                // UTLB hit
                if (ret) {
                    set_exception(EXCP_DATA_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        }
    }

    /*
     * TODO: Make sure the vpn is being set properly for
     *       UTLB_READ and UTLB_WRITE below.  I wonder if I am confused because
     *       it seems weird to me that different VPN pages can have different
     *       sizes.
     */
    if (!ret) {
        switch (access_type) {
        case UTLB_READ:
            set_exception(EXCP_DATA_TLB_READ_MISS);
            mmu.pteh &= ~MMUPTEH_VPN_MASK;
            mmu.pteh |= vpn_vaddr << MMUPTEH_VPN_SHIFT;
            mmu.tea = vaddr;
            return NULL;
        case UTLB_WRITE:
            set_exception(EXCP_DATA_TLB_WRITE_MISS);
            mmu.pteh &= ~MMUPTEH_VPN_MASK;
            mmu.pteh |= vpn_vaddr << MMUPTEH_VPN_SHIFT;
            mmu.tea = vaddr;
            return NULL;
        case UTLB_READ_ITLB:
            return NULL;
        default:
            throw InvalidParamError("Unknown access type in utlb_search");
        }
    }

    return ret;
}

struct Sh4::itlb_entry *Sh4::itlb_search(addr32_t vaddr) {
    struct Sh4::itlb_entry *ret = NULL;
    addr32_t vpn_vaddr;

    for (unsigned i = 0; i < ITLB_SIZE; i++) {
        struct itlb_entry *ent = itlb + i;
        addr32_t vpn_ent;

        switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
        case ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vpn_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vpn_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vpn_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vpn_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            throw IntegrityError("Unrecognized ITLB size value");
        }

        if (!(ITLB_ENT_SH_MASK & ent->ent) &&
            (!(mmu.mmucr & MMUCR_SV_MASK) || !(reg.sr & SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)
            unsigned itlb_asid = (ent->key & ITLB_KEY_ASID_MASK) >>
                ITLB_KEY_ASID_SHIFT;
            unsigned mmu_asid = (mmu.pteh & MMUPTEH_ASID_MASK) >>
                MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & ITLB_KEY_VALID_MASK) &&
                itlb_asid == mmu_asid) {
                // ITLB hit
                if (ret) {
                    /*
                     *  TODO: set_exception may be setting more flags than is
                     *        necessary in this scenario; the manual is a
                     *        little vague on how this is supposed to work.
                     */
                    mmu.tea = vaddr;
                    set_exception(EXCP_INST_TLB_MULT_HIT);
                    return NULL;
                } else {
                    ret = ent;
                }
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & ITLB_KEY_VALID_MASK)) {
                // ITLB hit
                if (ret) {
                    /*
                     *  TODO: set_exception may be setting more flags than is
                     *        necessary in this scenario; the manual is a
                     *        little vague on how this is supposed to work.
                     */
                    mmu.tea = vaddr;
                    set_exception(EXCP_INST_TLB_MULT_HIT);
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
    struct utlb_entry *utlb_ent;
    utlb_ent = utlb_search(vaddr, UTLB_READ_ITLB);

    if (!utlb_ent) {
        set_exception(EXCP_INST_TLB_MISS);
        mmu.pteh &= ~MMUPTEH_VPN_MASK;
        mmu.pteh |= vpn_vaddr << MMUPTEH_VPN_SHIFT;
        mmu.tea = vaddr;
        return NULL;
    }

    // now replace one of the ITLB entries.  Ideally there would be some sort
    // of Least-Recently-Used algorithm here.
    unsigned which = vaddr & (4 - 1);

    // the key formats are exactly the same, so this is safe.
    itlb[which].key = utlb_ent->key;

    // Notice how the PR gets AND'd with 2.  That is because the ITLB version of
    // PR is only 1 bit, while the UTLB version of PR is two bits.  ITLB's PR
    // corresponds to the upper bit of UTLB's PR.
    itlb[which].ent = 0;
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_PPN_MASK) >>
                        UTLB_ENT_PPN_SHIFT) << ITLB_ENT_PPN_SHIFT;
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_SZ_MASK) >>
                        UTLB_ENT_SZ_SHIFT) << ITLB_ENT_SZ_SHIFT;
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_SH_MASK) >>
                        UTLB_ENT_SH_SHIFT) << ITLB_ENT_SH_SHIFT;
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_C_MASK) >>
                        UTLB_ENT_C_SHIFT) << ITLB_ENT_C_SHIFT;
    itlb[which].ent |= ((((utlb_ent->ent & UTLB_ENT_PR_MASK) >>
                         UTLB_ENT_PR_SHIFT) & 2) << ITLB_ENT_PR_SHIFT);
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_SA_MASK) >>
                        UTLB_ENT_SA_SHIFT) << ITLB_ENT_SA_SHIFT;
    itlb[which].ent |= ((utlb_ent->ent & UTLB_ENT_TC_MASK) >>
                        UTLB_ENT_TC_SHIFT) << ITLB_ENT_TC_SHIFT;

    /*
     * The SH7750 Hardware Manual says to loop back to the beginning (see the
     * flowchart on page 44), so I implement that by recursing back into this
     * function.  Some sort of infinite-recursion detection may be warranted
     * here just in case.
     */
    return itlb_search(vaddr);
}
