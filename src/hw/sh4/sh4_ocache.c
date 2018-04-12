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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "sh4.h"
#include "sh4_excp.h"
#include "error.h"
#include "log.h"

#include "sh4_ocache.h"

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

#define SH4_OCACHE_DO_WRITE_ORA_TMPL(type, postfix)                     \
    void sh4_ocache_do_write_ora_##postfix(Sh4 *sh4,                    \
                                           addr32_t paddr, type val) {  \
        type *addr = (type*)sh4_ocache_get_ora_ram_addr(sh4, paddr);    \
        *addr = val;                                                    \
    }

SH4_OCACHE_DO_WRITE_ORA_TMPL(double, double)
SH4_OCACHE_DO_WRITE_ORA_TMPL(float, float)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint32_t, 32)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint16_t, 16)
SH4_OCACHE_DO_WRITE_ORA_TMPL(uint8_t, 8)

#define SH4_OCACHE_DO_READ_ORA_TMPL(type, postfix)                      \
    type sh4_ocache_do_read_ora_##postfix(Sh4 *sh4, addr32_t paddr) {   \
        type *ptr = (type*)sh4_ocache_get_ora_ram_addr(sh4, paddr);     \
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

int sh4_sq_pref(Sh4 *sh4, addr32_t addr) {
    unsigned sq_sel = (addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT;
    unsigned sq_idx = sq_sel << 3;
    reg32_t qacr = sh4->reg[SH4_REG_QACR0 + sq_sel];
    addr32_t addr_actual = (addr & SH4_SQ_ADDR_MASK) |
        (((qacr & SH4_QACR_MASK) >> SH4_QACR_SHIFT) << 26);

    int idx;
    for (idx = 0; idx < 8; idx++) {
        memory_map_write_32((sh4->ocache.sq + sq_idx)[idx],
                         addr_actual + idx * sizeof(uint32_t));
    }
    return MEM_ACCESS_SUCCESS;
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
