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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <intmath.h>

#include "sh4.h"
#include "sh4_excp.h"
#include "washdc/error.h"
#include "log.h"
#include "washdc/MemoryMap.h"
#include "sh4_mem.h"

#include "sh4_ocache.h"

/*
 * read to/write from the operand cache's RAM-space in situations where we
 * don't actually have a real operand cache available.  It is up to the
 * caller to make sure that the operand cache is enabled (OCE in the CCR),
 * that the Operand Cache's RAM switch is enabled (ORA in the CCR) and that
 * paddr lies within the Operand Cache RAM mapping (in_oc_ram_area returns
 * true).
 */
double sh4_ocache_do_read_ora_double(addr32_t addr, void *ctxt);
float sh4_ocache_do_read_ora_float(addr32_t addr, void *ctxt);
uint32_t sh4_ocache_do_read_ora_32(addr32_t addr, void *ctxt);
uint16_t sh4_ocache_do_read_ora_16(addr32_t addr, void *ctxt);
uint8_t sh4_ocache_do_read_ora_8(addr32_t addr, void *ctxt);

void sh4_ocache_do_write_ora_double(addr32_t addr, double val, void *ctxt);
void sh4_ocache_do_write_ora_float(addr32_t addr, float val, void *ctxt);
void sh4_ocache_do_write_ora_32(addr32_t addr, uint32_t val, void *ctxt);
void sh4_ocache_do_write_ora_16(addr32_t addr, uint16_t val, void *ctxt);
void sh4_ocache_do_write_ora_8(addr32_t addr, uint8_t val, void *ctxt);

struct memory_interface sh4_ora_intf = {
    .readdouble = sh4_ocache_do_read_ora_double,
    .readfloat = sh4_ocache_do_read_ora_float,
    .read32 = sh4_ocache_do_read_ora_32,
    .read16 = sh4_ocache_do_read_ora_16,
    .read8 = sh4_ocache_do_read_ora_8,

    .writedouble = sh4_ocache_do_write_ora_double,
    .writefloat = sh4_ocache_do_write_ora_float,
    .write32 = sh4_ocache_do_write_ora_32,
    .write16 = sh4_ocache_do_write_ora_16,
    .write8 = sh4_ocache_do_write_ora_8
};

#define SH4_OCACHE_LONGS_PER_CACHE_LINE  8
#define SH4_OCACHE_ENTRY_COUNT           512
#define SH4_OCACHE_LINE_SHIFT            5
#define SH4_OCACHE_LINE_SIZE             (SH4_OCACHE_LONGS_PER_CACHE_LINE * 4)
#define SH4_OCACHE_SIZE        (SH4_OCACHE_ENTRY_COUNT * SH4_OCACHE_LINE_SIZE)

// The valid flag
#define SH4_OCACHE_KEY_VALID_SHIFT 0
#define SH4_OCACHE_KEY_VALID_MASK  (1 << SH4_OCACHE_KEY_VALID_SHIFT)

// the dirty flag
#define SH4_OCACHE_KEY_DIRTY_SHIFT 1
#define SH4_OCACHE_KEY_DIRTY_MASK  (1 << SH4_OCACHE_KEY_DIRTY_SHIFT)

// the tag represents bits 28:10 (inclusive) of a 29-bit address.
#define SH4_OCACHE_KEY_TAG_SHIFT 2
#define SH4_OCACHE_KEY_TAG_MASK  (0x7ffff << SH4_OCACHE_KEY_TAG_SHIFT)

static void *sh4_ocache_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr);

void sh4_ocache_init(struct sh4_ocache *ocache) {
    ocache->oc_ram_area = (uint8_t*)malloc(sizeof(uint8_t) * SH4_OC_RAM_AREA_SIZE);

    sh4_ocache_clear(ocache);
}

void sh4_ocache_cleanup(struct sh4_ocache *ocache) {
    free(ocache->oc_ram_area);
}

