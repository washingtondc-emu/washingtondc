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

#include "sh4.hpp"

Sh4::Sh4(Memory *mem) {
    this->mem = mem;

    this->inst_cache = new boost::uint8_t[INSTCACHE_ENTRY_COUNT];
    this->op_cache = new boost::uint8_t[OPCACHE_ENTRY_COUNT];

    memset(inst_cache, 0, sizeof(boost::uint8_t) * INSTCACHE_ENTRY_COUNT);
    memset(op_cache, 0, sizeof(boost::uint8_t) * OPCACHE_ENTRY_COUNT);

    memset(utlb, 0, sizeof(utlb));
}

int Sh4::write_mem(void *out, addr32_t addr, size_t len) {
    // TODO: finish
    if (mmu.mmucr & MMUCR_AT_MASK) {
    } else {
    }
}

int Sh4::read_mem(void *out, addr32_t addr, size_t len) {

}

// find entry with matching VPN
struct Sh4::utlb_entry *Sh4::utlb_search(addr32_t vaddr) const {
    struct Sh4::utlb_entry *ret = NULL;

    for (unsigned i = 0; i < UTLB_SIZE; i++) {
        struct utlb_entry *ent = utlb + i;
        addr32_t vpn_ent;
        addr32_t vpn_vaddr;

        switch ((ent->ent & UTLB_ENT_SZ_MASK) >> UTLB_ENT_SZ_SHIFT) {
        case ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vp_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vp_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vp_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vp_ent = ((ent->key & UTLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            throw IntegrityError("Unrecognized UTLB size value");
        }

        if (!(UTLB_ENT_SH_MASK & ent->ent) &&
            (!(mmu.mmucr & MMUCR_SV) || !(reg.sr & SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)

            unsigned utlb_asid = (ent->key & UTLB_KEY_ASID_MASK) >>
                UTLB_KEY_ASID_SHIFT;
            unsigned mmu_asid = (mmu.pteh & MMUPTEH_ASID_MASK) >>
                MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & UTLB_KEY_VALID_MASK) &&
                utlb_asid == asid_shift) {
                // UTLB hit
                if (ret)
                    throw UnimplementedError("Data TLB multiple hit exception");
                else
                    ret = ent;
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & UTLB_KEY_VALID_MASK)) {
                // UTLB hit
                if (ret)
                    throw UnimplementedError("Data TLB multiple hit exception");
                else
                    ret = ent;
            }
        }
    }

    if (!ret)
        throw UnimplementedError("Data TLB miss exception");
    return ret;
}

struct Sh4::itlb_entry *Sh4::itlb_search(addr32_t vaddr) {
    struct Sh4::itlb_entry *ret = NULL;

    for (unsigned i = 0; i < ITLB_SIZE; i++) {
        struct itlb_entry *ent = itlb + i;
        addr32_t vpn_ent;
        addr32_t vpn_vaddr;

        switch ((ent->ent & ITLB_ENT_SZ_MASK) >> ITLB_ENT_SZ_SHIFT) {
        case ONE_KILO:
            // upper 22 bits
            vpn_vaddr = vaddr & 0xfffffc00;
            vp_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffffc00;
            break;
        case FOUR_KILO:
            // upper 20 bits
            vpn_vaddr = vaddr & 0xfffff000;
            vp_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfffff000;
            break;
        case SIXTYFOUR_KILO:
            // upper 16 bits
            vpn_vaddr = vaddr & 0xffff0000;
            vp_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xffff0000;
            break;
        case ONE_MEGA:
            // upper 12 bits
            vpn_vaddr = vaddr & 0xfff00000;
            vp_ent = ((ent->key & ITLB_KEY_VPN_MASK) << 8) & 0xfff00000;
            break;
        default:
            throw IntegrityError("Unrecognized ITLB size value");
        }

        if (!(ITLB_ENT_SH_MASK & ent->ent) &&
            (!(mmu.mmucr & MMUCR_SV) || !(reg.sr & SR_MD_MASK))) {
            // (not sharing pages) and (single-VM space or user-mode mode)
            unsigned itlb_asid = (ent->key & ITLB_KEY_ASID_MASK) >>
                ITLB_KEY_ASID_SHIFT;
            unsigned mmu_asid = (mmu.pteh & MMUPTEH_ASID_MASK) >>
                MMUPTEH_ASID_SHIFT;
            if (vpn_vaddr == vpn_ent && (ent->key & ITLB_KEY_VALID_MASK) &&
                itlb_asid == asid_shift) {
                // ITLB hit
                if (ret)
                    throw UnimplementedError("Data TLB multiple hit exception");
                else
                    ret = ent;
            }
        } else {
            if (vpn_vaddr == vpn_ent && (ent->key & ITLB_KEY_VALID_MASK)) {
                // ITLB hit
                if (ret)
                    throw UnimplementedError("Data TLB multiple hit exception");
                else
                    ret = ent;
            }
        }
    }

