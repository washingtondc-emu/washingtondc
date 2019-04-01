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

#ifndef USE_LIBEVENT
#error this should not be built when -DUSE_LIBEVENT=Off
#endif

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
 *
 * TODO: this thread can only be safely called from the emu (main) thread and
 * the io_thread because it calls libevent functions, and those functions might
 * not work if libevent is not initialized.  There's no way to know if libevent
 * is initialized from within this function so the only threads that can call
 * this are threads that implicitly know whether or not libevent is initialized.
 * That means the io_thread can call this (although I'm not sure there's any
 * good reason to) and the main/emu thread can call this (because it is the
 * thread which signals the io_thread to clean up and exit).  This is good
 * enough for now because the main/emu thread is the only thread which uses this
 * function anyways.  In the future I will need to revise this behavior if I
 * ever choose to let more threads call this function.
 */
void io_thread_kick(void);

extern struct event_base *io_thread_event_base;
