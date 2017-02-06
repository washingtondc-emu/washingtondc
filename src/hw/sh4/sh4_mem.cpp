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

#include "BaseException.hpp"

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#include "sh4_mmu.hpp"
#include "sh4_excp.hpp"
#include "sh4_mem.hpp"
#include "sh4.hpp"

int sh4_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {
    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

#ifdef ENABLE_SH4_OCACHE
    bool index_enable = sh4->reg[SH4_REG_CCR] & SH4_CCR_OIX_MASK ? true : false;
    bool cache_as_ram = sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK ? true : false;
#endif

    if (virt_area != SH4_AREA_P0 && !privileged) {
        // TODO: allow user-mode access to the store queue area

        /*
         * The spec says user-mode processes can only write to the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
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
#ifdef ENABLE_SH4_OCACHE
                        if ((utlb_ent->ent & SH4_UTLB_ENT_C_MASK) &&
                            (sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK)) {
                            // page is cacheable and the cache is enabled
                            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_WT_MASK) {
                                return sh4_ocache_write_wt(&sh4->op_cache,
                                                           data, len, paddr,
                                                           index_enable,
                                                           cache_as_ram);
                            } else {
                                return sh4_ocache_write_cb(&sh4->op_cache,
                                                           data, len, paddr,
                                                           index_enable,
                                                           cache_as_ram);
                            }
                        } else {
#endif

#ifndef ENABLE_SH4_OCACHE
                            // handle the case where OCE is enabled and ORA is
                            // enabled but we don't have Ocache available
                            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                                sh4_in_oc_ram_area(paddr)) {
                                do_write_ora(data, paddr, len);
                                return 0;
                            }
#endif
                            return memory_map_write(data, paddr & 0x1fffffff,
                                                    len);

#ifdef ENABLE_SH4_OCACHE
                        }
#endif
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
#ifdef ENABLE_SH4_OCACHE
                    if ((utlb_ent->ent & SH4_UTLB_ENT_C_MASK) &&
                        (sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK)) {
                        // page is cacheable and the cache is enabled
                        if (sh4->reg[SH4_REG_CCR] & SH4_CCR_WT_MASK) {
                            return sh4_ocache_write_wt(&sh4->op_cache, data,
                                                       len, paddr, index_enable,
                                                       cache_as_ram);
                        } else {
                            return sh4_ocache_write_cb(&sh4->op_cache, data,
                                                       len, paddr, index_enable,
                                                       cache_as_ram);
                        }
                    } else {
#else
                        // handle the case where OCE is enabled and ORA is
                        // enabled but we don't have Ocache available
                        if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                            (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                            sh4_in_oc_ram_area(paddr)) {
                            do_write_ora(data, paddr, len);
                            return;
                        }
#endif

                        // don't use the cache
                        return memory_map_write(data, paddr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
                    }
#endif
                } else {
                    sh4_set_exception(sh4, SH4_EXCP_INITIAL_PAGE_WRITE);
                    sh4->reg[SH4_REG_TEA] = addr;
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
            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_WT_MASK) {
                return sh4_ocache_write_wt(&sh4->op_cache, data, len, addr,
                                           index_enable, cache_as_ram);
            } else {
                return sh4_ocache_write_cb(&sh4->op_cache, data, len, addr,
                                           index_enable, cache_as_ram);
            }
#else
            // handle the case where OCE is enabled and ORA is
            // enabled but we don't have Ocache available
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                sh4_in_oc_ram_area(addr)) {
                sh4_do_write_ora(sh4, data, addr, len);
                return 0;
            }

            // don't use the cache
            return memory_map_write(data, addr & 0x1fffffff, len);
#endif
        }
        break;
    case SH4_AREA_P1:
#ifdef ENABLE_SH4_OCACHE
        if (sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) {
            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_CB_MASK) {
                return sh4_ocache_write_cb(&sh4->op_cache, data, len, addr,
                                           index_enable, cache_as_ram);
            } else {
                return sh4_ocache_write_wt(&sh4->op_cache, data, len, addr,
                                           index_enable, cache_as_ram);
            }
        } else {
#endif
            return memory_map_write(data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
        }
#endif
        break;
    case SH4_AREA_P2:
        return memory_map_write(data, addr & 0x1fffffff, len);
    case SH4_AREA_P4:
        return sh4_do_write_p4(sh4, data, addr, len);
    default:
        break;
    }

    BOOST_THROW_EXCEPTION(IntegrityError() <<
                          errinfo_wtf("I don't believe it should be possible "
                                      "to get here (see Sh4::write_mem)"));
}

int sh4_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len) {
    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

#ifdef ENABLE_SH4_OCACHE
    bool index_enable =
        sh4->reg[SH4_REG_CCR] & SH4_CCR_OIX_MASK ? true : false;
    bool cache_as_ram =
        sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK ? true : false;
#endif

    if (virt_area != SH4_AREA_P0 && !privileged) {
        // TODO: allow user-mode access to the store queue area

        /*
         * The spec says user-mode processes can only write to the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
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

#ifdef ENABLE_SH4_OCACHE
            if ((utlb_ent->ent & SH4_UTLB_ENT_C_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK)) {
                // page is cacheable and the cache is enabled
                if (sh4->reg[SH4_REG_CCR] & SH4_CCR_WT_MASK) {
                    return sh4_ocache_read(&sh4->op_cache, data, len, paddr,
                                           index_enable, cache_as_ram);
                } else {
                    return sh4_ocache_read(&sh4->op_cache, data, len, paddr,
                                           index_enable, cache_as_ram);
                }
            } else {
#else
                // handle the case where OCE is enabled and ORA is
                // enabled but we don't have Ocache available
                if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                    (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                    sh4_in_oc_ram_area(paddr)) {
                    sh4_do_read_ora(sh4, data, paddr, len);
                    return 0;
                }
#endif

                // don't use the cache
                return memory_map_read(data, paddr & 0x1fffffff, len);
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
            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_WT_MASK) {
                return sh4_ocache_read(&sh4->op_cache, data, len, addr,
                                       index_enable, cache_as_ram);
            } else {
                return sh4_ocache_read(&sh4->op_cache, data, len, addr,
                                       index_enable, cache_as_ram);
            }
#else
            // handle the case where OCE is enabled and ORA is
            // enabled but we don't have Ocache available
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                sh4_in_oc_ram_area(addr)) {
                sh4_do_read_ora(sh4, data, addr, len);
                return 0;
            }

            // don't use the cache
            return memory_map_read(data, addr & 0x1fffffff, len);
#endif
        }
        break;
    case SH4_AREA_P1:
#ifdef ENABLE_SH4_OCACHE
        if (sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) {
            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_CB_MASK) {
                return sh4_ocache_read(&sh4->op_cache, data, len, addr,
                                       index_enable, cache_as_ram);
            } else {
                return sh4_ocache_read(&sh4->op_cache, data, len, addr,
                                       index_enable, cache_as_ram);
            }
        } else {
#endif
            return memory_map_read(data, addr & 0x1fffffff, len);
#ifdef ENABLE_SH4_OCACHE
        }
#endif
        break;
    case SH4_AREA_P2:
        return memory_map_read(data, addr & 0x1fffffff, len);
    case SH4_AREA_P4:
        return sh4_do_read_p4(sh4, data, addr, len);
    default:
        break;
    }

    return 1;
}

int sh4_do_read_p4(Sh4 *sh4, void *dat, addr32_t addr, unsigned len) {
    if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {
        return sh4_read_mem_mapped_reg(sh4, dat, addr, len);
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("reading from part of the P4 "
                                          "memory region") <<
                          errinfo_guest_addr(addr));

}

int sh4_do_write_p4(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len) {
    if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {
        return sh4_write_mem_mapped_reg(sh4, dat, addr, len);
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("writing to part of the P4 "
                                          "memory region") <<
                          errinfo_guest_addr(addr));
}


int sh4_read_inst(Sh4 *sh4, inst_t *out, addr32_t addr) {
    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

#ifdef ENABLE_SH4_ICACHE
    bool index_enable = sh4->reg[SH4_REG_CCR] &
        SH4_CCR_IIX_MASK ? true : false;
#endif

    if (virt_area != SH4_AREA_P0 && !privileged) {
        /*
         * The spec says user-mode processes can only access the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for unprivileged "
                                              "access to high memory areas"));
    }

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_AT_MASK) {
#ifdef ENABLE_SH4_MMU
            struct sh4_itlb_entry *itlb_ent = sh4_itlb_search(sh4, addr);

            if (!itlb_ent)
                return 1;  // exception set by sh4_itlb_search

            if (privileged || (itlb_ent->ent & SH4_ITLB_ENT_PR_MASK)) {
                addr32_t paddr = sh4_itlb_ent_translate(itlb_ent, addr);

#ifdef ENABLE_SH4_ICACHE
                if ((itlb_ent->ent & SH4_ITLB_ENT_C_MASK) &&
                    (sh4->reg[SH4_REG_CCR] & SH4_CCR_ICE_MASK)) {
                    // use the cache
                    boost::uint32_t buf;
                    int ret;
                    ret = sh4_icache_read(&sh4->inst_cache, &buf,
                                          paddr, index_enable);
                    *out = buf;
                    return ret;
                } else {
#endif
                    // don't use the cache
                    return memory_map_read(out, addr & 0x1fffffff,
                                           sizeof(*out));
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
            if (sh4->reg[SH4_REG_CCR] & SH4_CCR_ICE_MASK) {
                boost::uint32_t buf;
                int ret;
                ret = sh4_icache_read(&sh4->inst_cache, &buf,
                                      addr, index_enable);
                *out = buf;
                return ret;
            } else {
#endif
                return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
#ifdef ENABLE_SH4_ICACHE
            }
#endif
        }
        break;
    case SH4_AREA_P1:
#ifdef ENABLE_SH4_ICACHE
        if (sh4->reg[SH4_REG_CCR] & SH4_CCR_ICE_MASK) {
            boost::uint32_t buf;
            int ret;
            ret = sh4_icache_read(&sh4->inst_cache, &buf,
                                  addr, index_enable);
            *out = buf;
            return ret;
        } else {
#endif
            return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
#ifdef ENABLE_SH4_ICACHE
        }
#endif
        break;
    case SH4_AREA_P2:
        return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
    case SH4_AREA_P4:
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("CPU exception for reading "
                                              "instructions from the P4 "
                                              "memory area"));
    default:
        break;
    }

    return 1;
}

enum VirtMemArea sh4_get_mem_area(addr32_t addr) {
    if (addr <= SH4_AREA_P0_LAST)
        return SH4_AREA_P0;
    if (addr >= SH4_AREA_P1_FIRST && addr <= SH4_AREA_P1_LAST)
        return SH4_AREA_P1;
    if (addr >= SH4_AREA_P2_FIRST && addr <= SH4_AREA_P2_LAST)
        return SH4_AREA_P2;
    if (addr >= SH4_AREA_P3_FIRST && addr <= SH4_AREA_P3_LAST)
        return SH4_AREA_P3;
    return SH4_AREA_P4;
}

#ifndef ENABLE_SH4_OCACHE

void *sh4_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr) {
    addr32_t area_offset = paddr & 0xfff;
    addr32_t area_start;
    addr32_t mask;
    if (sh4->reg[SH4_REG_CCR] & SH4_CCR_OIX_MASK)
        mask = 1 << 25;
    else
        mask = 1 << 13;
    if (paddr & mask)
        area_start = SH4_OC_RAM_AREA_SIZE >> 1;
    else
        area_start = 0;
    return sh4->oc_ram_area + area_start + area_offset;
}

void sh4_do_write_ora(Sh4 *sh4, void const *dat, addr32_t paddr, unsigned len) {
    void *addr = sh4_get_ora_ram_addr(sh4, paddr);
    memcpy(addr, dat, len);
}

void sh4_do_read_ora(Sh4 *sh4, void *dat, addr32_t paddr, unsigned len) {
    void *addr = sh4_get_ora_ram_addr(sh4, paddr);
    memcpy(dat, addr, len);
}

#endif
