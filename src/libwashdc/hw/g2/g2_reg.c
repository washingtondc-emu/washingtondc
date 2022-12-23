/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019, 2022 snickerbockers
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

#include <string.h>
#include <stdio.h>

#include <stdint.h>

#include "washdc/types.h"
#include "mem_code.h"
#include "mem_areas.h"
#include "washdc/error.h"
#include "log.h"
#include "mmio.h"
#include "hw/sys/holly_intc.h"
#include "dc_sched.h"
#include "dreamcast.h"
#include "intmath.h"

#include "g2_reg.h"

#define G2_ADDR_MASK ADDR_AREA0_MASK

/*
 * The below table details the amount of time it takes for DMA transfers of
 * various sizes to complete.  The time given is the amount of time until the
 * interrupt occurred.  These measurements were taken on a real Dreamcast, while
 * performing DMA transfers from main sh4 system memory to AICA memory.
 *
 * I did 3 trials for each transfer size.  The reason for this is that in the
 * first two trials i made minor mistakes in the way i configured the dma
 * transaction.  I was afraid they would impact the measurements, but that
 * doesn't seem to be the case.  The third timing for each transfer size is the
 * most "correct" one, but really they're all within margin of error of each
 * other so i consider them all to be equally valid.
 *
 * 32b   | 8 us
 *       | 8 us
 *       | 9 us
 * 64b   | 14 us
 *       | 14 us
 *       | 13 us
 * 128b  | 24 us
 *       | 24 us
 *       | 25 us
 * 256b  | 46 us
 *       | 48 us
 *       | 47 us
 * 512b  | 92 us
 *       | 90 us
 *       | 91 us
 * 1kb   | 182 us
 *       | 181 us
 *       | 182 us
 * 2kb   | 359 us
 *       | 357 us
 *       | 360 us
 * 4kb   | 713 us
 *       | 715 us
 *       | 715 us
 * 8kb   | 1423 us
 *       | 1424 us
 *       | 1424 us
 * 16kb  | 2843 us
 *       | 2846 us
 *       | 2843 us
 * 32kb  | 5680 us
 *       | 5690 us
 *       | 5686 us
 * 64kb  | 11358 us
 *       | 11382 us
 *       | 11371 us
 * 128kb | 22721 us
 *       | 22746 us
 *       | 22740 us
 * 256kb | 45441 us
 *       | 45516 us
 *       | 45442 us
 * 512kb | 90880 us
 *       | 90987 us
 *       | 90877 us
 * 1mb   | 181732 us
 *       | 182010 us
 *       | 181770 us
 * 2mb   | 363418 us
 *       | 364040 us
 *       | 363534 us
 *
 * *** NOTE: the below case was done by accident because i had
 *           words confused with bytes (so i thought they were 512kb and 1mb,
 *           respectively).  I think the result is still valid because the
 *           overflow would have gone into a mirror of AICA's memory.
 *
 * 4mb   | 727969 us
 *       | 728047 us
 *       | 726985 us
 */
static dc_cycle_stamp_t aica_dma_complete_int_delay(size_t n_bytes) {
    double linear = n_bytes * 0.17347222;
    double constant = 7.64459071;
    if (constant >= linear)
        return 0;
    double us = linear - constant;
    dc_cycle_stamp_t ret =  (dc_cycle_stamp_t)(us * (double)SCHED_FREQUENCY / 1000000.0);
    return ret;
}
#define AICA_DMA_COMPLETE_INT_DELAY aica_dma_complete_int_delay

#define N_G2_REGS (ADDR_G2_LAST - ADDR_G2_FIRST + 1)

DECL_MMIO_REGION(g2_reg_32, N_G2_REGS, ADDR_G2_FIRST, uint32_t)
DEF_MMIO_REGION(g2_reg_32, N_G2_REGS, ADDR_G2_FIRST, uint32_t)
static struct mmio_region_g2_reg_32 mmio_region_g2_reg_32;

static uint8_t reg_backing[N_G2_REGS];

struct g2_dma_ch {
    uint32_t tsel, dir, star, stag, len, st, en, susp;

    void(*do_xfer)(uint32_t src_addr, uint32_t dst_addr, unsigned n_bytes);

    char const *name;
};

