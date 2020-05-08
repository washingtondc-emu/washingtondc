/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2020 snickerbockers
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

#ifndef SERIALSERVER_HPP_
#define SERIALSERVER_HPP_

#include <cstdint>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include "washdc/serial_server.h"

#ifndef ENABLE_TCP_SERIAL
#error this file should not be built with ENABLE_TCP_SERIAL disabled!
#endif

// it's 'cause 1998 is the year the Dreamcast came out in Japan
#define SERIAL_PORT_NO 1998

void serial_server_init(void);
void serial_server_cleanup(void);

/*
 * This gets called every time the io_thread wakes up.  It will check if any
 * work needs to be done, and it will perform that work.
 */
void serial_server_run(void);

extern struct serial_server_intf sersrv_intf;

#endif
