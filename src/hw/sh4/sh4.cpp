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

#include <cstring>

#include "Icache.hpp"
#include "BaseException.hpp"

#include "sh4.hpp"

Sh4::Sh4(Memory *mem) {
    this->mem = mem;

    this->op_cache = new struct op_cache_line[OPCACHE_ENTRY_COUNT];
    memset(op_cache, 0, sizeof(struct op_cache_line) * OPCACHE_ENTRY_COUNT);

    memset(utlb, 0, sizeof(utlb));

    this->inst_cache = new Icache(this, mem);
}

Sh4::~Sh4() {
    delete inst_cache;
}

int Sh4::write_mem(void const *out, addr32_t addr, size_t len) {
    // TODO: finish
    if (mmu.mmucr & MMUCR_AT_MASK) {
    } else {
    }

    return 1;
}

int Sh4::read_mem(void *out, addr32_t addr, size_t len) {
    return 1;
}

// find entry with matching VPN
struct Sh4::utlb_entry *Sh4::utlb_search(addr32_t vaddr) {
    struct Sh4::utlb_entry *ret = NULL;

    for (unsigned i = 0; i < UTLB_SIZE; i++) {
        struct utlb_entry *ent = utlb + i;
        addr32_t vpn_ent;
        addr32_t vpn_vaddr;

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
        throw UnimplementedError("Instruction TLB miss exception");
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

bool Sh4::op_cache_check(struct Sh4::op_cache_line const *line,
                         addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = op_cache_tag_from_paddr(paddr);

    addr32_t line_tag = op_cache_line_get_tag(line);
    if (line_tag == paddr_tag)
        return line;
    return NULL;
}

addr32_t Sh4::op_cache_selector(addr32_t paddr) const {
    if (cache_reg.ccr & CCR_ORA_MASK) {
        // the hardware manual is a little vague on how this effects
        // the half of the cache which is not being used as memory.
        throw UnimplementedError("Operand Cache as RAM");
    }

    addr32_t ent_sel = paddr & 0xff0;
    if (cache_reg.ccr & CCR_OIX_MASK)
        ent_sel |= (paddr & (1 << 25)) >> 12;
    else
        ent_sel |= paddr & (1 << 13);
    ent_sel >>= 4;

    return ent_sel;
}

int Sh4::op_cache_read4(boost::uint32_t *out, addr32_t paddr) {
    int err = 0;

    if (paddr & 0x3) {
        throw UnimplementedError("Unaligned memory access exception");
    }
    op_cache_line *line = op_cache_selector(paddr) + op_cache;

    if (line->key & OPCACHE_KEY_VALID_MASK) {
        if (op_cache_check(line, paddr)) {
            // cache hit
            addr32_t idx = (paddr & 0x1f) >> 2;

            *out = line->lw[idx];
            return 0;
        } else {
            // tag does not match, V bit is 1
            if (line->key & OPCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = op_cache_write_back(line, paddr);
                if (err)
                    return err;
                err = op_cache_load(line, paddr);
            } else {
                //cache miss (no write-back)
                err = op_cache_load(line, paddr);
            }
        }
    } else {
        // valid bit is 0, tag may or may not match
        // cache miss (no write-back)
        err = op_cache_load(line, paddr);
    }

    if (!err) {
        addr32_t idx = (paddr & 0x1f) >> 2;

        *out = line->lw[idx];
        return 0;
    }

    return err;
}

int Sh4::op_cache_write4_cb(boost::uint32_t const *data, addr32_t paddr) {
    int err = 0;

    if (paddr & 0x3) {
        throw UnimplementedError("Unaligned memory access exception");
    }
    op_cache_line *line = op_cache_selector(paddr) + op_cache;
    unsigned lw_idx = (paddr & 0x1f) >> 2;

    if (op_cache_check(line, paddr)) {
        if (line->key & OPCACHE_KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            line->lw[lw_idx] = *data;
            line->key |= OPCACHE_KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            op_cache_load(line, paddr);
            line->lw[lw_idx] = *data;
            line->key |= OPCACHE_KEY_DIRTY_MASK;
        }
    } else {
        if (line->key & OPCACHE_KEY_VALID_MASK) {
            if (line->key & OPCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = op_cache_write_back(line, paddr);
                if (err)
                    return err;
                err = op_cache_load(line, paddr);
                line->lw[lw_idx] = *data;
                line->key |= OPCACHE_KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                op_cache_load(line, paddr);
                line->lw[lw_idx] = *data;
                line->key |= OPCACHE_KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            op_cache_load(line, paddr);
            line->lw[lw_idx] = *data;
            line->key |= OPCACHE_KEY_DIRTY_MASK;
        }
    }

    return 0;
}

int Sh4::op_cache_write4_wt(boost::uint32_t const *data, addr32_t paddr) {
    int err = 0;

    if (paddr & 0x3) {
        throw UnimplementedError("Unaligned memory access exception");
    }
    struct op_cache_line *line = op_cache_selector(paddr) + op_cache;
    unsigned lw_idx = (paddr & 0x1f) >> 2;

    if (op_cache_check(line, paddr) && (line->key & OPCACHE_KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        line->lw[lw_idx] = *data;
        if ((err = mem->write(data, paddr, sizeof(boost::uint32_t))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(data, paddr, sizeof(boost::uint32_t))) != 0)
            return err;
    }

    return 0;
}

int Sh4::op_cache_load(struct op_cache_line *line, addr32_t paddr) {
    int err_code;

    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_OPCACHE_LINE;
    if ((err_code = mem->read(line->lw, paddr & ~31, n_bytes)) != 0)
        return err_code;

    op_cache_line_set_tag(line, op_cache_tag_from_paddr(paddr));
    line->key |= OPCACHE_KEY_VALID_MASK;
    line->key &= ~OPCACHE_KEY_DIRTY_MASK;

    return 0;
}

int Sh4::op_cache_write_back(struct op_cache_line *line, addr32_t paddr) {
    int err_code = 0;

    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_OPCACHE_LINE;
    if ((err_code = mem->write(line->lw, paddr & ~31, n_bytes)) != 0)
        return err_code;

    line->key &= ~OPCACHE_KEY_DIRTY_MASK;
    return 0;
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