static void
g2_dma_ad_xfer(uint32_t src_addr, uint32_t dst_addr, unsigned n_bytes);
static void
g2_dma_unimplemented_xfer(uint32_t src_addr, uint32_t dst_addr, unsigned n_bytes);

static struct g2_dma_ch dma_ch_ad = {
    .name = "ad",
    .do_xfer = g2_dma_ad_xfer
};

static struct g2_dma_ch dma_ch_e1 = {
    .name = "e1",
    .do_xfer = g2_dma_unimplemented_xfer
};

static struct g2_dma_ch dma_ch_e2 = {
    .name = "e2",
    .do_xfer = g2_dma_unimplemented_xfer
};

static struct g2_dma_ch dma_ch_dd = {
    .name = "dd",
    .do_xfer = g2_dma_unimplemented_xfer
};

static uint32_t g2_dma_read_st(struct g2_dma_ch *ch) {
    uint32_t val = ch->st;
    LOG_DBG("G2: Read 0x%08x from %sst\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_st(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %sst\n", (unsigned)val, ch->name);
    if (val) {
        LOG_DBG("G2: %sdir is %d\n", ch->name, (int)ch->dir);
        LOG_DBG("G2: %stsel is %d\n", ch->name, (int)ch->tsel);
        if (ch->dir)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);

        uint32_t src_addr =
            ch->star & ~(BIT_RANGE(0, 4) | BIT_RANGE(29, 31));
        uint32_t dst_addr =
            ch->stag & ~(BIT_RANGE(0, 4) | BIT_RANGE(29, 31));
        unsigned n_bytes = ch->len & BIT_RANGE(5, 24);

        ch->do_xfer(src_addr, dst_addr, n_bytes);
    }
    ch->st = val;
}

static uint32_t g2_dma_read_tsel(struct g2_dma_ch *ch) {
    uint32_t val = ch->tsel;
    LOG_DBG("G2: Read 0x%08x from %stsel\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_tsel(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %stsel\n", (unsigned)val, ch->name);
    ch->tsel = val;
}

static uint32_t g2_dma_read_en(struct g2_dma_ch *ch) {
    uint32_t val = ch->en;
    LOG_DBG("G2: Read 0x%08x from %sen\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_en(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %sen\n", (unsigned)val, ch->name);
    ch->en = val;
}

static uint32_t g2_dma_read_susp(struct g2_dma_ch *ch) {
    uint32_t val = ch->susp;
    LOG_DBG("G2: Read 0x%08x from %ssusp\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_susp(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %ssusp\n", (unsigned)val, ch->name);
    ch->susp = val;
}

static uint32_t g2_dma_read_dir(struct g2_dma_ch *ch) {
    uint32_t val = ch->dir;
    LOG_DBG("G2: Read 0x%08x from %sdir\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_dir(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %sdir\n", (unsigned)val, ch->name);
    ch->dir = val;
}

static uint32_t g2_dma_read_star(struct g2_dma_ch *ch) {
    uint32_t val = ch->star;
    LOG_DBG("G2: Read 0x%08x from %sstar\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_star(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %sstar\n", (unsigned)val, ch->name);
    ch->star = val;
}

static uint32_t g2_dma_read_stag(struct g2_dma_ch *ch) {
    uint32_t val = ch->stag;
    LOG_DBG("G2: Read 0x%08x from %sstag\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_stag(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %sstag\n", (unsigned)val, ch->name);
    ch->stag = val;
}

static uint32_t g2_dma_read_len(struct g2_dma_ch *ch) {
    uint32_t val = ch->len;
    LOG_DBG("G2: Read 0x%08x from %slen\n", (unsigned)val, ch->name);
    return val;
}

static void g2_dma_write_len(struct g2_dma_ch *ch, uint32_t val) {
    LOG_DBG("G2: Write 0x%08x to %slen\n", (unsigned)val, ch->name);
    ch->len = val;
}

