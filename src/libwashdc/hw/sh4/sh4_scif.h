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

#ifndef SH4_SCIF_H_
#define SH4_SCIF_H_

#include <stdint.h>
#include <stdatomic.h>

#include "washdc/ring.h"

/*
 * SH4 SCIF (Serial Port) emulation
 *
 * This hooks up the Dreamcast's serial port to TCP/IP so you can interact
 * with it using programs like telnet.
 *
 * The timing here isn't very accurate, due to equal parts necessity and
 * lazyness.  The best-case-scenario bandwidth is 115.2 kbaud (and the
 * practical-scenario is significantly lower than that) and since the virtual
 * dreamcast could be running significantly slower than a real dreamcast, it
 * doesn't make sense to implement this in the same way as a real SH4 SCIF.
 * There has to be additional buffering well in excess of the measly 16-bytes
 * that you'd get on real hardware.
 *
 * One way to implement this accurately (this is where the lazy part comes in)
 * would be to simulate an actual terminal server connected to the SCIF. 
 * The virtual terminal server would buffer data taken over TCP (since
 * ostensibly real terminal servers have to solve the problem of asymmetric
 * bandwidth too) and send that data over a simulated serial link to the SCIF
 * at a steady 9.6 kbps (or whatever bandwidth the user has configured), with
 * the flow-control, stop bits, parity, etc all taken into account.
 *
 * AFAIK the only official releases that use the serial port are a handful of
 * Japan-only SNK releases that hook up with the Neo Geo Pocket Color, which
 * isn't something I'm going to be in a position to support any time soon.  The
 * homebrew stuff primarily uses it for logging and console access, for which
 * purposes HLE will suffice.  I might need to rewrite this all later if I want
 * to support those SD card adapters that can plug into the serial port, but as
 * with the Neo Geo Pocket stuff, that's a long way into the future.
 */

#define SCIF_BUF_LEN 16

struct sh4_scif {
    // for txq, the SCIF is the producer
    // for rxq, the SCIF is the consumer
    struct text_ring txq, rxq;

    /*
     * We dequeue stuff from txq and rxq as often as we can into these two
     * buffers.
     */
    char tx_buf[SCIF_BUF_LEN];
    char rx_buf[SCIF_BUF_LEN];
    unsigned tx_buf_len, rx_buf_len;

    /*
     * For the DR, TEND, TDFE and RDF bits in SCFSR2, the SH4 spec states that
     * software can only clear these bits after reading a 1 from them.  These
     * four booleans track whether the SCFSR2 register has been read from since
     * the last time the corresponding bit was set to 1.
     */
    bool tend_read, dr_read, tdfe_read, rdf_read;

    bool ser_srv_connected;

    atomic_flag nothing_pending;
};

typedef struct sh4_scif sh4_scif;

struct Sh4;

void sh4_scif_init(sh4_scif *scif);
void sh4_scif_cleanup(sh4_scif *scif);

void sh4_scif_connect_server(Sh4 *sh4);

sh4_reg_val
sh4_scfdr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info);

sh4_reg_val
sh4_scfrdr2_reg_read_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info);

void
sh4_scftdr2_reg_write_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info,
                              sh4_reg_val val);

/* the SH4 Serial Mode Register */
int
sh4_scsmr2_reg_read_handler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info);
int
sh4_scsmr2_reg_write_handler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info);

/* the SH4 FIFO Control Register */
sh4_reg_val
sh4_scfcr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info);
void
sh4_scfcr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val);

/* Serial Control Register */
sh4_reg_val
sh4_scscr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info);
void
sh4_scscr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val);

/* Serial Status Register */
sh4_reg_val
sh4_scfsr2_reg_read_handler(Sh4 *sh4,
                            struct Sh4MemMappedReg const *reg_info);
void
sh4_scfsr2_reg_write_handler(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val);

/*
 * Called by the serial server when it's hungry for more data.
 * This is analagous to the RS-232 clear-to-send signal, which is why it's
 * called sh4_scif_cts.
 */
void sh4_scif_cts(Sh4 *sh4);

// Called by the serial server whenever it has another byte.
void sh4_scif_rx(Sh4 *sh4);

void sh4_scif_periodic(Sh4 *sh4);

#endif
