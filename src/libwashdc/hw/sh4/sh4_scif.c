/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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
#include <stdlib.h>
#include <stdbool.h>

#ifdef ENABLE_TCP_SERIAL
#include "io/serial_server.h"
#endif

#include "dreamcast.h"
#include "sh4_reg.h"
#include "sh4_reg_flags.h"
#include "sh4.h"
#include "washdc/error.h"
#include "log.h"
#include "dc_sched.h"
#include "dreamcast.h"

#include "sh4_scif.h"

static void sh4_scif_rxi_int_handler(struct SchedEvent *event);
static void sh4_scif_txi_int_handler(struct SchedEvent *event);

static struct SchedEvent sh4_scif_rxi_int_event = {
    .handler = sh4_scif_rxi_int_handler
};

static struct SchedEvent sh4_scif_txi_int_event = {
    .handler = sh4_scif_txi_int_handler
};

static bool sh4_scif_rxi_int_event_scheduled;
static bool sh4_scif_txi_int_event_scheduled;

/*
 * This shouldn't be too difficult, but the authors of the official sh4
 * documentation picked some really obtuse names for the scif's registers,
 * and that makes things a little hard to understand around here.
 */

static inline bool tx_interrupt_enabled(Sh4 *sh4) {
    return (bool)(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_TIE_MASK);
}

static inline bool rx_interrupt_enabled(Sh4 *sh4) {
    return (bool)(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_RIE_MASK);
}

__attribute__((unused))
static inline bool tx_enabled(Sh4 *sh4) {
    return (bool)(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_TE_MASK);
}

__attribute__((unused))
static inline bool rx_enabled(Sh4 *sh4) {
    return (bool)(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_RE_MASK);
}

__attribute__((unused))
static inline bool rx_err_interrupt_enabled(Sh4 *sh4) {
    return (bool)(sh4->reg[SH4_REG_SCSCR2] & SH4_SCSCR2_REIE_MASK);
}

/*
 * receive a character from the rxq into the rx_buf.
 * This function will return true if the operation succeeded.
 */
static bool recv_char(struct sh4_scif *scif) {
    if (scif->rx_buf_len >= SCIF_BUF_LEN)
        return false;

    struct text_ring *rxq = &scif->rxq;

    if (!text_ring_consume(rxq, scif->rx_buf + scif->rx_buf_len))
        return false;
    scif->rx_buf_len++;

    return true;
}

/*
 * similar to recv_char, but for the txq.  This function moves a character from
 * the tx_buf into the txq.  If the operation succeeded, it returns true.  If
 * the operation failed, it returns false.
 */
static bool send_char(struct sh4_scif *scif) {
    if (scif->tx_buf_len <= 0)
        return false;

    struct text_ring *txq = &scif->txq;

    text_ring_produce(txq, scif->tx_buf[0]);
    memmove(scif->tx_buf, scif->tx_buf + 1,
            (SCIF_BUF_LEN - 1) * sizeof(scif->tx_buf[0]));
    scif->tx_buf_len--;

    return true;
}

//  get data in the rx_buf if possible
static void fill_rx_buf(struct sh4_scif *scif) {
    while (recv_char(scif))
        ;
}

static void drain_tx_buf(struct sh4_scif *scif) {
    while (send_char(scif))
        ;
}

/*
 * read a character from the rx_buf.  This function returns true if the
 * character was successsfully read, else 0 (which would mean the rx_buf and
 * rxq are both empty).
 */
static bool read_char(struct sh4_scif *scif, char *char_out) {
    fill_rx_buf(scif);

    if (scif->rx_buf_len > 0) {
        *char_out = scif->rx_buf[0];
        memmove(scif->rx_buf, scif->rx_buf + 1,
                (SCIF_BUF_LEN - 1) * sizeof(scif->rx_buf[0]));
        scif->rx_buf_len--;

        fill_rx_buf(scif);

        return true;
    }

    return false;
}

#ifdef ENABLE_TCP_SERIAL
static bool write_char(struct sh4_scif *scif, char in) {
    drain_tx_buf(scif);

    if (scif->tx_buf_len < SCIF_BUF_LEN) {
        scif->tx_buf[scif->tx_buf_len++] = in;

        drain_tx_buf(scif);

        return true;
    }

    return false;
}
#endif