uint8_t g2_reg_read_8(addr32_t addr, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g2_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t g2_reg_read_16(addr32_t addr, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g2_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t g2_reg_read_32(addr32_t addr, void *ctxt) {
    addr &= G2_ADDR_MASK;
    return mmio_region_g2_reg_32_read(&mmio_region_g2_reg_32, addr);
}

void g2_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    addr &= G2_ADDR_MASK;
    mmio_region_g2_reg_32_write(&mmio_region_g2_reg_32, addr, val);
}

float g2_reg_read_float(addr32_t addr, void *ctxt) {
    addr &= G2_ADDR_MASK;
    uint32_t tmp = mmio_region_g2_reg_32_read(&mmio_region_g2_reg_32, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void g2_reg_write_float(addr32_t addr, float val, void *ctxt) {
    addr &= G2_ADDR_MASK;
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_g2_reg_32_write(&mmio_region_g2_reg_32, addr, tmp);
}

double g2_reg_read_double(addr32_t addr, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g2_reg_write_double(addr32_t addr, double val, void *ctxt) {
    addr &= G2_ADDR_MASK;
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static struct SchedEvent aica_dma_raise_event;
static bool sched_aica_dma_event;

static void post_delay_aica_dma_int(struct SchedEvent *event) {
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE); // ?
    sched_aica_dma_event = false;
    dma_ch_ad.st = 0;
}

static void
g2_dma_unimplemented_xfer(uint32_t src_addr, uint32_t dst_addr, unsigned n_bytes) {
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
g2_dma_ad_xfer(uint32_t src_addr, uint32_t dst_addr, unsigned n_bytes) {
    unsigned n_words = n_bytes / 4;
    LOG_DBG("AICA: Request to transfer 0x%08x bytes from 0x%08x to 0x%08x\n",
            n_bytes, (unsigned)src_addr, (unsigned)dst_addr);

    sh4_dmac_transfer_words(dreamcast_get_cpu(), src_addr, dst_addr, n_words);

    aica_dma_raise_event.handler = post_delay_aica_dma_int;
    aica_dma_raise_event.when =
        clock_cycle_stamp(&sh4_clock) + AICA_DMA_COMPLETE_INT_DELAY(n_bytes);
    sched_event(&sh4_clock, &aica_dma_raise_event);
}

static void adst_reg_write(struct mmio_region_g2_reg_32 *region,
                                   unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_st(&dma_ch_ad, val);
}

static uint32_t adst_reg_read(struct mmio_region_g2_reg_32 *region,
                                      unsigned idx, void *ctxt) {
    return g2_dma_read_st(&dma_ch_ad);
}

static uint32_t adtsel_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_tsel(&dma_ch_ad);
}

static void adtsel_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_tsel(&dma_ch_ad, val);
}

static uint32_t aden_reg_read(struct mmio_region_g2_reg_32 *region,
                              unsigned idx, void *ctxt) {
    return g2_dma_read_en(&dma_ch_ad);
}

static void aden_reg_write(struct mmio_region_g2_reg_32 *region,
                           unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_en(&dma_ch_ad, val);
}

static uint32_t adsusp_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_susp(&dma_ch_ad);
}

static void adsusp_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_susp(&dma_ch_ad, val);
}

static uint32_t addir_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_dir(&dma_ch_ad);
}

static void addir_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_dir(&dma_ch_ad, val);
}

static uint32_t adstar_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_star(&dma_ch_ad);
}

static void adstar_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_star(&dma_ch_ad, val);
}

static uint32_t adstag_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_stag(&dma_ch_ad);
}

static void adstag_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_stag(&dma_ch_ad, val);
}

static uint32_t adlen_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_len(&dma_ch_ad);
}

static void adlen_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_len(&dma_ch_ad, val);
}

static uint32_t e1st_reg_read(struct mmio_region_g2_reg_32 *region,
                                      unsigned idx, void *ctxt) {
    return g2_dma_read_st(&dma_ch_e1);
}

static void e1st_reg_write(struct mmio_region_g2_reg_32 *region,
                                   unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_st(&dma_ch_e1, val);
}

static uint32_t e1tsel_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_tsel(&dma_ch_e1);
}

static void e1tsel_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_tsel(&dma_ch_e1, val);
}

static uint32_t e1en_reg_read(struct mmio_region_g2_reg_32 *region,
                              unsigned idx, void *ctxt) {
    return g2_dma_read_en(&dma_ch_e1);
}

static void e1en_reg_write(struct mmio_region_g2_reg_32 *region,
                           unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_en(&dma_ch_e1, val);
}

static uint32_t e1susp_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_susp(&dma_ch_e1);
}

static void e1susp_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_susp(&dma_ch_e1, val);
}

static uint32_t e1dir_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_dir(&dma_ch_e1);
}

