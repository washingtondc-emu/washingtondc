/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include <cstring>
#include <cstdlib>

#ifdef ENABLE_SERIAL_SERVER
#include "serial_server.h"
#endif

#include "dreamcast.h"
#include "sh4_reg.h"
#include "sh4_reg_flags.h"
#include "sh4.h"
#include "error.h"

#include "sh4_scif.h"

/*
 * This shouldn't be too difficult, but the authors of the official sh4
 * documentation picked some really obtuse names for the scif's registers,
 * and that makes things a little hard to understand around here.
 */

static inline bool tx_interrupt_enabled(Sh4 *sh4) {
    return bool(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_TIE_MASK);
}

static inline bool rx_interrupt_enabled(Sh4 *sh4) {
    return bool(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_RIE_MASK);
}

static inline bool tx_enabled(Sh4 *sh4) {
    return bool(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_TE_MASK);
}

static inline bool rx_enabled(Sh4 *sh4) {
    return bool(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_RE_MASK);
}

static inline bool rx_err_interrupt_enabled(Sh4 *sh4) {
    return bool(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_REIE_MASK);
}

static void check_rx_trig(Sh4 *sh4);
static void check_tx_trig(Sh4 *sh4);
static void check_rx_reset(Sh4 *sh4);
static void check_tx_reset(Sh4 *sh4);

static void push_queue(struct fifo_head *queue, uint8_t val);

// returns false if the queue was empty; else true
static bool pop_queue(struct fifo_head *queue, uint8_t *val);

/*
 * when the number of bytes remaining in the tx fifo falls below the value
 * returned by this fucntion, we have to tell the software about it via the
 * TDFE bit in SCFSR2 and (if it's enabled) the TX interrupt.
 */
static inline unsigned tx_fifo_trigger(Sh4 *sh4) {
    const static unsigned lut[4] = {
        8, 4, 2, 1
    };

    unsigned ttrg = (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_TTRG_MASK) >>
        SH4_SCFCR2_TTRG_SHIFT;

    return lut[ttrg];
}

static inline unsigned rx_fifo_trigger(Sh4 *sh4) {
    const static unsigned lut[4] = {
        1, 4, 8, 14
    };

    unsigned rtrg = (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_RTRG_MASK) >>
        SH4_SCFCR2_RTRG_SHIFT;

    return lut[rtrg];
}

extern "C"
void sh4_scif_init(sh4_scif *scif) {
    memset(scif, 0, sizeof(*scif));

    fifo_init(&scif->txq);
    fifo_init(&scif->rxq);
}

extern "C"
void sh4_scif_cleanup(sh4_scif *scif) {
    struct fifo_node *nodep;
    while ((nodep = fifo_pop(&scif->txq)))
        free(&FIFO_DEREF(nodep, struct sh4_scif_byte, node));
    while ((nodep = fifo_pop(&scif->rxq)))
        free(&FIFO_DEREF(nodep, struct sh4_scif_byte, node));

    memset(scif, 0, sizeof(*scif));
}

extern "C"
void sh4_scif_connect_server(Sh4 *sh4, struct serial_server *ser_srv) {
    sh4->scif.ser_srv = ser_srv;
}

extern "C" int
sh4_scfdr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    struct sh4_scif *scif = &sh4->scif;

    size_t rx_sz = fifo_len(&scif->rxq);
    size_t tx_sz = fifo_len(&scif->txq);

    if (rx_sz > 16)
        rx_sz = 16;

    if (tx_sz > 16)
        tx_sz = 16;

    uint16_t val = rx_sz | (tx_sz << 8);
    memcpy(buf, &val, sizeof(val));

    return 0;
}

// this is called when the software wants to read from the SCIF's rx fifo
extern "C" int
sh4_scfrdr2_reg_read_handler(Sh4 *sh4, void *buf,
                             struct Sh4MemMappedReg const *reg_info) {
   struct sh4_scif *scif = &sh4->scif;
   uint8_t val;

   if (pop_queue(&scif->rxq, &val)) {
       if (fifo_len(&sh4->scif.rxq) >= rx_fifo_trigger(sh4)) {
           sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_DR_MASK;
           sh4->scif.dr_read = false;
       }

       memcpy(buf, &val, sizeof(val));

       return 0;
   }

    // sh4 spec says the value is undefined in this case
    memset(buf, 0, sizeof(uint8_t));
    return 0;
}

// this is called when the software wants to write to the SCIF's tx fifo
extern "C" int
sh4_scftdr2_reg_write_handler(Sh4 *sh4, void const *buf,
                              struct Sh4MemMappedReg const *reg_info) {
#ifdef ENABLE_SERIAL_SERVER
    if (sh4->scif.ser_srv) {
        uint8_t dat;

        memcpy(&dat, buf, sizeof(dat));
        push_queue(&sh4->scif.txq, dat);
        serial_server_notify_tx_ready(sh4->scif.ser_srv);
    }
#endif

    return 0;
}

#ifdef ENABLE_SERIAL_SERVER

extern "C"
void sh4_scif_cts(Sh4 *sh4) {
    if (sh4->scif.ser_srv) {
        struct fifo_head *txq = &sh4->scif.txq;
        uint8_t val;

        check_tx_reset(sh4);

        if (pop_queue(txq, &val)) {
            serial_server_put(sh4->scif.ser_srv, val);
            check_tx_trig(sh4);
        } else {
            sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_TEND_MASK;
        }
    }
}

#endif

extern "C"
void sh4_scif_rx(Sh4 *sh4, uint8_t dat) {
    push_queue(&sh4->scif.rxq, dat);

    if (fifo_len(&sh4->scif.rxq) >= rx_fifo_trigger(sh4))
        sh4->reg[SH4_REG_SCFSR2] &= ~SH4_SCFSR2_DR_MASK;

    check_rx_reset(sh4);
    check_rx_trig(sh4);
}