static void check_rx_trig(Sh4 *sh4);
static void check_tx_trig(Sh4 *sh4);
static void check_rx_reset(Sh4 *sh4);
static void check_tx_reset(Sh4 *sh4);

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

void sh4_scif_init(sh4_scif *scif) {
    memset(scif, 0, sizeof(*scif));

    text_ring_init(&scif->rxq);
    text_ring_init(&scif->txq);

    atomic_flag_test_and_set(&scif->nothing_pending);
}

void sh4_scif_cleanup(sh4_scif *scif) {
    memset(scif, 0, sizeof(*scif));
}

void sh4_scif_connect_server(Sh4 *sh4) {
    sh4->scif.ser_srv_connected = true;
}

sh4_reg_val
sh4_scfdr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info) {
    struct sh4_scif *scif = &sh4->scif;

    size_t rx_sz = scif->rx_buf_len;
    size_t tx_sz = scif->tx_buf_len;

    if (rx_sz > 16)
        rx_sz = 16;

    if (tx_sz > 16)
        tx_sz = 16;

    uint16_t val = rx_sz | (tx_sz << 8);

    return val;
}

// this is called when the software wants to read from the SCIF's rx fifo
sh4_reg_val
sh4_scfrdr2_reg_read_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info) {
   struct sh4_scif *scif = &sh4->scif;
   char val;

   if (read_char(scif, &val)) {
       if (scif->rx_buf_len >= rx_fifo_trigger(sh4)) {
           sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_DR_MASK;
           sh4->scif.dr_read = false;
       }

       return val;
   }

   // sh4 spec says the value is undefined in this case
   return 0;
}

// this is called when the software wants to write to the SCIF's tx fifo
void
sh4_scftdr2_reg_write_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info,
                              sh4_reg_val val) {
#ifdef ENABLE_TCP_SERIAL
  struct sh4_scif *scif = &sh4->scif;
    if (scif->ser_srv_connected) {
        uint8_t dat = val;
        write_char(scif, (char)dat);

        serial_server_notify_tx_ready();
    }
#endif
}

void sh4_scif_cts(Sh4 *sh4) {
    atomic_flag_clear(&sh4->scif.nothing_pending);
}

void sh4_scif_rx(Sh4 *sh4) {
    atomic_flag_clear(&sh4->scif.nothing_pending);
}

static void check_rx_trig(Sh4 *sh4) {
    unsigned rtrg = rx_fifo_trigger(sh4);

    struct sh4_scif *scif = &sh4->scif;
    fill_rx_buf(scif);

    if (scif->rx_buf_len >= rtrg) {
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_RDF_MASK;

        if (rx_interrupt_enabled(sh4)) {
            if (!sh4_scif_rxi_int_event_scheduled) {
                sh4_scif_rxi_int_event_scheduled = true;
                sh4_scif_rxi_int_event.when = clock_cycle_stamp(sh4->clk);
                sh4_scif_rxi_int_event.arg_ptr = sh4;
                sched_event(sh4->clk, &sh4_scif_rxi_int_event);
            }
        }
    }
}

static void check_tx_trig(Sh4 *sh4) {
    unsigned ttrg = tx_fifo_trigger(sh4);

    if (sh4->scif.tx_buf_len <= ttrg) {
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_TDFE_MASK;

        if (tx_interrupt_enabled(sh4)) {
            if (!sh4_scif_txi_int_event_scheduled) {
                sh4_scif_txi_int_event_scheduled = true;
                sh4_scif_txi_int_event.when = clock_cycle_stamp(sh4->clk);
                sh4_scif_txi_int_event.arg_ptr = sh4;
                sched_event(sh4->clk, &sh4_scif_txi_int_event);
            }
        }
    }
}

static void check_rx_reset(Sh4 *sh4) {
    if (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_RFRST_MASK) {
        sh4->scif.rx_buf_len = 0;
        char trash;
        struct sh4_scif *scif = &sh4->scif;
        while (read_char(scif, &trash))
            ;

        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_DR_MASK;
    }
}