static void e1dir_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_dir(&dma_ch_e1, val);
}

static uint32_t e1star_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_star(&dma_ch_e1);
}

static void e1star_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_star(&dma_ch_e1, val);
}

static uint32_t e1stag_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_stag(&dma_ch_e1);
}

static void e1stag_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_stag(&dma_ch_e1, val);
}

static uint32_t e1len_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_len(&dma_ch_e1);
}

static void e1len_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_len(&dma_ch_e1, val);
}

static uint32_t e2st_reg_read(struct mmio_region_g2_reg_32 *region,
                                      unsigned idx, void *ctxt) {
    return g2_dma_read_st(&dma_ch_e2);
}

static void e2st_reg_write(struct mmio_region_g2_reg_32 *region,
                                   unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_st(&dma_ch_e2, val);
}

static uint32_t e2tsel_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_tsel(&dma_ch_e2);
}

static void e2tsel_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_tsel(&dma_ch_e2, val);
}

static uint32_t e2en_reg_read(struct mmio_region_g2_reg_32 *region,
                              unsigned idx, void *ctxt) {
    return g2_dma_read_en(&dma_ch_e2);
}

static void e2en_reg_write(struct mmio_region_g2_reg_32 *region,
                           unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_en(&dma_ch_e2, val);
}

static uint32_t e2susp_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_susp(&dma_ch_e2);
}

static void e2susp_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_susp(&dma_ch_e2, val);
}

static uint32_t e2dir_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_dir(&dma_ch_e2);
}

static void e2dir_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_dir(&dma_ch_e2, val);
}

static uint32_t e2star_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_star(&dma_ch_e2);
}

static void e2star_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_star(&dma_ch_e2, val);
}

static uint32_t e2stag_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_stag(&dma_ch_e2);
}

static void e2stag_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_stag(&dma_ch_e2, val);
}

static uint32_t e2len_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_len(&dma_ch_e2);
}

static void e2len_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_len(&dma_ch_e2, val);
}

static uint32_t ddst_reg_read(struct mmio_region_g2_reg_32 *region,
                                      unsigned idx, void *ctxt) {
    return g2_dma_read_st(&dma_ch_dd);
}

static void ddst_reg_write(struct mmio_region_g2_reg_32 *region,
                                   unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_st(&dma_ch_dd, val);
}

static uint32_t ddtsel_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_tsel(&dma_ch_dd);
}

static void ddtsel_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_tsel(&dma_ch_dd, val);
}

static uint32_t dden_reg_read(struct mmio_region_g2_reg_32 *region,
                              unsigned idx, void *ctxt) {
    return g2_dma_read_en(&dma_ch_dd);
}

static void dden_reg_write(struct mmio_region_g2_reg_32 *region,
                           unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_en(&dma_ch_dd, val);
}

static uint32_t ddsusp_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_susp(&dma_ch_dd);
}

static void ddsusp_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_susp(&dma_ch_dd, val);
}

static uint32_t dddir_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_dir(&dma_ch_dd);
}

static void dddir_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_dir(&dma_ch_dd, val);
}

static uint32_t ddstar_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_star(&dma_ch_dd);
}

static void ddstar_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_star(&dma_ch_dd, val);
}

static uint32_t ddstag_reg_read(struct mmio_region_g2_reg_32 *region,
                                unsigned idx, void *ctxt) {
    return g2_dma_read_stag(&dma_ch_dd);
}

static void ddstag_reg_write(struct mmio_region_g2_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_stag(&dma_ch_dd, val);
}

static uint32_t ddlen_reg_read(struct mmio_region_g2_reg_32 *region,
                               unsigned idx, void *ctxt) {
    return g2_dma_read_len(&dma_ch_dd);
}

static void ddlen_reg_write(struct mmio_region_g2_reg_32 *region,
                            unsigned idx, uint32_t val, void *ctxt) {
    g2_dma_write_len(&dma_ch_dd, val);
}

