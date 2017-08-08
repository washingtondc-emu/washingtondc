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

#include <event2/event.h>

/*
 * the io thread runs libevent in a separate thread to perform asynchronus IO
 * on behalf of other threads without impacting performance (calling
 * event_base_loop from the emulation thread was causing a noticible loss of
 * performance even with EVLOOP_NONBLOCK).
 *
 * Users register callbacks for read/write operations.  These callbacks will be
 * called from the io thread, so it is up to them to move the data to whatever
 * thread needs the data in a safe manner.
 */

void io_thread_launch(void);

void io_thread_join(void);

/*
 * tell the io thread to wake up and check dc_is_running.
 * If dreamcast_kill has not yet been called, then this function is effectively
 * a no-op.
 */
void io_thread_kick(void);

extern struct event_base *io_thread_event_base;
