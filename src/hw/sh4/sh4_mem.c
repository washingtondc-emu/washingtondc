/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

static float sh4_do_read_p4_float(Sh4 *sh4, addr32_t addr);
static double sh4_do_read_p4_double(Sh4 *sh4, addr32_t addr);
static uint32_t sh4_do_read_p4_32(Sh4 *sh4, addr32_t addr);
static uint16_t sh4_do_read_p4_16(Sh4 *sh4, addr32_t addr);
static uint8_t sh4_do_read_p4_8(Sh4 *sh4, addr32_t addr);

static void sh4_do_write_p4_float(Sh4 *sh4, addr32_t addr, float val);
static void sh4_do_write_p4_double(Sh4 *sh4, addr32_t addr, double val);
static void sh4_do_write_p4_32(Sh4 *sh4, addr32_t addr, uint32_t val);
static void sh4_do_write_p4_16(Sh4 *sh4, addr32_t addr, uint16_t val);
static void sh4_do_write_p4_8(Sh4 *sh4, addr32_t addr, uint8_t val);

/*
 * TODO: need to adequately return control to the debugger when there's a memory
 * error and the debugger has its error-handler set up.  longjmp is the obvious
 * solution, but until all the codebase is out of C++ I don't want to risk that.
 */

#define SH4_WRITE_MEM_TMPL(type, postfix)                               \
    void sh4_write_mem_##postfix(Sh4 *sh4, type val, addr32_t addr) {   \
        enum VirtMemArea virt_area = sh4_get_mem_area(addr);            \
        switch (virt_area) {                                            \
        case SH4_AREA_P0:                                               \
        case SH4_AREA_P3:                                               \
            /*                                                          \
             * TODO: Check for MMUCR_AT_MASK in the MMUCR register and raise \
             * an error or do TLB lookups accordingly.                  \
             *                                                          \
             * currently it is impossible for this to be set because of the \
             * ERROR_UNIMPLEMENTED that gets raised if you set this bit in \
             * sh4_reg.c                                                \
             */                                                         \
                                                                        \
            /* handle the case where OCE is enabled and ORA is */       \
            /* enabled but we don't have Ocache available */            \
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&           \
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&           \
                sh4_ocache_in_ram_area(addr)) {                         \
                sh4_ocache_do_write_ora_##postfix(sh4, addr, val);      \
                return;                                                 \
            }                                                           \
                                                                        \
            /* don't use the cache */                                   \
            /* INTENTIONAL FALLTHROUGH */                               \
        case SH4_AREA_P1:                                               \
        case SH4_AREA_P2:                                               \
            memory_map_write_##postfix(val, addr & 0x1fffffff);         \
            return;                                                     \
        case SH4_AREA_P4:                                               \
            sh4_do_write_p4_##postfix(sh4, addr, val);                  \
            return;                                                     \
        default:                                                        \
            break;                                                      \
        }                                                               \
                                                                        \
        error_set_wtf("this should not be possible");                   \
        RAISE_ERROR(ERROR_INTEGRITY);                                   \
        exit(1); /* never happens */                                    \
    }                                                                   \

SH4_WRITE_MEM_TMPL(uint8_t, 8)
SH4_WRITE_MEM_TMPL(uint16_t, 16)
SH4_WRITE_MEM_TMPL(uint32_t, 32)
SH4_WRITE_MEM_TMPL(float, float)
SH4_WRITE_MEM_TMPL(double, double)

#define SH4_READ_MEM_TMPL(type, postfix)                                \
    type sh4_read_mem_##postfix(Sh4 *sh4, addr32_t addr) {              \
        enum VirtMemArea virt_area = sh4_get_mem_area(addr);            \
        switch (virt_area) {                                            \
        case SH4_AREA_P0:                                               \
        case SH4_AREA_P3:                                               \
            /*                                                          \
             * TODO: Check for MMUCR_AT_MASK in the MMUCR register and raise \
             * an error or do TLB lookups accordingly.                  \
             *                                                          \
             * currently it is impossible for this to be set because of the \
             * ERROR_UNIMPLEMENTED that gets raised if you set this bit in \
             * sh4_reg.c                                                \
             */                                                         \
                                                                        \
            /* handle the case where OCE is enabled and ORA is */       \
            /* enabled but we don't have Ocache available */            \
            if ((sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) &&           \
                (sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) &&           \
                sh4_ocache_in_ram_area(addr)) {                         \
                return sh4_ocache_do_read_ora_##postfix(sh4, addr);     \
            }                                                           \
                                                                        \
            /* don't use the cache */                                   \
            /* INTENTIONAL FALLTHROUGH */                               \
        case SH4_AREA_P1:                                               \
        case SH4_AREA_P2:                                               \
            return memory_map_read_##postfix(addr & 0x1fffffff);        \
        case SH4_AREA_P4:                                               \
            return sh4_do_read_p4_##postfix(sh4, addr);                 \
        default:                                                        \
            break;                                                      \
        }                                                               \
                                                                        \
        /* TODO: memory access exception ? */                           \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }

SH4_READ_MEM_TMPL(uint8_t, 8);
SH4_READ_MEM_TMPL(uint16_t, 16);
SH4_READ_MEM_TMPL(uint32_t, 32);
SH4_READ_MEM_TMPL(float, float);
SH4_READ_MEM_TMPL(double, double);

#define SH4_DO_WRITE_P4_TMPL(type, postfix)                             \
    static void sh4_do_write_p4_##postfix(Sh4 *sh4, addr32_t addr, type val) { \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            sh4_sq_write_##postfix(sh4, addr, val);                     \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            sh4_write_mem_mapped_reg_##postfix(sh4, addr, val);         \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            sh4_ocache_write_addr_array_##postfix(sh4, addr, val);  \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(val));                              \
            error_set_feature("writing to part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

SH4_DO_WRITE_P4_TMPL(uint8_t, 8)
SH4_DO_WRITE_P4_TMPL(uint16_t, 16)
SH4_DO_WRITE_P4_TMPL(uint32_t, 32)
SH4_DO_WRITE_P4_TMPL(float, float)
SH4_DO_WRITE_P4_TMPL(double, double)

#define SH4_DO_READ_P4_TMPL(type, postfix)                              \
    static type sh4_do_read_p4_##postfix(Sh4 *sh4, addr32_t addr) {     \
        type tmp_val;                                                   \
                                                                        \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            return sh4_sq_read_##postfix(sh4, addr);                    \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            return sh4_read_mem_mapped_reg_##postfix(sh4, addr);        \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            return sh4_ocache_read_addr_array_##postfix(sh4, addr);     \
        } else {                                                        \
            error_set_length(sizeof(type));                             \
            error_set_address(addr);                                    \
            error_set_feature("reading from part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
                                                                        \
        return tmp_val;                                                 \
    }

SH4_DO_READ_P4_TMPL(uint8_t, 8)
SH4_DO_READ_P4_TMPL(uint16_t, 16)
SH4_DO_READ_P4_TMPL(uint32_t, 32)
SH4_DO_READ_P4_TMPL(float, float)
SH4_DO_READ_P4_TMPL(double, double)

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
    if (addr <= SH4_AREA_P1_LAST)
        return SH4_AREA_P1;
    if (addr <= SH4_AREA_P2_LAST)
        return SH4_AREA_P2;
    if (addr <= SH4_AREA_P3_LAST)
        return SH4_AREA_P3;
    return SH4_AREA_P4;
}
