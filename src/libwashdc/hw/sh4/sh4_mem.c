/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2020, 2022 snickerbockers
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
        if (sh4_addr_in_sq_area(addr)) {                                \
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
        if (sh4_addr_in_sq_area(addr)) {                                \
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
        if (sh4_addr_in_sq_area(addr)) {                                \
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
            } else if (addr >= 0xe4000000 && addr < 0xf0000000) {       \
                /* SEGA Tetris does this. */                            \
                /* TODO: is returning 0 the correct behavior? */        \
                LOG_WARN("Reading from addr %08X in SH4 P4 "            \
                         "reserved area.\n", (unsigned)addr);           \
                return 0;                                               \
            } else {                                                    \
                error_set_address(addr);                                \
                error_set_length(sizeof(type));                         \
                error_set_feature("reading from part of the P4 memory " \
                                  "region");                            \
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
        if (sh4_addr_in_sq_area(addr)) {                                \
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

static inline uint32_t vpn_mask_for_size(enum sh4_tlb_page_sz sz) {
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

static inline uint32_t ppn_mask_for_size(enum sh4_tlb_page_sz sz) {
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

static inline uint32_t page_offset_mask_for_size(enum sh4_tlb_page_sz sz) {
    switch (sz) {
    case SH4_TLB_PAGE_1KB:
        return 0x3ff;
    case SH4_TLB_PAGE_4KB:
        return 0xfff;
    case SH4_TLB_PAGE_64KB:
        return 0xffff;
    case SH4_TLB_PAGE_1MB:
        return 0xfffff;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static bool
asid_check(struct Sh4 *sh4, bool shared, unsigned asid1, unsigned asid2) {
    if (!shared &&
        (!(sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SV_MASK) ||
         !(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK))) {
        return asid1 == asid2;
    } else {
        return true;
    }
}

// vpn should be shifted such that the MSB is at bit 31
struct sh4_utlb_ent *
sh4_utlb_find_ent_associative(struct Sh4 *sh4, uint32_t vpn) {
    struct sh4_utlb_ent *ent = NULL;
    unsigned asid = sh4->reg[SH4_REG_PTEH] & BIT_RANGE(0, 7);
    unsigned idx;
    /* printf("search for VPN %08X\n", (unsigned)vpn); */
    for (idx = 0; idx < SH4_UTLB_LEN; idx++) {
        struct sh4_utlb_ent *curs = sh4->mem.utlb + idx;
        if (!curs->valid)
            continue;
        uint32_t mask = vpn_mask_for_size(curs->sz);
        /* printf("compare %08X to %08X\n", (unsigned)(curs->vpn & mask), (unsigned)(vpn & mask)); */
        if ((curs->vpn & mask) == (vpn & mask) && asid_check(sh4, curs->shared, asid, curs->asid)) {
            if (ent) {
                error_set_feature("UTLB multiple hit exception");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
            ent = curs;
        }
    }
    return ent;
}

// vpn should be shifted such that the MSB is at bit 31
struct sh4_itlb_ent *
sh4_itlb_find_ent_associative(struct Sh4 *sh4, uint32_t vpn) {
    struct sh4_itlb_ent *ent = NULL;
    unsigned asid = sh4->reg[SH4_REG_PTEH] & BIT_RANGE(0, 7);
    unsigned idx;
    for (idx = 0; idx < SH4_ITLB_LEN; idx++) {
        struct sh4_itlb_ent *curs = sh4->mem.itlb + idx;
        if (!curs->valid)
            continue;
        uint32_t mask = vpn_mask_for_size(curs->sz);
        if ((curs->vpn & mask) == (vpn & mask) && asid_check(sh4, curs->shared, asid, curs->asid)) {
            if (ent) {
                error_set_feature("ITLB multiple hit exception");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
            ent = curs;
        }
    }
    if (!ent)
        SH4_MEM_TRACE("FAILED TO LOCATE ITLB ENTRY FOR VPN %08X ASID %08X\n",
                      (unsigned)vpn, asid);
    return ent;
}

void sh4_set_mem_map(struct Sh4 *sh4, struct memory_map *map) {
    sh4->mem.map = map;
}

static void sh4_utlb_addr_array_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    struct sh4_utlb_ent *ent;
    bool associative = (addr >> 7) & 1 ? true : false;
    bool valid = (val >> 8) & 1 ? true : false;
    bool dirty = (val >> 9) & 1 ? true : false;

    uint32_t vpn = (val & BIT_RANGE(10, 31));
    uint32_t asid = val & BIT_RANGE(0, 7);

    if (associative)
        SH4_MEM_TRACE("UTLB ADDRESS ARRAY ASSOCIATIVE WRITE %08X TO %08X\n",
                      (unsigned)val, (unsigned)addr);
    else
        SH4_MEM_TRACE("UTLB ADDRESS ARRAY NON-ASSOCIATIVE WRITE %08X TO %08X\n",
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

        /*
         * This seems to work, but I'm still a little uneasy due to vague
         * SH4 documentation.  NetBSD uses this to invalidate pages.  Windows
         * does not appear to use it at all.
         */
        SH4_MEM_TRACE("%s CALLED - RADICALLY UNTESTED UTLB ASSOCIATIVE ADDRESS "
                      "ARRAY WRITE\n", __func__);

        struct sh4_itlb_ent *itlb_ent = sh4_itlb_find_ent_associative(sh4, vpn);
        if (itlb_ent)
            itlb_ent->valid = valid;

        if (!(ent = sh4_utlb_find_ent_associative(sh4, vpn)))
            return;

        ent->valid = valid;
        ent->dirty = dirty;

        SH4_MEM_TRACE("UTLB INDEX %u:\n"
                      "\tVPN %08X\n"
                      "\tDIRTY %s\n"
                      "\tVALID %s\n",
                      ent - sh4->mem.utlb,
                      (unsigned)vpn,
                      dirty ? "TRUE" : "FALSE",
                      valid ? "TRUE" : "FALSE");

        if (itlb_ent) {
            SH4_MEM_TRACE("UNTESTED UTLB->ITLB TRANSFER\n");
            itlb_ent->asid = ent->asid;
            itlb_ent->vpn = ent->vpn;
            itlb_ent->ppn = ent->ppn;
            itlb_ent->protection = ent->protection & 2 ? 1 : 0;
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

        SH4_MEM_TRACE("UTLB INDEX %u:\n"
                      "\tVPN %08X\n"
                      "\tDIRTY %s\n"
                      "\tVALID %s\n",
                      idx, (unsigned)vpn,
                      dirty ? "TRUE" : "FALSE",
                      valid ? "TRUE" : "FALSE");
    }
}

static uint32_t sh4_utlb_addr_array_read(struct Sh4 *sh4, addr32_t addr) {
    // associative access is never performed for reads
    unsigned idx = (addr >> 8) & 0x3f;
    struct sh4_utlb_ent *ent = sh4->mem.utlb + idx;
    return ent->vpn | (ent->dirty << 9) |
        (ent->valid << 8) | (ent->asid & BIT_RANGE(0,7));
}

static void
sh4_utlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    SH4_MEM_TRACE("UTLB DATA ARRAY 1 WRITE %08X TO %08X\n",
                  (unsigned)val, (unsigned)addr);

    unsigned idx = (addr >> 8) & 0x3f;
    struct sh4_utlb_ent *ent = sh4->mem.utlb + idx;
    if (!ent) {
        error_set_address(addr);
        error_set_feature("page fault exceptions");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned ppn = (val & BIT_RANGE(10, 28));
    enum sh4_tlb_page_sz sz =
        (enum sh4_tlb_page_sz)(((val >> 4) & 1) | ((val >> 6) & 2));
    bool valid = (val >> 8) & 1;
    unsigned protection = (val >> 5) & 3;
    bool cacheable = (val >> 3) & 1;
    bool dirty = (val >> 2) & 1;
    bool shared = (val >> 1) & 1;
    bool wt = val & 1;

#if defined ENABLE_LOG_DEBUG && defined(SUPER_VERBOSE_MEM_TRACE)
    char const *page_sz;
    switch (sz) {
    case SH4_TLB_PAGE_1KB:
        page_sz = "1KB";
        break;
    case SH4_TLB_PAGE_4KB:
        page_sz = "4KB";
        break;
    case SH4_TLB_PAGE_64KB:
        page_sz = "64KB";
        break;
    case SH4_TLB_PAGE_1MB:
        page_sz = "1MB";
        break;
    default:
        // should never happen
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    SH4_MEM_TRACE("UTLB INDEX %u:\n"
                  "\tPPN %08X\n"
                  "\tPAGE SIZE %s\n"
                  "\tVALID %s\n"
                  "\tPROTECTION %02X\n"
                  "\tCACHEABLE %s\n"
                  "\tDIRTY %s\n"
                  "\tSHARED %s\n"
                  "\tWT %s\n",
                  idx, ppn, page_sz, valid ? "TRUE" : "FALSE",
                  protection, cacheable ? "TRUE" : "FALSE",
                  dirty ? "TRUE" : "FALSE",
                  shared ? "TRUE" : "FALSE",
                  wt ? "TRUE" : "FALSE");
#endif

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
    unsigned idx = (addr >> 8) & 3;
    struct sh4_itlb_ent *ent = sh4->mem.itlb + idx;

    SH4_MEM_TRACE("ITLB ADDRESS ARRAY WRITE %08X TO %08X\n",
                  (unsigned)val, (unsigned)addr);

    ent->vpn = val & BIT_RANGE(10, 31);
    ent->valid = (val >> 8) & 1;
    ent->asid = val & BIT_RANGE(0, 7);

    SH4_MEM_TRACE("ITLB INDEX %u:\n"
                  "\tVPN %08X\n"
                  "\tVALID %s\n"
                  "\tASID %u\n",
                  idx,
                  (unsigned)ent->vpn,
                  ent->valid ? "TRUE" : "FALSE",
                  (unsigned)ent->asid);
}

static uint32_t sh4_itlb_addr_array_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_itlb_data_array_1_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    unsigned idx = (addr >> 8) & 3;
    struct sh4_itlb_ent *ent = sh4->mem.itlb + idx;

    SH4_MEM_TRACE("ITLB DATA 1 ARRAY WRITE %08X TO %08X\n",
                  (unsigned)val, (unsigned)addr);

    ent->ppn = val & BIT_RANGE(10, 28);
    ent->valid = (val >> 8) & 1;
    ent->protection = (val >> 6) & 1;
    ent->sz = (enum sh4_tlb_page_sz)(((val >> 4) & 1) | ((val >> 6) & 2));
    ent->cacheable = (val >> 3) & 1;
    ent->shared = (val >> 1) & 1;

#if defined ENABLE_LOG_DEBUG && defined(SUPER_VERBOSE_MEM_TRACE)
    char const *page_sz;
    switch (ent->sz) {
    case SH4_TLB_PAGE_1KB:
        page_sz = "1KB";
        break;
    case SH4_TLB_PAGE_4KB:
        page_sz = "4KB";
        break;
    case SH4_TLB_PAGE_64KB:
        page_sz = "64KB";
        break;
    case SH4_TLB_PAGE_1MB:
        page_sz = "1MB";
        break;
    default:
        // should never happen
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    SH4_MEM_TRACE("ITLB INDEX %u:\n"
                  "\tPPN %08X\n"
                  "\tVALID %s\n"
                  "\tPROTECTION %X\n"
                  "\tSIZE %s\n"
                  "\tCACHEABLE %s\n"
                  "\tSHARED %s\n",
                  idx,
                  (unsigned)ent->ppn,
                  ent->valid ? "TRUE" : "FALSE",
                  ent->protection,
                  page_sz,
                  ent->cacheable ? "TRUE" : "FALSE",
                  ent->shared ? "TRUE" : "FALSE");
#endif
}

static uint32_t sh4_itlb_data_array_1_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
sh4_itlb_data_array_2_write(struct Sh4 *sh4, addr32_t addr, uint32_t val) {
    unsigned idx = (addr >> 8) & 3;
    struct sh4_itlb_ent *ent = sh4->mem.itlb + idx;

    SH4_MEM_TRACE("ITLB DATA 2 ARRAY WRITE %08X\n", (unsigned)addr);
    ent->tc = (val >> 3) & 1;
    ent->sa = val & 3;
}

static uint32_t sh4_itlb_data_array_2_read(struct Sh4 *sh4, addr32_t addr) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

#ifdef ENABLE_MMU

static void sh4_utlb_increment_urc(struct Sh4 *sh4) {
    uint32_t mmucr = sh4->reg[SH4_REG_MMUCR];
    uint32_t urc = (mmucr & BIT_RANGE(10, 15)) >> 10;
    uint32_t urb = (mmucr & BIT_RANGE(18, 23)) >> 18;

    if (urb != 0 && (urc < urb)) {
        urc++;
        if (urb && urc >= urb)
            urc = 0;
    } else {
        /*
         * if software wrote a value to MMUCR that causes urc > urb,
         * then urb doesn't take effect until urc overflows.
         */
        urc++;
    }

    sh4->reg[SH4_REG_MMUCR] &= ~BIT_RANGE(10, 15);
    sh4->reg[SH4_REG_MMUCR] |= ((urc << 10) & BIT_RANGE(10, 15));
}

enum sh4_utlb_translate_result
sh4_utlb_translate_address(struct Sh4 *sh4, uint32_t *addrp, bool write) {
    uint32_t addr = *addrp;
    unsigned area = (addr >> 29) & 7;
    if (sh4_mmu_at(sh4) && (sh4_addr_in_sq_area(addr) ||
                            (area != 4 && area != 5 && area != 7))) {
        struct sh4_utlb_ent *ent = sh4_utlb_find_ent_associative(sh4, addr);
        sh4_utlb_increment_urc(sh4);
        if (!ent)
            return SH4_UTLB_MISS;
        addr = sh4_utlb_ent_translate_addr(ent, addr);
        /* printf("%s Translate %08X to %08X\n", __func__, (unsigned)*addrp, (unsigned)addr); */

        if (sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK) {
            // privlege mode
            switch (ent->protection) {
            case 0:
            case 2:
                if (write)
                    return SH4_UTLB_PROT_VIOL;
                break;
            case 1:
            case 3:
                if (write && !ent->dirty)
                    return SH4_UTLB_INITIAL_WRITE;
                break;
            }
        } else {
            // user mode
            switch (ent->protection) {
            case 0:
            case 1:
                return SH4_UTLB_PROT_VIOL;
            case 2:
                if (write)
                    return SH4_UTLB_PROT_VIOL;
                break;
            case 3:
                if (write && !ent->dirty)
                    return SH4_UTLB_INITIAL_WRITE;
                break;
            }
        }

        /*
         * remap area 7 VPNs to P4 PPNs
         * SH4 does this because external memory addresses are 28 bits, which
         * means it's impossible to reference the P4 area in a PPN since that
         * requires setting bits 29-31.
         */
        if ((addr & BIT_RANGE(24, 28)) == BIT_RANGE(24, 28))
            addr |= BIT_RANGE(29, 31);
    }

    *addrp = addr;
    return SH4_UTLB_SUCCESS;
}

static unsigned sh4_mmu_get_lrui(struct Sh4 *sh4) {
    uint32_t mmucr = sh4->reg[SH4_REG_MMUCR];
    return (mmucr & SH4_MMUCR_LRUI_MASK) >> SH4_MMUCR_LRUI_SHIFT;
}

static void sh4_mmu_set_lrui(struct Sh4 *sh4, unsigned lrui) {
    sh4->reg[SH4_REG_MMUCR] &= ~SH4_MMUCR_LRUI_MASK;
    sh4->reg[SH4_REG_MMUCR] |= ((lrui << SH4_MMUCR_LRUI_SHIFT) & SH4_MMUCR_LRUI_MASK);
}

enum sh4_itlb_translate_result
sh4_itlb_translate_address(struct Sh4 *sh4, uint32_t *addr_p) {
    uint32_t addr = *addr_p;
    bool already_searched_utlb = false;

    unsigned area = (addr >> 29) & 7;
 mulligan:
    if (sh4_mmu_at(sh4) && !(area == 4 || area == 5 || area == 7)) {
        struct sh4_itlb_ent *itlb_ent = sh4_itlb_find_ent_associative(sh4, addr);
        /* LOG_ERROR("ITLB ATTEMPTING TO TRANSLATE INSTRUCTION ADDRESS %08X\n", (unsigned)addr); */
        if (!itlb_ent) {
            // ITLB miss ("page fault") exception

            SH4_MEM_TRACE("SEARCHING UTLB TO REPLACE ITLB\n");

            if (already_searched_utlb)
                RAISE_ERROR(ERROR_INTEGRITY);

            unsigned lrui = sh4_mmu_get_lrui(sh4);

            unsigned idx;
            if ((lrui & BIT_RANGE(3, 5)) == BIT_RANGE(3, 5)) {
                idx = 0;
            } else if ((lrui & ((1 << 5) | BIT_RANGE(1, 2))) == 6) {
                idx = 1;
            } else if ((lrui & ((1 << 4) | (1 << 2) | 1)) == 1) {
                idx = 2;
            } else if ((lrui & (3 | (1 << 3))) == 0) {
                idx = 3;
            } else {
                error_set_feature("Unknown LRUI setting");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }

            itlb_ent = sh4->mem.itlb + idx;

            struct sh4_utlb_ent *utlb_ent = sh4_utlb_find_ent_associative(sh4, addr);
            if (!utlb_ent) {
                SH4_MEM_TRACE("ITLB PAGE FAULT SEARCHING FOR %08X\n", (unsigned)addr);
                return SH4_ITLB_MISS; // ITLB miss exception gets raised
            }

            SH4_MEM_TRACE("Copying over UTLB entry %u into ITLB entry %u\n",
                          (unsigned)(utlb_ent - sh4->mem.utlb),
                          (unsigned)(itlb_ent - sh4->mem.itlb));

            itlb_ent->asid = utlb_ent->asid;
            itlb_ent->vpn = utlb_ent->vpn;
            itlb_ent->ppn = utlb_ent->ppn;
            itlb_ent->protection = (utlb_ent->protection >> 1) & 1;
            itlb_ent->sa = utlb_ent->sa;
            itlb_ent->sz = utlb_ent->sz;
            itlb_ent->valid = utlb_ent->valid;
            itlb_ent->shared = utlb_ent->shared;
            itlb_ent->cacheable = utlb_ent->cacheable;
            itlb_ent->tc = utlb_ent->tc;

            already_searched_utlb = true;
            goto mulligan;
        }
        addr = sh4_itlb_ent_translate_addr(itlb_ent, addr);

        if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK) && !itlb_ent->protection) {
            return SH4_ITLB_PROT_VIOL;
        }

        unsigned idx = itlb_ent - sh4->mem.itlb;
        unsigned lrui = sh4_mmu_get_lrui(sh4);
        switch (idx) {
        case 0:
            lrui &= BIT_RANGE(3, 5);
            break;
        case 1:
            lrui &= ~((1 << 5) | BIT_RANGE(1, 2));
            lrui |= 1 << 5;
            break;
        case 2:
            lrui &= ~(1 | (1 << 2) | (1 << 4));
            lrui |= (1 << 2) | (1 << 4);
            break;
        case 3:
            lrui |= BIT_RANGE(0, 1) | (1 << 3);
            break;
        default:
            error_set_feature("Unknown LRUI setting");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        sh4_mmu_set_lrui(sh4, lrui);

        /* printf("itlb translate %08X to %08X\n", (unsigned)*addr_p, (unsigned)addr); */
        *addr_p = addr;
    }

    return SH4_ITLB_SUCCESS;
}

uint32_t sh4_itlb_ent_translate_addr(struct sh4_itlb_ent const *ent, uint32_t vpn) {
    return (vpn & page_offset_mask_for_size(ent->sz)) |
        (ent->ppn & ppn_mask_for_size(ent->sz));
}

uint32_t sh4_utlb_ent_translate_addr(struct sh4_utlb_ent const *ent, uint32_t vpn) {
    return (vpn & page_offset_mask_for_size(ent->sz)) | (ent->ppn & ppn_mask_for_size(ent->sz));
}

#endif

void sh4_mmu_invalidate_tlb(struct Sh4 *sh4) {
    LOG_DBG("%s is invalidating the entire SH4 TLB\n", __func__);

    unsigned idx;
    for (idx = 0; idx < SH4_UTLB_LEN; idx++)
        sh4->mem.utlb[idx].valid = false;
    for (idx = 0; idx < SH4_ITLB_LEN; idx++)
        sh4->mem.itlb[idx].valid = false;
}

void sh4_mmu_do_ldtlb(struct Sh4 *sh4) {
    unsigned idx = (sh4->reg[SH4_REG_MMUCR] & BIT_RANGE(10, 15)) >> 10;
    struct sh4_utlb_ent *ent = sh4->mem.utlb + idx;
    uint32_t pteh = sh4->reg[SH4_REG_PTEH];
    uint32_t ptel = sh4->reg[SH4_REG_PTEL];
    uint32_t ptea = sh4->reg[SH4_REG_PTEA];

    ent->asid = pteh & BIT_RANGE(0, 7);
    ent->vpn = pteh & BIT_RANGE(10,31);
    ent->ppn = ptel & BIT_RANGE(10, 28);
    ent->sz = (enum sh4_tlb_page_sz)
        (((ptel & (1<<7)) >> 6) | ((ptel >> 4) & 1));
    ent->shared = (ptel >> 1) & 1;
    ent->protection = (ptel >> 5) & 3;
    ent->wt = ptel & 1;
    ent->cacheable = (ptel >> 3) & 1;
    ent->dirty = (ptel >> 2) & 1;
    ent->valid = (ptel >> 8) & 1;
    ent->sa = ptea & 7;
    ent->tc = (ptea >> 3) & 1;

#if defined ENABLE_LOG_DEBUG && defined(SUPER_VERBOSE_MEM_TRACE)
    char const *page_sz;
    switch (ent->sz) {
    case SH4_TLB_PAGE_1KB:
        page_sz = "1KB";
        break;
    case SH4_TLB_PAGE_4KB:
        page_sz = "4KB";
        break;
    case SH4_TLB_PAGE_64KB:
        page_sz = "64KB";
        break;
    case SH4_TLB_PAGE_1MB:
        page_sz = "1MB";
        break;
    default:
        // should never happen
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    SH4_MEM_TRACE("LDTLB INTO UTLB INDEX %u:\n"
                  "\tVPN %08X\n"
                  "\tPPN %08X\n"
                  "\tASID %08X\n"
                  "\tPAGE SIZE %s\n"
                  "\tVALID %s\n"
                  "\tPROTECTION %02X\n"
                  "\tCACHEABLE %s\n"
                  "\tDIRTY %s\n"
                  "\tSHARED %s\n"
                  "\tWT %s\n"
                  "\tSA %08X\n"
                  "\tTC %s\n",
                  idx, ent->vpn, ent->ppn, ent->asid, page_sz, ent->valid ? "TRUE" : "FALSE",
                  ent->protection, ent->cacheable ? "TRUE" : "FALSE",
                  ent->dirty ? "TRUE" : "FALSE",
                  ent->shared ? "TRUE" : "FALSE",
                  ent->wt ? "TRUE" : "FALSE",
                  ent->sa, ent->tc ? "TRUE" : "FALSE");
#endif
}
