/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2020 snickerbockers
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
#include "log.h"
#include "intmath.h"

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

static void
sh4_utlb_addr_array_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_utlb_addr_array_read(struct Sh4 *sh4, addr32_t addr);
static void
sh4_utlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_utlb_data_array_1_read(struct Sh4 *sh4, addr32_t addr);
static void
sh4_utlb_data_array_2_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_utlb_data_array_2_read(struct Sh4 *sh4, addr32_t addr);
static void
sh4_itlb_addr_array_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_itlb_addr_array_read(struct Sh4 *sh4, addr32_t addr);
static void
sh4_itlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_itlb_data_array_1_read(struct Sh4 *sh4, addr32_t addr);
static void
sh4_itlb_data_array_2_write(struct Sh4 *sh4, addr32_t addr, uint32_t val);
static uint32_t
sh4_itlb_data_array_2_read(struct Sh4 *sh4, addr32_t addr);

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
        } else if (sizeof(val) == 4) {                                  \
            if (addr >= SH4_P4_ITLB_ADDR_ARRAY_FIRST &&                 \
                addr <= SH4_P4_ITLB_ADDR_ARRAY_LAST) {                  \
                sh4_itlb_addr_array_write(sh4,  addr, val);             \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_1_LAST) {         \
                sh4_itlb_data_array_1_write(sh4, addr, val);            \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_2_LAST) {         \
                sh4_itlb_data_array_2_write(sh4, addr, val);            \
            } else if (addr >= SH4_P4_UTLB_ADDR_ARRAY_FIRST &&          \
                       addr <= SH4_P4_UTLB_ADDR_ARRAY_LAST) {           \
                sh4_utlb_addr_array_write(sh4, addr, val);              \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_1_LAST) {         \
                sh4_utlb_data_array_1_write(sh4, addr, val);            \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_2_LAST) {         \
                sh4_utlb_data_array_2_write(sh4, addr, val);            \
            } else {                                                    \
                error_set_address(addr);                                \
                error_set_length(sizeof(val));                          \
                error_set_feature("writing to part of the P4 memory region"); \
                RAISE_ERROR(ERROR_UNIMPLEMENTED);                       \
            }                                                           \
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
        } else if (sizeof(val) == 4) {                                  \
            if (addr >= SH4_P4_ITLB_ADDR_ARRAY_FIRST &&                 \
                addr <= SH4_P4_ITLB_ADDR_ARRAY_LAST) {                  \
                sh4_itlb_addr_array_write(sh4,  addr, val);             \
                return 0;                                               \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_1_LAST) {         \
                sh4_itlb_data_array_1_write(sh4, addr, val);            \
                return 0;                                               \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_2_LAST) {         \
                sh4_itlb_data_array_2_write(sh4, addr, val);            \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_ADDR_ARRAY_FIRST &&          \
                       addr <= SH4_P4_UTLB_ADDR_ARRAY_LAST) {           \
                sh4_utlb_addr_array_write(sh4, addr, val);              \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_1_LAST) {         \
                sh4_utlb_data_array_1_write(sh4, addr, val);            \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_2_LAST) {         \
                sh4_utlb_data_array_2_write(sh4, addr, val);            \
                return 0;                                               \
            } else {                                                    \
                return -1;                                              \
            }                                                           \
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
        } else if (addr == 0xfffffffc) {                                \
            /* see the Guilty Gear X section of bullshit.txt */         \
            LOG_INFO("UNKNOWN READ ADDRESS 0x%08x %u BYTES (PC=0x%08x)\n", \
                     (int)addr, (int)sizeof(type), (int)sh4->reg[SH4_REG_PC]); \
            return 0;                                                   \
        } else if (sizeof(type) == 4) {                                 \
            if (addr >= SH4_P4_ITLB_ADDR_ARRAY_FIRST &&                 \
                addr <= SH4_P4_ITLB_ADDR_ARRAY_LAST) {                  \
                return sh4_itlb_addr_array_read(sh4, addr);             \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_1_LAST) {         \
                return sh4_itlb_data_array_1_read(sh4, addr);           \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_2_LAST) {         \
                return sh4_itlb_data_array_2_read(sh4, addr);           \
            } else if (addr >= SH4_P4_UTLB_ADDR_ARRAY_FIRST &&          \
                       addr <= SH4_P4_UTLB_ADDR_ARRAY_LAST) {           \
                return sh4_utlb_addr_array_read(sh4, addr);             \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_1_LAST) {         \
                return sh4_utlb_data_array_1_read(sh4, addr);           \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_2_LAST) {         \
                return sh4_utlb_data_array_2_read(sh4, addr);           \
            } else {                                                    \
                error_set_address(addr);                                \
                error_set_length(sizeof(type));                         \
                error_set_feature("writing to part of the P4 memory region"); \
                RAISE_ERROR(ERROR_UNIMPLEMENTED);                       \
            }                                                           \
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
        } else if (addr == 0xfffffffc) {                                \
            /* see the Guilty Gear X section of bullshit.txt */         \
            *valp = 0;                                                  \
            LOG_INFO("UNKNOWN READ ADDRESS 0x%08x %u BYTES PC=0x%08x\n", \
                     (int)addr, (int)sizeof(type), (int)sh4->reg[SH4_REG_PC]); \
            return 0;                                                   \
        } else if (sizeof(type) == 4) {                                 \
            if (addr >= SH4_P4_ITLB_ADDR_ARRAY_FIRST &&                 \
                addr <= SH4_P4_ITLB_ADDR_ARRAY_LAST) {                  \
                *valp = sh4_itlb_addr_array_read(sh4, addr);            \
                return 0;                                               \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_1_LAST) {         \
                *valp = sh4_itlb_data_array_1_read(sh4, addr);          \
                return 0;                                               \
            } else if (addr >= SH4_P4_ITLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_ITLB_DATA_ARRAY_2_LAST) {         \
                *valp = sh4_itlb_data_array_2_read(sh4, addr);          \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_ADDR_ARRAY_FIRST &&          \
                       addr <= SH4_P4_UTLB_ADDR_ARRAY_LAST) {           \
                *valp = sh4_utlb_addr_array_read(sh4, addr);            \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_1_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_1_LAST) {         \
                *valp = sh4_utlb_data_array_1_read(sh4, addr);          \
                return 0;                                               \
            } else if (addr >= SH4_P4_UTLB_DATA_ARRAY_2_FIRST &&        \
                       addr <= SH4_P4_UTLB_DATA_ARRAY_2_LAST) {         \
                *valp = sh4_utlb_data_array_2_read(sh4, addr);          \
                return 0;                                               \
            } else {                                                    \
                return -1;                                              \
            }                                                           \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

SH4_TRY_READ_P4_TMPL(uint8_t, 8)
SH4_TRY_READ_P4_TMPL(uint16_t, 16)
SH4_TRY_READ_P4_TMPL(uint32_t, 32)
SH4_TRY_READ_P4_TMPL(float, float)
SH4_TRY_READ_P4_TMPL(double, double)

static uint32_t vpn_mask_for_size(enum sh4_tlb_page_sz sz) {
    switch (sz) {
    case SH4_TLB_PAGE_1KB:
        return ~0x3ff;
    case SH4_TLB_PAGE_4KB:
        return ~0xfff;
    case SH4_TLB_PAGE_64KB:
        return ~0xffff;
    case SH4_TLB_PAGE_1MB:
        return ~0xfffff;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

// vpn should be shifted such that the MSB is at bit 31
static struct sh4_utlb_ent *
sh4_utlb_find_ent_associative(struct Sh4 *sh4, uint32_t vpn) {
    struct sh4_utlb_ent *ent = NULL;
    unsigned asid = sh4->reg[SH4_REG_PTEH] & BIT_RANGE(0, 7);
    unsigned idx;
    for (idx = 0; idx < SH4_UTLB_LEN; idx++) {
        struct sh4_utlb_ent *curs = sh4->mem.utlb + idx;
        uint32_t mask = vpn_mask_for_size(curs->sz);
        if ((curs->vpn & mask) == (vpn & mask) && curs->asid == asid) {
            if (ent) {
                error_set_feature("UTLB multiple hit exception");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
            ent = curs;
            return ent;
        }
    }
    return NULL;
}

// vpn should be shifted such that the MSB is at bit 31
static struct sh4_itlb_ent *
sh4_itlb_find_ent_associative(struct Sh4 *sh4, uint32_t vpn) {
    struct sh4_itlb_ent *ent = NULL;
    unsigned asid = sh4->reg[SH4_REG_PTEH] & BIT_RANGE(0, 7);
    unsigned idx;
    for (idx = 0; idx < SH4_ITLB_LEN; idx++) {
        struct sh4_itlb_ent *curs = sh4->mem.itlb + idx;
        uint32_t mask = vpn_mask_for_size(curs->sz);
        if ((curs->vpn & mask) == (vpn & mask) && curs->asid == asid) {
            if (ent) {
                error_set_feature("ITLB multiple hit exception");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
            ent = curs;
            return ent;
        }
    }
    return NULL;
}

void sh4_set_mem_map(struct Sh4 *sh4, struct memory_map *map) {
    sh4->mem.map = map;
}

static void sh4_utlb_addr_array_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    struct sh4_utlb_ent *ent;
    bool associative = (addr >> 7) & 1 ? true : false;
    bool valid = (val >> 8) & 1 ? true : false;
    bool dirty = (val >> 9) & 1 ? true : false;

    uint32_t vpn = (val & BIT_RANGE(10, 31)) >> 10;
    uint32_t asid = val & BIT_RANGE(0, 7);

    if (associative)
        LOG_WARN("UTLB ADDRESS ARRAY ASSOCIATIVE WRITE %08X to %08X\n",
                 (unsigned)val, (unsigned)addr);
    else
        LOG_WARN("UTLB ADDRESS ARRAY NON-ASSOCIATIVE WRITE %08X to %08X\n",
                 (unsigned)val, (unsigned)addr);

    if (associative) {
        /*
         * XXX SH4 spec is pretty vague about how this UTLB->ITLB propagation
         * works, so some of this may be wrong.  If I understand correctly, then
         * the valid bit gets copied over if it matches the ITLB but not the
         * UTLB...and then if the UTLB matches too then it copies else everything over as well?
         *
         * I feel like there's an MMU hardware test coming...
         */
        struct sh4_itlb_ent *itlb_ent = sh4_itlb_find_ent_associative(sh4, vpn);
        if (itlb_ent)
            itlb_ent->valid = valid;

        if (!(ent = sh4_utlb_find_ent_associative(sh4, vpn)))
            return;

        ent->valid = valid;
        ent->dirty = dirty;

        if (itlb_ent) {
            itlb_ent->asid = ent->asid;
            itlb_ent->vpn = ent->vpn;
            itlb_ent->ppn = ent->ppn;
            itlb_ent->protection = ent->protection & 2;
            itlb_ent->sa = ent->sa;
            itlb_ent->sz = ent->sz;
            itlb_ent->shared = ent->shared;
            itlb_ent->cacheable = ent->cacheable;
            itlb_ent->tc = ent->tc;
        }
    } else {
        unsigned idx = (addr >> 8) & 0x3f;
        ent = sh4->mem.utlb + idx;

        ent->vpn = vpn;
        ent->asid = asid;
        ent->dirty = dirty;
        ent->valid = valid;
    }
}

static uint32_t sh4_utlb_addr_array_read(struct Sh4 *sh4, addr32_t addr) {
    // associative access is never performed for reads
    unsigned idx = (addr >> 8) & 0x3f;
    struct sh4_utlb_ent *ent = sh4->mem.utlb + idx;
    return (ent->vpn << 10) | (ent->dirty << 9) |
        (ent->valid << 8) | (ent->asid & BIT_RANGE(0,7));
}

static void
sh4_utlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    LOG_WARN("UTLB DATA ARRAY 1 WRITE %08X to %08X\n",
             (unsigned)val, (unsigned)addr);

    unsigned idx = (addr >> 8) & 0x3f;
    struct sh4_utlb_ent *ent = sh4->mem.utlb + idx;

    unsigned ppn = (val & BIT_RANGE(10, 28)) >> 10;
    enum sh4_tlb_page_sz sz =
        (enum sh4_tlb_page_sz)(((val >> 4) & 1) | ((val >> 6) & 2));
    bool valid = (val >> 8) & 1;
    unsigned protection = (val >> 5) & 3;
    bool cacheable = (val >> 3) & 1;
    bool dirty = (val >> 2) & 1;
    bool shared = (val >> 1) & 1;
    bool wt = val & 1;

    ent->ppn = ppn;
    ent->sz = sz;
    ent->valid = valid;
    ent->protection = protection;
    ent->cacheable = cacheable;
    ent->dirty = dirty;
    ent->shared = shared;
    ent->wt = wt;
}

static uint32_t sh4_utlb_data_array_1_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_utlb_data_array_2_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t sh4_utlb_data_array_2_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_itlb_addr_array_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t sh4_itlb_addr_array_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_itlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t sh4_itlb_data_array_1_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_itlb_data_array_2_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t sh4_itlb_data_array_2_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