void sh4_ocache_clear(struct sh4_ocache *ocache) {
    memset(ocache->oc_ram_area, 0, sizeof(uint8_t) * SH4_OC_RAM_AREA_SIZE);
}

#ifdef INVARIANTS
#define INVARIANTS_ORA_WRITE_RANGE_CHECK                          \
    if (!sh4_ocache_in_ram_area(paddr)) {                         \
            error_set_address(paddr);                             \
            error_set_value(val);                                 \
            RAISE_ERROR(ERROR_INTEGRITY);                         \
    }
#define INVARIANTS_ORA_READ_RANGE_CHECK                           \
    if (!sh4_ocache_in_ram_area(paddr)) {                         \
            error_set_address(paddr);                             \
            RAISE_ERROR(ERROR_INTEGRITY);                         \
    }
#else
#define INVARIANTS_ORA_WRITE_RANGE_CHECK
#define INVARIANTS_ORA_READ_RANGE_CHECK
#endif

/*
 * XXX based on some preliminary hardware tests I have done, you can actually
 * write to the ORA area with ORA disabled, and it will hold the value.  I do
 * not understand how that works since the specification says that will not
 * work.
 *
 * I have verified that writes to the ORA area when the operand cache is
 * disabled will not maintain their value, and read will always return 0.
 */

// TODO index mode can re-order ORA banks, so maybe we need to consider that?

#define SH4_OCACHE_DO_WRITE_ORA_TMPL(type, postfix)                     \
    void sh4_ocache_do_write_ora_##postfix(uint32_t paddr, type val,    \
                                           void *ctxt) {                \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
        INVARIANTS_ORA_WRITE_RANGE_CHECK                                \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK))                \
            return;                                                     \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK))                \
            LOG_WARN("WARNING: WRITING %08X to %08X (%u bytes) WITH ORA " \
                     "DISABLED\n",                                      \
                     (unsigned)val, (unsigned)paddr, (unsigned)sizeof(val)); \
                                                                        \
        type *addr = (type*)sh4_ocache_get_ora_ram_addr(sh4, paddr);    \
        *addr = val;                                                    \
    }

SH4_OCACHE_DO_WRITE_ORA_TMPL(double, double)
SH4_OCACHE_DO_WRITE_ORA_TMPL(float, float)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint32_t, 32)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint16_t, 16)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint8_t, 8)

#define SH4_OCACHE_DO_READ_ORA_TMPL(type, postfix)                      \
    type sh4_ocache_do_read_ora_##postfix(uint32_t paddr, void *ctxt) { \
        struct Sh4 *sh4 = (struct Sh4*)ctxt;                            \
        INVARIANTS_ORA_READ_RANGE_CHECK                                 \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK))                \
            return 0;                                                   \
        type *ptr = (type*)sh4_ocache_get_ora_ram_addr(sh4, paddr);     \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK))                \
            LOG_WARN("WARNING: READING %08X to %08X (%u bytes) WITH ORA " \
                     "DISABLED\n",                                      \
                     (unsigned)*ptr, (unsigned)paddr, (unsigned)sizeof(type)); \
        return *ptr;                                                    \
    }

SH4_OCACHE_DO_READ_ORA_TMPL(double, double)
SH4_OCACHE_DO_READ_ORA_TMPL(float, float)
SH4_OCACHE_DO_READ_ORA_TMPL(uint32_t, 32)
SH4_OCACHE_DO_READ_ORA_TMPL(uint16_t, 16)
SH4_OCACHE_DO_READ_ORA_TMPL(uint8_t, 8)

static void *sh4_ocache_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr) {
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
    return sh4->ocache.oc_ram_area + area_start + area_offset;
}

