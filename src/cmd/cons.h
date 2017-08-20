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

#ifndef CONS_H_
#define CONS_H_

#include <stdbool.h>

/*
 * cons is the text-buffering system used by the cmd_thread.
 * it stores both an rx ring for user input and a tx ring for program output
 */

/*
 * this function can be called from any thread.  It will print the given string
 * to console's program-output (tx ring).
 *
 * This does NOT kick the cmd_thread, you have to do that yourself.
 */
void cons_puts(char const *txt);

/*
 * this function drains a single character from the tx ring.  It can only be
 * safely called from the cmd thread.
 */
bool cons_tx_drain_single(char *out);

#endif
