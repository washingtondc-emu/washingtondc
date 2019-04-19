/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include "sh4.h"
#include "washdc/MemoryMap.h"
#include "hw/sh4/sh4_ocache.h"
#include "hw/sh4/sh4_icache.h"

#ifdef ENABLE_DEBUGGER
#include "washdc/debugger.h"
#endif

static void sh4_do_write_p4_double(addr32_t addr, double val, void *ctxt);
static void sh4_do_write_p4_float(addr32_t addr, float val, void *ctxt);
static void sh4_do_write_p4_32(addr32_t addr, uint32_t val, void *ctxt);
static void sh4_do_write_p4_16(addr32_t addr, uint16_t val, void *ctxt);
static void sh4_do_write_p4_8(addr32_t addr, uint8_t val, void *ctxt);

static double sh4_do_read_p4_double(addr32_t addr, void *ctxt);
static float sh4_do_read_p4_float(addr32_t addr, void *ctxt);
static uint32_t sh4_do_read_p4_32(addr32_t addr, void *ctxt);
static uint16_t sh4_do_read_p4_16(addr32_t addr, void *ctxt);
static uint8_t sh4_do_read_p4_8(addr32_t addr, void *ctxt);

static int sh4_try_read_p4_32(addr32_t addr, uint32_t *valp, void *ctxt);
static int sh4_try_read_p4_16(addr32_t addr, uint16_t *valp, void *ctxt);
static int sh4_try_read_p4_8(addr32_t addr, uint8_t *valp, void *ctxt);
static int sh4_try_read_p4_float(addr32_t addr, float *valp, void *ctxt);
static int sh4_try_read_p4_double(addr32_t addr, double *valp, void *ctxt);

static int sh4_try_write_p4_32(addr32_t addr, uint32_t val, void *ctxt);
static int sh4_try_write_p4_16(addr32_t addr, uint16_t val, void *ctxt);
static int sh4_try_write_p4_8(addr32_t addr, uint8_t val, void *ctxt);
static int sh4_try_write_p4_float(addr32_t addr, float val, void *ctxt);
static int sh4_try_write_p4_double(addr32_t addr, double val, void *ctxt);

struct memory_interface sh4_p4_intf = {
    .readdouble = sh4_do_read_p4_double,
    .readfloat = sh4_do_read_p4_float,
    .read32 = sh4_do_read_p4_32,
    .read16 = sh4_do_read_p4_16,
    .read8 = sh4_do_read_p4_8,

    .writedouble = sh4_do_write_p4_double,
    .writefloat = sh4_do_write_p4_float,
    .write32 = sh4_do_write_p4_32,
    .write16 = sh4_do_write_p4_16,
    .write8 = sh4_do_write_p4_8,

    .try_readdouble = sh4_try_read_p4_double,
    .try_readfloat = sh4_try_read_p4_float,
    .try_read32 = sh4_try_read_p4_32,
    .try_read16 = sh4_try_read_p4_16,
    .try_read8 = sh4_try_read_p4_8,

    .try_writedouble = sh4_try_write_p4_double,
    .try_writefloat = sh4_try_write_p4_float,
    .try_write32 = sh4_try_write_p4_32,
    .try_write16 = sh4_try_write_p4_16,
    .try_write8 = sh4_try_write_p4_8
};

void sh4_mem_init(Sh4 *sh4) {
    sh4->mem.map = NULL;
}

void sh4_mem_cleanup(Sh4 *sh4) {
}

/*
 * TODO: need to adequately return control to the debugger when there's a memory
 * error and the debugger has its error-handler set up.  longjmp is the obvious
 * solution, but until all the codebase is out of C++ I don't want to risk that.
 */

#define SH4_DO_WRITE_P4_TMPL(type, postfix)                             \
    static void                                                         \
    sh4_do_write_p4_##postfix(addr32_t addr, type val, void *ctxt) {    \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            sh4_sq_write_##postfix(sh4, addr, val);                     \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            sh4_write_mem_mapped_reg_##postfix(sh4, addr, val);         \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            sh4_ocache_write_addr_array_##postfix(sh4, addr, val);      \
        } else if (addr >= SH4_IC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_IC_ADDR_ARRAY_LAST) {                    \
            sh4_icache_write_addr_array_##postfix(sh4, addr, val);      \
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

#define SH4_TRY_WRITE_P4_TMPL(type, postfix)                            \
    static int                                                          \
    sh4_try_write_p4_##postfix(addr32_t addr, type val, void *ctxt) {   \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            sh4_sq_write_##postfix(sh4, addr, val);                     \
            return 0;                                                   \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            sh4_write_mem_mapped_reg_##postfix(sh4, addr, val);         \
            return 0;                                                   \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            sh4_ocache_write_addr_array_##postfix(sh4, addr, val);      \
            return 0;                                                   \
        } else if (addr >= SH4_IC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_IC_ADDR_ARRAY_LAST) {                    \
            sh4_icache_write_addr_array_##postfix(sh4, addr, val);      \
            return 0;                                                   \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

SH4_TRY_WRITE_P4_TMPL(uint8_t, 8)
SH4_TRY_WRITE_P4_TMPL(uint16_t, 16)
SH4_TRY_WRITE_P4_TMPL(uint32_t, 32)
SH4_TRY_WRITE_P4_TMPL(float, float)
SH4_TRY_WRITE_P4_TMPL(double, double)

#define SH4_DO_READ_P4_TMPL(type, postfix)                              \
    static type sh4_do_read_p4_##postfix(addr32_t addr, void *ctxt) {   \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
                                                                        \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            return sh4_sq_read_##postfix(sh4, addr);                    \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            return sh4_read_mem_mapped_reg_##postfix(sh4, addr);        \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            return sh4_ocache_read_addr_array_##postfix(sh4, addr);     \
        } else if (addr >= SH4_IC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_IC_ADDR_ARRAY_LAST) {                    \
            return sh4_icache_read_addr_array_##postfix(sh4, addr);     \
        } else {                                                        \
            error_set_length(sizeof(type));                             \
            error_set_address(addr);                                    \
            error_set_feature("reading from part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

SH4_DO_READ_P4_TMPL(uint8_t, 8)
SH4_DO_READ_P4_TMPL(uint16_t, 16)
SH4_DO_READ_P4_TMPL(uint32_t, 32)
SH4_DO_READ_P4_TMPL(float, float)
SH4_DO_READ_P4_TMPL(double, double)

#define SH4_TRY_READ_P4_TMPL(type, postfix)                             \
    static int sh4_try_read_p4_##postfix(addr32_t addr, type *valp,     \
                                         void *ctxt) {                  \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
                                                                        \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            *valp = sh4_sq_read_##postfix(sh4, addr);                   \
            return 0;                                                   \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            *valp = sh4_read_mem_mapped_reg_##postfix(sh4, addr);       \
            return 0;                                                   \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            *valp = sh4_ocache_read_addr_array_##postfix(sh4, addr);    \
            return 0;                                                   \
        } else if (addr >= SH4_IC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_IC_ADDR_ARRAY_LAST) {                    \
            *valp = sh4_icache_read_addr_array_##postfix(sh4, addr);    \
            return 0;                                                   \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

SH4_TRY_READ_P4_TMPL(uint8_t, 8)
SH4_TRY_READ_P4_TMPL(uint16_t, 16)
SH4_TRY_READ_P4_TMPL(uint32_t, 32)
SH4_TRY_READ_P4_TMPL(float, float)
SH4_TRY_READ_P4_TMPL(double, double)

void sh4_set_mem_map(struct Sh4 *sh4, struct memory_map *map) {
    sh4->mem.map = map;
}
