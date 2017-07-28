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

#include <stdlib.h>

#include "sh4_mmu.h"
#include "sh4_excp.h"
#include "sh4_mem.h"
#include "sh4_ocache.h"
#include "sh4.h"
#include "dreamcast.h"
#include "MemoryMap.h"
#include "mem_code.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

static inline enum VirtMemArea sh4_get_mem_area(addr32_t addr);

/*
 * TODO: need to adequately return control to the debugger when there's a memory
 * error and the debugger has its error-handler set up.  longjmp is the obvious
 * solution, but until all the codebase is out of C++ I don't want to risk that.
 */

int sh4_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {
#ifdef ENABLE_DEBUGGER
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg && debug_is_w_watch(dbg, addr, len)) {
        sh4->aborted_operation = true;
        return MEM_ACCESS_EXC;
    }
#endif

    int ret;

    if ((ret = sh4_do_write_mem(sh4, data, addr, len)) == MEM_ACCESS_FAILURE)
        RAISE_ERROR(get_error_pending());
    return ret;
}

int sh4_do_write_mem(Sh4 *sh4, void const *data, addr32_t addr, unsigned len) {

    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

#if 0
    /*
     * this is commented out because you can't leave privileged mode without
     * raising an EROR_UNIMPLEMENTED (see sh4_on_sr_change in sh4.c)
     */
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
        error_set_feature("CPU exception for unprivileged "
                          "access to high memory areas");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }
#endif

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        /*
         * TODO: Check for MMUCR_AT_MASK in the MMUCR register and raise an
         * error or do TLB lookups accordingly.
         *
         * currently it is impossible for this to be set because of the
         * ERROR_UNIMPLEMENTED that gets raised if you set this bit in sh4_reg.c
         */

        // handle the case where OCE is enabled and ORA is
        // enabled but we don't have Ocache available
        if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
            (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
            sh4_ocache_in_ram_area(addr)) {
            sh4_ocache_do_write_ora(sh4, data, addr, len);
            return MEM_ACCESS_SUCCESS;
        }

        // don't use the cache
        // INTENTIONAL FALLTHROUGH
    case SH4_AREA_P1:
    case SH4_AREA_P2:
        return memory_map_write(data, addr & 0x1fffffff, len);
    case SH4_AREA_P4:
        return sh4_do_write_p4(sh4, data, addr, len);
    default:
        break;
    }

    error_set_wtf("this should not be possible");
    RAISE_ERROR(ERROR_INTEGRITY);
    exit(1); // never happens
}

int sh4_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len) {
#ifdef ENABLE_DEBUGGER
    struct debugger *dbg = dreamcast_get_debugger();
    if (dbg && debug_is_r_watch(dbg, addr, len)) {
        sh4->aborted_operation = true;
        return MEM_ACCESS_EXC;
    }
#endif
    int ret;

    if ((ret = sh4_do_read_mem(sh4, data, addr, len)) == MEM_ACCESS_FAILURE)
        RAISE_ERROR(get_error_pending());
    return ret;
}

int sh4_do_read_mem(Sh4 *sh4, void *data, addr32_t addr, unsigned len) {

    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

#if 0
    /*
     * this is commented out because you can't leave privileged mode without
     * raising an EROR_UNIMPLEMENTED (see sh4_on_sr_change in sh4.c)
     */
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
        error_set_feature("CPU exception for unprivileged "
                          "access to high memory areas");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }
#endif

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        /*
         * TODO: Check for MMUCR_AT_MASK in the MMUCR register and raise an
         * error or do TLB lookups accordingly.
         *
         * currently it is impossible for this to be set because of the
         * ERROR_UNIMPLEMENTED that gets raised if you set this bit in sh4_reg.c
         */

        // handle the case where OCE is enabled and ORA is
        // enabled but we don't have Ocache available
        if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&
            (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&
            sh4_ocache_in_ram_area(addr)) {
            sh4_ocache_do_read_ora(sh4, data, addr, len);
            return MEM_ACCESS_SUCCESS;
        }

        // don't use the cache
        // INTENTIONAL FALLTHROUGH
    case SH4_AREA_P1:
    case SH4_AREA_P2:
        return memory_map_read(data, addr & 0x1fffffff, len);
    case SH4_AREA_P4:
        return sh4_do_read_p4(sh4, data, addr, len);
    default:
        break;
    }

    return MEM_ACCESS_EXC;
}

int sh4_do_read_p4(Sh4 *sh4, void *dat, addr32_t addr, unsigned len) {
    if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL)
        return sh4_sq_read(sh4, dat, addr, len);

    if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {
        return sh4_read_mem_mapped_reg(sh4, dat, addr, len);
    }

    if (addr >= SH4_OC_ADDR_ARRAY_FIRST && addr <= SH4_OC_ADDR_ARRAY_LAST) {
        sh4_ocache_read_addr_array(sh4, dat, addr, len);
        return MEM_ACCESS_SUCCESS;
    }

    error_set_address(addr);
    error_set_feature("reading from part of the P4 memory region");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int sh4_do_write_p4(Sh4 *sh4, void const *dat, addr32_t addr, unsigned len) {
    if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL)
        return sh4_sq_write(sh4, dat, addr, len);

    if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {
        return sh4_write_mem_mapped_reg(sh4, dat, addr, len);
    }

    if (addr >= SH4_OC_ADDR_ARRAY_FIRST && addr <= SH4_OC_ADDR_ARRAY_LAST) {
        sh4_ocache_write_addr_array(sh4, dat, addr, len);
        return MEM_ACCESS_SUCCESS;
    }

    error_set_address(addr);
    error_set_feature("writing to part of the P4 memory region");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}


int sh4_read_inst(Sh4 *sh4, inst_t *out, addr32_t addr) {
    enum VirtMemArea virt_area = sh4_get_mem_area(addr);

#if 0
    /*
     * this is commented out because you can't leave privileged mode without
     * raising an EROR_UNIMPLEMENTED (see sh4_on_sr_change in sh4.c)
     */
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
        error_set_feature("CPU exception for unprivileged "
                          "access to high memory areas");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }
#endif

    switch (virt_area) {
    case SH4_AREA_P0:
    case SH4_AREA_P3:
        /*
         * TODO: Check for MMUCR_AT_MASK in the MMUCR register and raise an
         * error or do TLB lookups accordingly.
         *
         * currently it is impossible for this to be set because of the
         * ERROR_UNIMPLEMENTED that gets raised if you set this bit in sh4_reg.c
         */
    case SH4_AREA_P1:
    case SH4_AREA_P2:
        return memory_map_read(out, addr & 0x1fffffff, sizeof(*out));
    case SH4_AREA_P4:
        error_set_feature("CPU exception for reading instructions from the P4 "
                          "memory area");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    default:
        break;
    }

    return MEM_ACCESS_EXC;
}

static inline enum VirtMemArea sh4_get_mem_area(addr32_t addr) {
    /*
     * XXX I tried replacing this block of if statements with a lookup table,
     * but somehow it turned out to be slower that way.  This is possibly
     * because the lookup-table was not in the cache and had to be fetched from
     * memory.
     *
     * If you ever want to look into this again, the trick is to use the upper
     * four bits as the index into the lookup table (P0 will be 0-7,
     * P1 will be 8-9, etc.)
     */
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