void g2_reg_init(void) {
    init_mmio_region_g2_reg_32(&mmio_region_g2_reg_32, (void*)reg_backing);

    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADSTAG", 0x5f7800,
                                    adstag_reg_read,
                                    adstag_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADSTAR", 0x5f7804,
                                    adstar_reg_read,
                                    adstar_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADLEN", 0x5f7808,
                                    adlen_reg_read,
                                    adlen_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADDIR", 0x5f780c,
                                    addir_reg_read,
                                    addir_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADTSEL", 0x5f7810,
                                    adtsel_reg_read,
                                    adtsel_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADEN", 0x5f7814,
                                    aden_reg_read,
                                    aden_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADST", 0x5f7818,
                                    adst_reg_read,
                                    adst_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_ADSUSP", 0x5f781c,
                                    adsusp_reg_read,
                                    adsusp_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1STAG", 0x5f7820,
                                    e1stag_reg_read,
                                    e1stag_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1STAR", 0x5f7824,
                                    e1star_reg_read,
                                    e1star_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1LEN", 0x5f7828,
                                    e1len_reg_read,
                                    e1len_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1DIR", 0x5f782c,
                                    e1dir_reg_read,
                                    e1dir_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1TSEL", 0x5f7830,
                                    e1tsel_reg_read,
                                    e1tsel_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1EN", 0x5f7834,
                                    e1en_reg_read,
                                    e1en_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1ST", 0x5f7838,
                                    e1st_reg_read,
                                    e1st_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E1SUSP", 0x5f783c,
                                    e1susp_reg_read,
                                    e1susp_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2STAG", 0x5f7840,
                                    e2stag_reg_read,
                                    e2stag_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2STAR", 0x5f7844,
                                    e2star_reg_read,
                                    e2star_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2LEN", 0x5f7848,
                                    e2len_reg_read,
                                    e2len_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2DIR", 0x5f784c,
                                    e2dir_reg_read,
                                    e2dir_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2TSEL", 0x5f7850,
                                    e2tsel_reg_read,
                                    e2tsel_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2EN", 0x5f7854,
                                    e2en_reg_read,
                                    e2en_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2ST", 0x5f7858,
                                    e2st_reg_read,
                                    e2st_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_E2SUSP", 0x5f785c,
                                    e2susp_reg_read,
                                    e2susp_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDSTAG", 0x5f7860,
                                    ddstag_reg_read,
                                    ddstag_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDSTAR", 0x5f7864,
                                    ddstar_reg_read,
                                    ddstar_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDLEN", 0x5f7868,
                                    ddlen_reg_read,
                                    ddlen_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDIR", 0x5f786c,
                                    dddir_reg_read,
                                    dddir_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDTSEL", 0x5f7870,
                                    ddtsel_reg_read,
                                    ddtsel_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDEN", 0x5f7874,
                                    dden_reg_read,
                                    dden_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDST", 0x5f7878,
                                    ddst_reg_read,
                                    ddst_reg_write,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_DDSUSP", 0x5f787c,
                                    ddsusp_reg_read,
                                    ddsusp_reg_write,
                                    NULL);

    /* some debugging bullshit, hopefully I never need these... */
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_G2DSTO", 0x5f7890,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_G2TRTO", 0x5f7894,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);

    /* the modem, it will be a long time before I get around to this */
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_G2MDMTO", 0x5f7898,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_G2MDMW", 0x5f789c,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);

    /* ??? */
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78a0", 0x5f78a0,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78a4", 0x5f78a4,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78a8", 0x5f78a8,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78ac", 0x5f78ac,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78b0", 0x5f78b0,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78b4", 0x5f78b4,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "UNKNOWN_G2_REG_0x5f78b8", 0x5f78b8,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);

    mmio_region_g2_reg_32_init_cell(&mmio_region_g2_reg_32,
                                    "SB_G2APRO", 0x5f78bc,
                                    mmio_region_g2_reg_32_warn_read_handler,
                                    mmio_region_g2_reg_32_warn_write_handler,
                                    NULL);
}

void g2_reg_cleanup(void) {
    cleanup_mmio_region_g2_reg_32(&mmio_region_g2_reg_32);
}

struct memory_interface g2_intf = {
    .read32 = g2_reg_read_32,
    .read16 = g2_reg_read_16,
    .read8 = g2_reg_read_8,
    .readfloat = g2_reg_read_float,
    .readdouble = g2_reg_read_double,

    .write32 = g2_reg_write_32,
    .write16 = g2_reg_write_16,
    .write8 = g2_reg_write_8,
    .writefloat = g2_reg_write_float,
    .writedouble = g2_reg_write_double
};