#ifdef INVARIANTS
static inline void sq_invariants_check(size_t len, unsigned sq_idx) {
    if (len / 4 + sq_idx > 8) {
        /* the spec doesn't say what kind of error to raise here */
        error_set_length(len);
        error_set_feature("whatever happens when you provide an inappropriate "
                          "length during a store-queue operation");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}
#else
static inline void sq_invariants_check(size_t len, unsigned sq_idx) {
}
#endif

/* TODO: implement MMU functionality and also privileged mode */
#define SH4_SQ_WRITE_TMPL(type, postfix)        \
    void sh4_sq_write_##postfix(Sh4 *sh4, addr32_t addr, type val) {    \
                                                                        \
        unsigned sq_idx = (addr >> 2) & 0x7;                            \
        unsigned sq_sel = ((addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT) \
            << 3;                                                       \
        sq_invariants_check(sizeof(type), sq_idx);                      \
                                                                        \
        *(type*)(sh4->ocache.sq + sq_idx + sq_sel) = val;               \
    }

SH4_SQ_WRITE_TMPL(double, double)
SH4_SQ_WRITE_TMPL(float, float)
SH4_SQ_WRITE_TMPL(uint32_t, 32)
SH4_SQ_WRITE_TMPL(uint16_t, 16)
SH4_SQ_WRITE_TMPL(uint8_t, 8)

/* TODO: implement MMU functionality and also privileged mode */
#define SH4_SQ_READ_TMPL(type, postfix)                                 \
    type sh4_sq_read_##postfix(Sh4 *sh4, addr32_t addr) {               \
                                                                        \
    unsigned sq_idx = (addr >> 2) & 0x7;                                \
    unsigned sq_sel = ((addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT) \
        << 3;                                                           \
    sq_invariants_check(sizeof(type), sq_idx);                          \
                                                                        \
    return *(type*)(sh4->ocache.sq + sq_idx + sq_sel);                  \
}

SH4_SQ_READ_TMPL(double, double)
SH4_SQ_READ_TMPL(float, float)
SH4_SQ_READ_TMPL(uint32_t, 32)
SH4_SQ_READ_TMPL(uint16_t, 16)
SH4_SQ_READ_TMPL(uint8_t, 8)

static DEF_ERROR_INT_ATTR(sq_mmu_excp_tp)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_0)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_1)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_2)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_3)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_4)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_5)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_6)
static DEF_ERROR_U32_ATTR(sq_busrt_write_dword_7)

static DEF_ERROR_U32_ATTR(sq_addr_first)
static DEF_ERROR_U32_ATTR(sq_addr_last)

