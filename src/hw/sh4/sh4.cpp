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
#include "Ocache.hpp"
#include "BaseException.hpp"

#include "sh4.hpp"

Sh4::Sh4(Memory *mem) {
    this->mem = mem;

    memset(utlb, 0, sizeof(utlb));
    memset(itlb, 0, sizeof(itlb));
    memset(&reg, 0, sizeof(reg));
    memset(&mmu, 0, sizeof(mmu));
    memset(&cache_reg, 0, sizeof(cache_reg));

    this->inst_cache = new Icache(this, mem);
    this->op_cache = new Ocache(this, mem);
}

Sh4::~Sh4() {
    delete op_cache;
    delete inst_cache;
}

int Sh4::write_mem(basic_val_t data, addr32_t addr, unsigned len) {
    enum VirtMemArea virt_area = get_mem_area(addr);

    bool privileged = reg.sr & SR_MD_MASK ? true : false;
    bool index_enable = cache_reg.ccr & CCR_OIX_MASK ? true : false;
    bool cache_as_ram = cache_reg.ccr & CCR_ORA_MASK ? true : false;

    if (virt_area != AREA_P0 && privileged) {
        // TODO: allow user-mode access to the store queue area

        /*
         * The spec says user-mode processes can only write to the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        throw UnimplementedError("CPU exception for unprivileged access to "
                                 "high memory areas");
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
            struct utlb_entry *utlb_ent = utlb_search(addr, UTLB_WRITE);

            if (!utlb_ent)
                return 1; // exception set by utlb_search

            unsigned pr = (utlb_ent->ent & UTLB_ENT_PR_MASK) >>
                UTLB_ENT_PR_SHIFT;

            addr32_t paddr = utlb_ent_translate(utlb_ent, addr);
            if (privileged) {
                if (pr & 1) {
                    // page is marked as read-write

                    if (utlb_ent->ent & UTLB_ENT_D_MASK) {
                        if ((utlb_ent->ent & UTLB_ENT_C_MASK) &&
                            (cache_reg.ccr & CCR_OCE_MASK)) {
                            // page is cacheable and the cache is enabled
                            if (cache_reg.ccr & CCR_WT_MASK) {
                                return op_cache->cache_write_wt(data, len,
                                                                paddr,
                                                                index_enable,
                                                                cache_as_ram);
                            } else {
                                return op_cache->cache_write_cb(data, len,
                                                                paddr,
                                                                index_enable,
                                                                cache_as_ram);
                            }
                        } else {
                            // don't use the cache
                            return mem->write(&data, addr & 0x1fffffff, len);
                        }
                    } else {
                        set_exception(EXCP_INITIAL_PAGE_WRITE);
                        mmu.tea = addr;
                        return 1;
                    }
                } else {
                    // page is marked as read-only
                    unsigned vpn = (utlb_ent->key & UTLB_KEY_VPN_MASK) >>
                        UTLB_KEY_VPN_SHIFT;
                    set_exception(EXCP_DATA_TLB_WRITE_PROT_VIOL);
                    mmu.pteh &= ~MMUPTEH_VPN_MASK;
                    mmu.pteh |= vpn << MMUPTEH_VPN_SHIFT;
                    mmu.tea = addr;
                    return 1;
                }
            } else {
                if (pr != 3) {
                    // page is marked as read-only OR we don't have permissions
                    unsigned vpn = (utlb_ent->key & UTLB_KEY_VPN_MASK) >>
                        UTLB_KEY_VPN_SHIFT;
                    set_exception(EXCP_DATA_TLB_WRITE_PROT_VIOL);
                    mmu.pteh &= ~MMUPTEH_VPN_MASK;
                    mmu.pteh |= vpn << MMUPTEH_VPN_SHIFT;
                    mmu.tea = addr;
                    return 1;
                }

                if (utlb_ent->ent & UTLB_ENT_D_MASK) {
                    if ((utlb_ent->ent & UTLB_ENT_C_MASK) &&
                        (cache_reg.ccr & CCR_OCE_MASK)) {
                        // page is cacheable and the cache is enabled
                        if (cache_reg.ccr & CCR_WT_MASK) {
                            return op_cache->cache_write_wt(data, len, paddr,
                                                             index_enable,
                                                             cache_as_ram);
                        } else {
                            return op_cache->cache_write_cb(data, len, paddr,
                                                             index_enable,
                                                             cache_as_ram);
                        }
                    } else {
                        // don't use the cache
                        return mem->write(&data, addr & 0x1fffffff, len);
                    }
                } else {
                    set_exception(EXCP_INITIAL_PAGE_WRITE);
                    mmu.tea = addr;
                    return 1;
                }
            }
        } else {
            if (cache_reg.ccr & CCR_WT_MASK) {
                return op_cache->cache_write_wt(data, len, addr, index_enable,
                                                cache_as_ram);
            } else {
                return op_cache->cache_write_cb(data, len, addr, index_enable,
                                                cache_as_ram);
            }
        }
        break;
    case AREA_P1:
        if (cache_reg.ccr & CCR_OCE_MASK) {
            if (cache_reg.ccr & CCR_CB_MASK) {
                return op_cache->cache_write_cb(data, len, addr, index_enable,
                                                 cache_as_ram);
            } else {
                return op_cache->cache_write_wt(data, len, addr, index_enable,
                                                 cache_as_ram);
            }
        } else {
            return mem->write(&data, addr & 0x1fffffff, len);
        }
        break;
    case AREA_P2:
        return mem->write(&data, addr & 0x1fffffff, len);
        break;
    case AREA_P4:
        throw UnimplementedError("Register access through memory");
        break;
    default:
        break;
    }

    throw IntegrityError("I don't believe it should be possible to get here "
                         "(see Sh4::write_mem)");
}

int Sh4::read_mem(basic_val_t *data, addr32_t addr, unsigned len) {
    enum VirtMemArea virt_area = get_mem_area(addr & 0x1fffffff);

    bool privileged = reg.sr & SR_MD_MASK ? true : false;
    bool index_enable = cache_reg.ccr & CCR_OIX_MASK ? true : false;
    bool cache_as_ram = cache_reg.ccr & CCR_ORA_MASK ? true : false;

    if (virt_area != AREA_P0 && privileged) {
        // TODO: allow user-mode access to the store queue area

        /*
         * The spec says user-mode processes can only write to the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        throw UnimplementedError("CPU exception for unprivileged access to "
                                 "high memory areas");
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
            struct utlb_entry *utlb_ent = utlb_search(addr, UTLB_READ);

            if (!utlb_ent)
                return 1; // exception set by utlb_search

            unsigned pr = (utlb_ent->ent & UTLB_ENT_PR_MASK) >>
                UTLB_ENT_PR_SHIFT;

            addr32_t paddr = utlb_ent_translate(utlb_ent, addr);
            if (!privileged && !(pr & 2)) {
                // we don't have permissions
                unsigned vpn = (utlb_ent->key & UTLB_KEY_VPN_MASK) >>
                    UTLB_KEY_VPN_SHIFT;
                set_exception(EXCP_DATA_TLB_WRITE_PROT_VIOL);
                mmu.pteh &= ~MMUPTEH_VPN_MASK;
                mmu.pteh |= vpn << MMUPTEH_VPN_SHIFT;
                mmu.tea = addr;
                return 1;
            }

            if ((utlb_ent->ent & UTLB_ENT_C_MASK) &&
                (cache_reg.ccr & CCR_OCE_MASK)) {
                // page is cacheable and the cache is enabled
                if (cache_reg.ccr & CCR_WT_MASK) {
                    return op_cache->cache_read(data, len, paddr, index_enable,
                                                cache_as_ram);
                } else {
                    return op_cache->cache_read(data, len, paddr, index_enable,
                                                cache_as_ram);
                }
            } else {
                // don't use the cache
                return mem->read(data, addr & 0x1fffffff, len);
            }
        } else {
            if (cache_reg.ccr & CCR_WT_MASK) {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            } else {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            }
        }
        break;
    case AREA_P1:
        if (cache_reg.ccr & CCR_OCE_MASK) {
            if (cache_reg.ccr & CCR_CB_MASK) {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            } else {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            }
        } else {
            return mem->read(&data, addr & 0x1fffffff, len);
        }
        break;
    case AREA_P2:
        return mem->read(&data, addr & 0x1fffffff, len);
        break;
    case AREA_P4:
        throw UnimplementedError("Register access through memory");
        break;
    default:
        break;
    }

    return 1;
}

int Sh4::read_inst(inst_t *out, addr32_t addr) {
    enum VirtMemArea virt_area = get_mem_area(addr);
    bool privileged = reg.sr & SR_MD_MASK ? true : false;
    bool index_enable = cache_reg.ccr & CCR_IIX_MASK ? true : false;

    if (virt_area != AREA_P0 && !privileged) {
        /*
         * The spec says user-mode processes can only access the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        throw UnimplementedError("CPU exception for unprivileged access to "
                                 "high memory areas");
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
            struct itlb_entry *itlb_ent = itlb_search(addr);

            if (!itlb_ent)
                return 1;  // exception set by itlb_search

            if (privileged || (itlb_ent->ent & ITLB_ENT_PR_MASK)) {
                addr32_t paddr = itlb_ent_translate(itlb_ent, addr);

                if ((itlb_ent->ent & ITLB_ENT_C_MASK) &&
                    (cache_reg.ccr & CCR_ICE_MASK)) {
                    // use the cache
                    boost::uint32_t buf;
                    int ret;
                    ret = inst_cache->read(&buf, paddr, index_enable);
                    *out = buf;
                    return ret;
                } else {
                    // don't use the cache
                    return mem->read(out, addr & 0x1fffffff, sizeof(*out));
                }
            }
        } else {
            if (cache_reg.ccr & CCR_ICE_MASK) {
                boost::uint32_t buf;
                int ret;
                ret = inst_cache->read(&buf, addr, index_enable);
                *out = buf;
                return ret;
            } else {
                return mem->read(out, addr & 0x1fffffff, sizeof(*out));
            }
        }
        break;
    case AREA_P1:
        if (cache_reg.ccr & CCR_ICE_MASK) {
            boost::uint32_t buf;
            int ret;
            ret = inst_cache->read(&buf, addr, index_enable);
            *out = buf;
            return ret;
        } else {
            return mem->read(out, addr & 0x1fffffff, sizeof(*out));
        }
        break;
    case AREA_P2:
        return mem->read(out, addr & 0x1fffffff, sizeof(*out));
    case AREA_P4:
        throw UnimplementedError("CPU exception for reading instructions from "
                                 "the P4 memory area");
    default:
        break;
    }

    return 1;
}

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

enum Sh4::VirtMemArea Sh4::get_mem_area(addr32_t addr) {
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

void Sh4::set_exception(unsigned excp_code) {
    excp_reg.expevt = (excp_code << EXPEVT_CODE_SHIFT) | EXPEVT_CODE_MASK;

    enter_exception((ExceptionCode)excp_code);
}

void Sh4::set_interrupt(unsigned intp_code) {
    excp_reg.intevt = (intp_code << INTEVT_CODE_SHIFT) | INTEVT_CODE_MASK;

    enter_exception((ExceptionCode)intp_code);
}

const Sh4::ExcpMeta Sh4::excp_meta[Sh4::EXCP_COUNT] = {
    // exception code                         prio_level   prio_order   offset
    { EXCP_POWER_ON_RESET,                    1,           1,           0      },
    { EXCP_MANUAL_RESET,                      1,           2,           0      },
    { EXCP_HUDI_RESET,                        1,           1,           0      },
    { EXCP_INST_TLB_MULT_HIT,                 1,           3,           0      },
    { EXCP_DATA_TLB_MULT_HIT,                 1,           4,           0      },
    { EXCP_USER_BREAK_BEFORE,                 2,           0,           0x100  },
    { EXCP_INST_ADDR_ERR,                     2,           1,           0x100  },
    { EXCP_INST_TLB_MISS,                     2,           2,           0x400  },
    { EXCP_INST_TLB_PROT_VIOL,                2,           3,           0x100  },
    { EXCP_GEN_ILLEGAL_INST,                  2,           4,           0x100  },
    { EXCP_SLOT_ILLEGAL_INST,                 2,           4,           0x100  },
    { EXCP_GEN_FPU_DISABLE,                   2,           4,           0x100  },
    { EXCP_SLOT_FPU_DISABLE,                  2,           4,           0x100  },
    { EXCP_DATA_ADDR_READ,                    2,           5,           0x100  },
    { EXCP_DATA_ADDR_WRITE,                   2,           5,           0x100  },
    { EXCP_DATA_TLB_READ_MISS,                2,           6,           0x400  },
    { EXCP_DATA_TLB_WRITE_MISS,               2,           6,           0x400  },
    { EXCP_DATA_TLB_READ_PROT_VIOL,           2,           7,           0x100  },
    { EXCP_DATA_TLB_WRITE_PROT_VIOL,          2,           7,           0x100  },
    { EXCP_FPU,                               2,           8,           0x100  },
    { EXCP_INITIAL_PAGE_WRITE,                2,           9,           0x100  },
    { EXCP_UNCONDITIONAL_TRAP,                2,           4,           0x100  },
    { EXCP_USER_BREAK_AFTER,                  2,          10,           0x100  },
    { EXCP_NMI,                               3,           0,           0x600  },
    { EXCP_EXT_0,                             4,           2,           0x600  },
    { EXCP_EXT_1,                             4,           2,           0x600  },
    { EXCP_EXT_2,                             4,           2,           0x600  },
    { EXCP_EXT_3,                             4,           2,           0x600  },
    { EXCP_EXT_4,                             4,           2,           0x600  },
    { EXCP_EXT_5,                             4,           2,           0x600  },
    { EXCP_EXT_6,                             4,           2,           0x600  },
    { EXCP_EXT_7,                             4,           2,           0x600  },
    { EXCP_EXT_8,                             4,           2,           0x600  },
    { EXCP_EXT_9,                             4,           2,           0x600  },
    { EXCP_EXT_A,                             4,           2,           0x600  },
    { EXCP_EXT_B,                             4,           2,           0x600  },
    { EXCP_EXT_C,                             4,           2,           0x600  },
    { EXCP_EXT_D,                             4,           2,           0x600  },
    { EXCP_EXT_E,                             4,           2,           0x600  },
    { EXCP_TMU0_TUNI0,                        4,           2,           0x600  },
    { EXCP_TMU1_TUNI1,                        4,           2,           0x600  },
    { EXCP_TMU2_TUNI2,                        4,           2,           0x600  },
    { EXCP_TMU2_TICPI2,                       4,           2,           0x600  },
    { EXCP_RTC_ATI,                           4,           2,           0x600  },
    { EXCP_RTC_PRI,                           4,           2,           0x600  },
    { EXCP_RTC_CUI,                           4,           2,           0x600  },
    { EXCP_SCI_ERI,                           4,           2,           0x600  },
    { EXCP_SCI_RXI,                           4,           2,           0x600  },
    { EXCP_SCI_TXI,                           4,           2,           0x600  },
    { EXCP_SCI_TEI,                           4,           2,           0x600  },
    { EXCP_WDT_ITI,                           4,           2,           0x600  },
    { EXCP_REF_RCMI,                          4,           2,           0x600  },
    { EXCP_REF_ROVI,                          4,           2,           0x600  },
    { EXCP_GPIO_GPIOI,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE0,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE1,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE2,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE3,                        4,           2,           0x600  },
    { EXCP_DMAC_DMAE,                         4,           2,           0x600  },
    { EXCP_SCIF_ERI,                          4,           2,           0x600  },
    { EXCP_SCIF_RXI,                          4,           2,           0x600  },
    { EXCP_SCIF_BRI,                          4,           2,           0x600  },
    { EXCP_SCIF_TXI,                          4,           2,           0x600  }
};

void Sh4::enter_exception(enum ExceptionCode vector) {
    struct ExcpMeta const *meta = NULL;

    for (unsigned idx = 0; idx < EXCP_COUNT; idx++) {
        if (excp_meta[idx].code == vector) {
            meta = excp_meta + idx;
            break;
        }
    }

    if (!meta)
        throw IntegrityError("Unknown CPU exception/interrupt type");

    reg.spc = reg.pc;
    reg.ssr = reg.sr;
    reg.sgr = reg.rgen[7];

    reg.sr |= SR_BL_MASK;
    reg.sr |= SR_MD_MASK;
    reg.sr |= SR_RB_MASK;
    reg.sr &= ~SR_FD_MASK;

    if (vector == EXCP_POWER_ON_RESET ||
        vector == EXCP_MANUAL_RESET ||
        vector == EXCP_HUDI_RESET ||
        vector == EXCP_INST_TLB_MULT_HIT ||
        vector == EXCP_INST_TLB_MULT_HIT) {
        reg.pc = 0xa0000000;
    } else if (vector == EXCP_USER_BREAK_BEFORE ||
               vector == EXCP_USER_BREAK_AFTER) {
        // TODO: check brcr.ubde and use DBR instead of VBR if it is set
        reg.pc = reg.vbr + meta->offset;
    } else {
        reg.pc = reg.vbr + meta->offset;
    }
}
