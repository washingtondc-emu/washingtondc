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

void sh4_ocache_do_write_ora(Sh4 *sh4, void const *dat,
                             addr32_t paddr, unsigned len) {
    void *addr = sh4_ocache_get_ora_ram_addr(sh4, paddr);
    memcpy(addr, dat, len);
}

void sh4_ocache_do_read_ora(Sh4 *sh4, void *dat, addr32_t paddr, unsigned len) {
    void *addr = sh4_ocache_get_ora_ram_addr(sh4, paddr);
    memcpy(dat, addr, len);
}

void *sh4_ocache_get_ora_ram_addr(Sh4 *sh4, addr32_t paddr) {
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

int sh4_sq_write(Sh4 *sh4, void const *buf,
                 addr32_t addr, unsigned len) {
    /*
     * TODO: implement MMU functionality
     *
     * Also get the timing right, I'm not confident store-queues are supposed
     * to be as instantaneous as I'm making them...
     */
#ifdef ENABLE_SH4_MMU
    if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_AT_MASK) {
        error_set_feature("MMU support for store queues");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
#endif

    if ((sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SQMD_MASK) &&
        !(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        LOG_DBG("%s: Address error raised\n", __func__);
        sh4_set_exception(sh4, SH4_EXCP_INST_ADDR_ERR);
        return 1;
    }

    unsigned n_words = len >> 2;
    unsigned sq_idx = (addr >> 2) & 0x7;
    unsigned sq_sel = ((addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT) << 3;
    if ((n_words + sq_idx > 8) || (len & 3)) {
        // the spec doesn't say what kind of error to raise here
        error_set_length(len);
        error_set_feature("whatever happens when you provide an inappropriate "
                          "length during a store-queue write");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    assert(len + sq_idx * sizeof(sh4->ocache.sq[0]) +
           sq_sel * sizeof(sh4->ocache.sq[0]) <= sizeof(sh4->ocache.sq));

    memcpy(sh4->ocache.sq + sq_idx + sq_sel, buf, len);

    return 0;
}

int sh4_sq_read(Sh4 *sh4, void *buf, addr32_t addr, unsigned len) {
    /*
     * TODO: implement MMU functionality
     *
     * Also get the timing right, I'm not confident store-queues are supposed
     * to be as instantaneous as I'm making them...
     */
#ifdef ENABLE_SH4_MMU
    if (sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_AT_MASK) {
        error_set_feature("MMU support for store queues");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
#endif

    if ((sh4->reg[SH4_REG_MMUCR] & SH4_MMUCR_SQMD_MASK) &&
        !(sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK)) {
        LOG_DBG("%s: Address error raised\n", __func__);
        sh4_set_exception(sh4, SH4_EXCP_INST_ADDR_ERR);
        return 1;
    }

    unsigned n_words = len >> 2;
    unsigned sq_idx = (addr >> 2) & 0x7;
    unsigned sq_sel = ((addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT) << 3;
    if ((n_words + sq_idx > 8) || (len & 3)) {
        // the spec doesn't say what kind of error to raise here
        error_set_length(len);
        error_set_feature("whatever happens when you provide an inappropriate "
                          "length during a store-queue write");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    assert(len + sq_idx * sizeof(sh4->ocache.sq[0]) +
           sq_sel * sizeof(sh4->ocache.sq[0]) <= sizeof(sh4->ocache.sq));

    memcpy(buf, sh4->ocache.sq + sq_idx + sq_sel, len);

    return 0;
}

int sh4_sq_pref(Sh4 *sh4, addr32_t addr) {
    unsigned sq_sel = (addr & SH4_SQ_SELECT_MASK) >> SH4_SQ_SELECT_SHIFT;
    unsigned sq_idx = sq_sel << 3;
    reg32_t qacr = sh4->reg[SH4_REG_QACR0 + sq_sel];
    addr32_t addr_actual = (addr & SH4_SQ_ADDR_MASK) |
        (((qacr & SH4_QACR_MASK) >> SH4_QACR_SHIFT) << 26);

    int ret = sh4_do_write_mem(sh4, sh4->ocache.sq + sq_idx,
                               addr_actual, 8 * sizeof(uint32_t));
    if (ret == MEM_ACCESS_FAILURE)
        RAISE_ERROR(get_error_pending());
    return ret;
}

void sh4_ocache_write_addr_array(Sh4 *sh4, void const *dat,
                                 addr32_t paddr, unsigned len) {
    // do nothing
}

void sh4_ocache_read_addr_array(Sh4 *sh4, void *dat,
                                addr32_t paddr, unsigned len) {
    /*
     * I'm really not sure what to do here, so return all 0.
     * namco museum uses this, but I'm not sure why.
    */
    memset(dat, 0, len);
}
