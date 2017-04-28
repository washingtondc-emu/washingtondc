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

#include "sh4_mmu.h"
#include "sh4_excp.h"
#include "sh4_mem.h"
#include "sh4.hpp"
#include "Dreamcast.hpp"
#include "MemoryMap.hpp"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

int sh4_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {
#ifdef ENABLE_DEBUGGER
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg && debug_is_w_watch(dbg, addr, len)) {
        sh4->aborted_operation = true;
        return 1;
    }
#endif

    return sh4_do_write_mem(sh4, data, addr, len);
}

int sh4_do_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {

    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

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
            return sh4_mmu_write_mem(sh4, data, addr, len);
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON"
                                                 " and rebuild"));
#endif
        } else {
            // handle the case where OCE is enabled and ORA is
            // enabled but we don't have Ocache available
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                sh4_ocache_in_ram_area(addr)) {
                sh4_ocache_do_write_ora(sh4, data, addr, len);
                return 0;
            }

            // don't use the cache
            return memory_map_write(data, addr & 0x1fffffff, len);
        }
        break;
    case SH4_AREA_P1:
        return memory_map_write(data, addr & 0x1fffffff, len);
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
#ifdef ENABLE_DEBUGGER
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg && debug_is_r_watch(dbg, addr, len)) {
        sh4->aborted_operation = true;
        return 1;
    }
#endif

    return sh4_do_read_mem(sh4, data, addr, len);
}

int sh4_do_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len) {

    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

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
            return sh4_mmu_read_mem(sh4, data, addr, len);
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON "
                                                 "and rebuild"));

#endif
        } else {
            // handle the case where OCE is enabled and ORA is
            // enabled but we don't have Ocache available
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
                sh4_ocache_in_ram_area(addr)) {
                sh4_ocache_do_read_ora(sh4, data, addr, len);
                return 0;
            }

            // don't use the cache
            return memory_map_read(data, addr & 0x1fffffff, len);
        }
        break;
    case SH4_AREA_P1:
        return memory_map_read(data, addr & 0x1fffffff, len);
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
    if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL)
        return sh4_sq_write(sh4, dat, addr, len);

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
            return sh4_mmu_read_inst(sh4, out, addr);
#else // ifdef ENABLE_SH4_MMU
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("MMU") <<
                                  errinfo_advice("run cmake with "
                                                 "-DENABLE_SH4_MMU=ON "
                                                  "and rebuild"));
#endif
        } else {
            return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
        }
        break;
    case SH4_AREA_P1:
        return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
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