int sh4_sq_pref(Sh4 *sh4, addr32_t addr) {
    addr32_t addr_actual;
    unsigned sq_sel = (addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT;
    unsigned sq_idx = sq_sel << 3;

#ifdef ENABLE_MMU
    if (!(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK))
        if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SQMD_MASK) {
            error_set_feature("store queue address error exception");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
    if (sh4_mmu_at(sh4)) {
        addr32_t vpn = addr;
        enum sh4_utlb_translate_result res;
        switch ((res = sh4_utlb_translate_address(sh4, &vpn, true))) {
        case SH4_UTLB_MISS:
            // need to set exception registers here based on the vpn we just decoded
            SH4_MEM_TRACE("DATA TLB WRITE MISS EXCEPTION (store queue) DECODING "
                          "%08X\n", (unsigned)addr);
            sh4->reg[SH4_REG_TEA] = addr;
            sh4->reg[SH4_REG_PTEH] &= ~BIT_RANGE(10, 31);
            sh4->reg[SH4_REG_PTEH] |= (addr & BIT_RANGE(10, 31));

            /*
             * TODO: This seems like it obviously should be a write-miss, but
             * ambiguous wording in the SH4 spec makes it seem like a read
             * miss may be the correct exception...?
             */
            /* sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_READ_MISS); */
            sh4_set_exception(sh4, SH4_EXCP_DATA_TLB_WRITE_MISS);
            return MEM_ACCESS_FAILURE;
        case SH4_UTLB_SUCCESS:
            break;
        default:
            error_set_address(addr);
            error_set_sq_mmu_excp_tp((int)res);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        addr_actual = (vpn & BIT_RANGE(10, 28)) | (addr & BIT_RANGE(5, 9));
    } else {
#endif
        reg32_t qacr = sh4->reg[SH4_REG_QACR0 + sq_sel];
        addr_actual = (addr & SH4_SQ_ADDR_MASK) |
            (((qacr & SH4_QACR_MASK) >> SH4_QACR_SHIFT) << 26);
#ifdef ENABLE_MMU
    }
#endif

    struct memory_map_region *region =
        memory_map_get_region(sh4->mem.map, addr_actual, 8 * sizeof(uint32_t));

    if (region) {
        struct memory_interface const *intf = region->intf;
        uint32_t mask = region->mask;
        void *ctxt = region->ctxt;
        memory_map_write32_func write32 = intf->write32;
        uint32_t *sq = sh4->ocache.sq + sq_idx;

        CHECK_W_WATCHPOINT(addr_actual + 0, uint32_t);
        write32((addr_actual + 0) & mask, sq[0], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 4, uint32_t);
        write32((addr_actual + 4) & mask, sq[1], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 8, uint32_t);
        write32((addr_actual + 8) & mask, sq[2], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 12, uint32_t);
        write32((addr_actual + 12) & mask, sq[3], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 16, uint32_t);
        write32((addr_actual + 16) & mask, sq[4], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 20, uint32_t);
        write32((addr_actual + 20) & mask, sq[5], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 24, uint32_t);
        write32((addr_actual + 24) & mask, sq[6], ctxt);
        CHECK_W_WATCHPOINT(addr_actual + 28, uint32_t);
        write32((addr_actual + 28) & mask, sq[7], ctxt);

        return MEM_ACCESS_SUCCESS;
    } else {
        uint32_t first_addr = addr_actual;
        uint32_t last_addr = addr_actual + (8 * sizeof(uint32_t) - 1);

        LOG_ERROR("MEMORY MAP FAILURE TO FIND REGION CORRESPONDING TO BYTE "
                  "RANGE 0x%08x TO 0x%08x\n", (int)first_addr, (int)last_addr);

        uint32_t *sq = sh4->ocache.sq + sq_idx;
        error_set_sq_busrt_write_dword_0(sq[0]);
        error_set_sq_busrt_write_dword_1(sq[1]);
        error_set_sq_busrt_write_dword_2(sq[2]);
        error_set_sq_busrt_write_dword_3(sq[3]);
        error_set_sq_busrt_write_dword_4(sq[4]);
        error_set_sq_busrt_write_dword_5(sq[5]);
        error_set_sq_busrt_write_dword_6(sq[6]);
        error_set_sq_busrt_write_dword_7(sq[7]);

        error_set_address(addr_actual);
        error_set_sq_addr_first(first_addr);
        error_set_sq_addr_last(last_addr);
        error_set_length(8 * sizeof(uint32_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
}

/*
 * TODO: I'm really not sure what to do here, so return all 0.
 * namco museum uses this, but I'm not sure why.
 */
#define SH4_OCACHE_READ_ADDR_ARRAY_TMPL(type, postfix)                  \
    type sh4_ocache_read_addr_array_##postfix(Sh4 *sh4, addr32_t paddr) { \
        return (type)0;                                                 \
    }

SH4_OCACHE_READ_ADDR_ARRAY_TMPL(float, float)
SH4_OCACHE_READ_ADDR_ARRAY_TMPL(double, double)
SH4_OCACHE_READ_ADDR_ARRAY_TMPL(uint32_t, 32)
SH4_OCACHE_READ_ADDR_ARRAY_TMPL(uint16_t, 16)
SH4_OCACHE_READ_ADDR_ARRAY_TMPL(uint8_t, 8)

#define SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(type, postfix)                 \
    void sh4_ocache_write_addr_array_##postfix(Sh4 *sh4,                \
                                               addr32_t paddr,          \
                                               type val) {              \
        /* do nothing */                                                \
    }

SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(float, float)
SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(double, double)
SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(uint32_t, 32)
SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(uint16_t, 16)
SH4_OCACHE_WRITE_ADDR_ARRAY_TMPL(uint8_t, 8)
