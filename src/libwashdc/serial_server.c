/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#include "hw/sh4/sh4.h"

#include "serial_server.h"

struct Sh4 *sh4;
struct serial_server_intf const *sersrv;

void serial_server_notify_tx_ready(void) {
    if (sersrv && sersrv->notify_tx_ready)
        sersrv->notify_tx_ready();
}

void
serial_server_attach(struct serial_server_intf const *intf, struct Sh4 *cpu) {
    sersrv = intf;
    sh4 = cpu;
    if (sersrv->attach)
        sersrv->attach();
}

void washdc_serial_server_rx(char ch) {
    struct text_ring *rxq = &sh4->scif.rxq;
    text_ring_produce(rxq, ch);
    sh4_scif_rx(sh4);
}

int washdc_serial_server_tx(char *ch) {
    if (text_ring_consume(&sh4->scif.txq, ch))
        return 0;
    return -1;
}

void washdc_serial_server_cts(void) {
    sh4_scif_cts(sh4);
}
