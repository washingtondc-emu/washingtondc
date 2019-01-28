/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019 snickerbockers
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

#ifndef ENABLE_TCP_SERIAL
#error recompile with -DENABLE_TCP_SERIAL=On
#endif

#ifndef SERIALSERVER_H_
#define SERIALSERVER_H_

#include <stdint.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

struct Sh4;

// it's 'cause 1998 is the year the Dreamcast came out in Japan
#define SERIAL_PORT_NO 1998

void serial_server_init(struct Sh4 *cpu);
void serial_server_cleanup(void);

// this function can be safely called from outside of the context of the io thread
void serial_server_attach(void);

/*
 * The SCIF calls this to let us know that it has data ready to transmit.
 * If the SerialServer is idling, it will immediately call sh4_scif_cts, and the
 * sh4 will send the data to the SerialServer via the SerialServer's put method
 *
 * If the SerialServer is active, this function does nothing and the server will call
 * sh4_scif_cts later when it is ready.
 */
void serial_server_notify_tx_ready(void);

/*
 * This gets called every time the io_thread wakes up.  It will check if any
 * work needs to be done, and it will perform that work.
 */
void serial_server_run(void);

#endif