static void check_rx_trig(Sh4 *sh4) {
    unsigned rtrg = rx_fifo_trigger(sh4);

    if (fifo_len(&sh4->scif.rxq) >= rtrg) {
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_RDF_MASK;

        if (rx_interrupt_enabled(sh4))
            sh4_set_interrupt(sh4, SH4_IRQ_SCIF, SH4_EXCP_SCIF_RXI);
    }
}

static void check_tx_trig(Sh4 *sh4) {
    unsigned ttrg = tx_fifo_trigger(sh4);

    if (fifo_len(&sh4->scif.txq) <= ttrg) {
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_TDFE_MASK;

        if (tx_interrupt_enabled(sh4))
            sh4_set_interrupt(sh4, SH4_IRQ_SCIF, SH4_EXCP_SCIF_TXI);
    }
}

static void check_rx_reset(Sh4 *sh4) {
    if (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_RFRST_MASK) {
        while (pop_queue(&sh4->scif.rxq, NULL))
            ;

        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_DR_MASK;
    }
}

static void check_tx_reset(Sh4 *sh4) {
    if (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_TFRST_MASK) {
        while (pop_queue(&sh4->scif.txq, NULL))
            ;
    }
}

extern "C" int
sh4_scsmr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_SCSCR2], reg_info->len);
    return 0;
}

extern "C" int
sh4_scsmr2_reg_write_handler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    uint16_t dat;
    memcpy(&dat, buf, sizeof(dat));

    dat &= 0x7b;
    sh4->reg[SH4_REG_SCSCR2] = dat;

    if (!(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_TE_MASK))
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_TEND_MASK;

    return 0;
}

extern "C" int
sh4_scfcr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_SCFCR2], reg_info->len);

    return 0;
}

extern "C" int
sh4_scfcr2_reg_write_handler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->reg[SH4_REG_SCFCR2], buf, reg_info->len);

    // need to check these here due to potential flag changes
    check_rx_trig(sh4);
    check_tx_trig(sh4);
    check_rx_reset(sh4);
    check_tx_reset(sh4);

    return 0;
}

extern "C" int
sh4_scscr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_SCSCR2], reg_info->len);
    return 0;
}

extern "C" int
sh4_scscr2_reg_write_handler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->reg[SH4_REG_SCSCR2], buf, reg_info->len);

    // need to check these because the interrupts might have been enabled
    check_rx_trig(sh4);
    check_tx_trig(sh4);

    return 0;
}

extern "C" int
sh4_scfsr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    uint16_t tmp = sh4->reg[SH4_REG_SCFSR2];

    if (tmp & SH4_SCFSR2_TEND_MASK)
        sh4->scif.tend_read = true;
    if (tmp & SH4_SCFSR2_DR_MASK)
        sh4->scif.dr_read = true;
    if (tmp & SH4_SCFSR2_TDFE_MASK)
        sh4->scif.tdfe_read = true;
    if (tmp & SH4_SCFSR2_RDF_MASK)
        sh4->scif.rdf_read = true;

    memcpy(buf, &tmp, reg_info->len);

    return 0;
}

extern "C" int
sh4_scfsr2_reg_write_handler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    uint16_t new_val, orig_val;

    memcpy(&new_val, buf, reg_info->len);

    orig_val = sh4->reg[SH4_REG_SCFSR2];

    size_t tx_sz = fifo_len(&sh4->scif.txq);
    size_t rx_sz = fifo_len(&sh4->scif.rxq);

    bool turning_off_tend = !(new_val & SH4_SCFSR2_TEND_MASK) &&
        (orig_val & SH4_SCFSR2_TEND_MASK);
    if (turning_off_tend && sh4->scif.tend_read) {
        if (!(sh4->scif.tend_read && tx_sz))
            new_val |= SH4_SCFSR2_TEND_MASK;
    }

    bool turning_off_dr = !(new_val & SH4_SCFSR2_DR_MASK) &&
        (orig_val & SH4_SCFSR2_DR_MASK);
    if (turning_off_dr && sh4->scif.dr_read) {
        if (rx_sz < rx_fifo_trigger(sh4))
            new_val |= SH4_SCFSR2_DR_MASK;
    }

    bool turning_off_tdfe = !(new_val & SH4_SCFSR2_TDFE_MASK) &&
        (orig_val & SH4_SCFSR2_TDFE_MASK);
    if (turning_off_tdfe && sh4->scif.tdfe_read) {
        if (tx_sz <= tx_fifo_trigger(sh4))
            new_val |= SH4_SCFSR2_TDFE_MASK;
    }

    bool turning_off_rdf = !(new_val & SH4_SCFSR2_RDF_MASK) &&
        (orig_val & SH4_SCFSR2_RDF_MASK);
    if (turning_off_rdf && sh4->scif.rdf_read) {
        if (rx_sz >= rx_fifo_trigger(sh4))
            new_val |= SH4_SCFSR2_RDF_MASK;
    }

    sh4->reg[SH4_REG_SCFSR2] = new_val;

    return 0;
}

static void push_queue(struct fifo_head *queue, uint8_t val) {
    struct sh4_scif_byte *cont =
        (struct sh4_scif_byte*)malloc(sizeof(struct sh4_scif_byte));
    if (!cont) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    cont->dat = val;
    fifo_push(queue, &cont->node);
}

static bool pop_queue(struct fifo_head *queue, uint8_t *val) {
    struct fifo_node *nodep = fifo_pop(queue);

    if (!nodep)
        return false;

    struct sh4_scif_byte *cont = &FIFO_DEREF(nodep, struct sh4_scif_byte, node);

    if (val)
        *val = cont->dat;

    free(cont);
    return true;
}
