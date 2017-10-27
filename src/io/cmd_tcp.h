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

#ifndef CMD_TCP_H_
#define CMD_TCP_H_

#include <stdbool.h>

/*
 * TCP/IP frontend for WashingtonDC's command-line interface.
 *
 * This runs from the io_thread and pushes moves text to the cmd system via a
 * text_ring.  It has a handler that runs in the emu_thread and pushes the text
 * into the command-line interface.
 */

#define CMD_TCP_PORT_NO 2000

/*
 * initialize/deinitialize the TCP/IP cli frontend.
 *
 * These should only be called from the io_thread.
 */
void cmd_tcp_init(void);
void cmd_tcp_cleanup(void);

/*
 * this function gets called from the emulation thread to request a connection.
 * It will block until a connection is established.
 */
void cmd_tcp_attach(void);

/*
 * write c to the tcp frontend.
 * This can only safely be called from the cmd thread
 */
void cmd_tcp_put_text(char const *txt);

/*
 * read a character from the tcp frontend and store it in *out
 *
 * if this returns false, then *out is invalid and the ring is empty.
 * otherwise *out is valid.  It might be empty, but you can only know by
 * calling cmd_tcp_get again
 */
bool cmd_tcp_get(char *out);

#endif