static void check_tx_reset(Sh4 *sh4) {
    if (sh4->reg[SH4_REG_SCFCR2] & SH4_SCFCR2_TFRST_MASK) {
        /*
         * TODO implement this without creating a race condition
         *
         * The complication here is that only the serial_server is allowed to
         * consume from the txq, yet somehow we need to empty it here.
         */
        LOG_WARN("WARNING: %s not implemented\n", __func__);
        /* while (pop_queue(&sh4->scif.txq, NULL)) */
        /*     ; */
    }
}

int
sh4_scsmr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_SCSCR2], reg_info->len);
    return 0;
}

int
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

sh4_reg_val
sh4_scfcr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info) {
    return sh4->reg[SH4_REG_SCFCR2];
}

void
sh4_scfcr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val) {
    sh4->reg[SH4_REG_SCFCR2] = val;

    // need to check these here due to potential flag changes
    check_rx_trig(sh4);
    check_tx_trig(sh4);
    check_rx_reset(sh4);
    check_tx_reset(sh4);
}

sh4_reg_val
sh4_scscr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info) {
    return sh4->reg[SH4_REG_SCSCR2];
}

void
sh4_scscr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val) {
    sh4->reg[SH4_REG_SCSCR2] = val;

    // need to check these because the interrupts might have been enabled
    check_rx_trig(sh4);
    check_tx_trig(sh4);
}

sh4_reg_val
sh4_scfsr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info) {
    sh4_reg_val tmp = sh4->reg[SH4_REG_SCFSR2];

    if (tmp & SH4_SCFSR2_TEND_MASK)
        sh4->scif.tend_read = true;
    if (tmp & SH4_SCFSR2_DR_MASK)
        sh4->scif.dr_read = true;
    if (tmp & SH4_SCFSR2_TDFE_MASK)
        sh4->scif.tdfe_read = true;
    if (tmp & SH4_SCFSR2_RDF_MASK)
        sh4->scif.rdf_read = true;

    return tmp;
}

void
sh4_scfsr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val) {
    struct sh4_scif *scif = &sh4->scif;
    uint16_t new_val, orig_val;

    new_val = val;

    orig_val = sh4->reg[SH4_REG_SCFSR2];

    fill_rx_buf(scif);

    size_t tx_sz = scif->tx_buf_len;
    size_t rx_sz = scif->rx_buf_len;

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
}

/*
 * XXX I wish I could find a way to do SCIF events without relying on the
 * periodic event handler.  This is currently necessary because I connect
 * the SCIF to TCP via the io_thread, which means that the io_thread needs
 * to be able to raise SCIF interrupts.  Signalling the emulation thread to
 * do that when it's ready seems like the best way to do that, but I really
 * don't like solutions that rely on polling because it seems inefficient.
 */
void sh4_scif_periodic(Sh4 *sh4) {
    struct sh4_scif *scif = &sh4->scif;

    fill_rx_buf(scif);
    drain_tx_buf(scif);

    check_rx_reset(sh4);
    check_tx_reset(sh4);
    check_rx_trig(sh4);
    check_tx_trig(sh4);

    if (scif->tx_buf_len == 0) {
        sh4->reg[SH4_REG_SCFSR2] |= SH4_SCFSR2_TEND_MASK;
    }

    if (scif->rx_buf_len >= rx_fifo_trigger(sh4))
        sh4->reg[SH4_REG_SCFSR2] &= ~SH4_SCFSR2_DR_MASK;
}

static void sh4_scif_rxi_int_handler(struct SchedEvent *event) {
    Sh4 *sh4 = (Sh4*)event->arg_ptr;
    sh4_scif_rxi_int_event_scheduled = false;
    sh4_set_interrupt(sh4, SH4_IRQ_SCIF, SH4_EXCP_SCIF_RXI);
}

static void sh4_scif_txi_int_handler(struct SchedEvent *event) {
    Sh4 *sh4 = (Sh4*)event->arg_ptr;
    sh4_scif_txi_int_event_scheduled = false;
    sh4_set_interrupt(sh4, SH4_IRQ_SCIF, SH4_EXCP_SCIF_TXI);
}