    if (ret)
        return ret;

    // ITLB miss - check the UTLB
    struct utlb_entry *utlb_ent;
    try {
        utlb_ent = utlb_search(vaddr);
    } catch (UnimplementedError err) {
        // TODO: When CPU exceptions are implemented, there will need to be an
        //       option for utlb_search to prevent it from creating a CPU
        //       exception on miss so that this function can do it itself.
        throw UniplementedError("Instruction TLB miss exception");
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

struct Sh4::op_cache_line *Sh4::op_cache_check(addr32_t paddr) {
    addr32_t paddr_tag;

    if (cache_reg.ccr & CCR_ORA_MASK) {
        // the hardware manual is a little vague on how this effects
        // the half of the cache which is not being used as memory.
        throw UnimplementedError("Operand Cache as RAM");
    }

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = paddr & 0x1ffffc00;

    // entry selection (index into the op cache)
    addr32_t ent_sel = paddr & 0xff0;
    if (cache_reg.ccr & CCR_OIX_MASK)
        ent_sel |= (paddr & (1 << 25)) >> 12;
    else
        ent_sel |= paddr & (1 << 13);
    ent_sel >>= 4;

    struct op_cache_line *line = op_cache + ent_sel;

    if ((line->key & OPCACHE_KEY_VALID_BIT)) {
        addr32_t line_tag = (OPCACHE_KEY_TAG_MASK & line->key) >>
            OPCACHE_KEY_TAG_SHIFT;
        if (line_tag == paddr_tag)
            return line;
    }
    return NULL;
}

struct Sh4::inst_cache_line *Sh4::inst_cache_check(addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = paddr & 0x1ffffc00;

    // entry selection (index into the inst cache)
    addr32_t ent_sel = paddr & 0x7f0;
    if (cache_reg.ccr & CCR_IIX_MASK)
        ent_sel |= (paddr & (1 << 25)) >> 13;
    else
        ent_sel |= paddr & (1 << 12);
    ent_sel >>= 4;

    struct inst_cache_line *line = inst_cache + ent_sel;

    if ((line->key & INSTCACHE_KEY_VALID_BIT)) {
        addr32_t line_tag = (INSTCACHE_KEY_TAG_MASK & line->key) >>
            INSTCACHE_KEY_TAG_SHIFT;
        if (line_tag == paddr_tag)
            return line;
    }
    return NULL;
}

enum Sh4::PhysMemArea Sh4::get_mem_area(addr32_t addr) {
    if (addr >= AREA_P0_FIRST && addr <= AREA_P0_LAST)
        return AREA_P0;
    if (addr >= AREA_P1_FIRST && addr <= AREA_P1_LAST)
        return AREA_P1;
    if (addr >= AREA_P2_FIRST && addr <= AREA_P2_LAST)
        return AREA_P2;
    if (addr >= AREA_P3_FIRST && addr <= AREA_P3_LAST)
        return AREA_P3;
    return AREA_P4;
}
