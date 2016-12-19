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

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#include "sh4.hpp"

int Sh4::do_write_mem(basic_val_t data, addr32_t addr, unsigned len) {
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
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
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
#ifdef ENABLE_SH4_OCACHE
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
#endif
                            // don't use the cache
                            return mem->write(&data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
                        }
#endif
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
#ifdef ENABLE_SH4_OCACHE
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
#endif
                        // don't use the cache
                        return mem->write(&data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
                    }
#endif
                } else {
                    set_exception(EXCP_INITIAL_PAGE_WRITE);
                    mmu.tea = addr;
                    return 1;
                }
            }
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON"
                                                 " and rebuild"));
#endif
        } else {
#ifdef ENABLE_SH4_OCACHE
            if (cache_reg.ccr & CCR_WT_MASK) {
                return op_cache->cache_write_wt(data, len, addr, index_enable,
                                                cache_as_ram);
            } else {
                return op_cache->cache_write_cb(data, len, addr, index_enable,
                                                cache_as_ram);
            }
#else
            // don't use the cache
            return mem->write(&data, addr & 0x1fffffff, len);
#endif
        }
        break;
    case AREA_P1:
#ifdef ENABLE_SH4_OCACHE
        if (cache_reg.ccr & CCR_OCE_MASK) {
            if (cache_reg.ccr & CCR_CB_MASK) {
                return op_cache->cache_write_cb(data, len, addr, index_enable,
                                                 cache_as_ram);
            } else {
                return op_cache->cache_write_wt(data, len, addr, index_enable,
                                                 cache_as_ram);
            }
        } else {
#endif
            return mem->write(&data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
        }
#endif
        break;
    case AREA_P2:
        return mem->write(&data, addr & 0x1fffffff, len);
        break;
    case AREA_P4:
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Register access through "
                                              "memory"));
        break;
    default:
        break;
    }

    BOOST_THROW_EXCEPTION(IntegrityError() <<
                          errinfo_wtf("I don't believe it should be possible "
                                      "to get here (see Sh4::write_mem)"));
}

int Sh4::do_read_mem(basic_val_t *data, addr32_t addr, unsigned len) {
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
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
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

#ifdef ENABLE_SH4_OCACHE
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
#endif
                // don't use the cache
                return mem->read(data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
            }
#endif
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON "
                                                 "and rebuild"));

#endif
        } else {
#ifdef ENABLE_SH4_OCACHE
            if (cache_reg.ccr & CCR_WT_MASK) {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            } else {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            }
#else
            // don't use the cache
            return mem->read(data, addr & 0x1fffffff, len);
#endif
        }
        break;
    case AREA_P1:
#ifdef ENABLE_SH4_OCACHE
        if (cache_reg.ccr & CCR_OCE_MASK) {
            if (cache_reg.ccr & CCR_CB_MASK) {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            } else {
                return op_cache->cache_read(data, len, addr, index_enable,
                                            cache_as_ram);
            }
        } else {
#endif
            return mem->read(&data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
        }
#endif
        break;
    case AREA_P2:
        return mem->read(&data, addr & 0x1fffffff, len);
        break;
    case AREA_P4:
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Register access "
                                              "through memory"));
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
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case AREA_P0:
    case AREA_P3:
        if (mmu.mmucr & MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
            struct itlb_entry *itlb_ent = itlb_search(addr);

            if (!itlb_ent)
                return 1;  // exception set by itlb_search

            if (privileged || (itlb_ent->ent & ITLB_ENT_PR_MASK)) {
                addr32_t paddr = itlb_ent_translate(itlb_ent, addr);

#ifdef ENABLE_SH4_ICACHE
                if ((itlb_ent->ent & ITLB_ENT_C_MASK) &&
                    (cache_reg.ccr & CCR_ICE_MASK)) {
                    // use the cache
                    boost::uint32_t buf;
                    int ret;
                    ret = inst_cache->read(&buf, paddr, index_enable);
                    *out = buf;
                    return ret;
                } else {
#endif
                    // don't use the cache
                    return mem->read(out, addr & 0x1fffffff, sizeof(*out));
#ifdef ENABLE_SH4_ICACHE
                }
#endif
            }
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON "
                                                  "and rebuild"));
#endif
        } else {
#ifdef ENABLE_SH4_ICACHE
            if (cache_reg.ccr & CCR_ICE_MASK) {
                boost::uint32_t buf;
                int ret;
                ret = inst_cache->read(&buf, addr, index_enable);
                *out = buf;
                return ret;
            } else {
#endif
                return mem->read(out, addr & 0x1fffffff, sizeof(*out));
#ifdef ENABLE_SH4_ICACHE
            }
#endif
        }
        break;
    case AREA_P1:
#ifdef ENABLE_SH4_ICACHE
        if (cache_reg.ccr & CCR_ICE_MASK) {
            boost::uint32_t buf;
            int ret;
            ret = inst_cache->read(&buf, addr, index_enable);
            *out = buf;
            return ret;
        } else {
#endif
            return mem->read(out, addr & 0x1fffffff, sizeof(*out));
#ifdef ENABLE_SH4_ICACHE
        }
#endif
        break;
    case AREA_P2:
        return mem->read(out, addr & 0x1fffffff, sizeof(*out));
    case AREA_P4:
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for reading "
                                              "instructions from the P4 "
                                              "memory area"));
    default:
        break;
    }

    return 1;
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
