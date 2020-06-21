/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
