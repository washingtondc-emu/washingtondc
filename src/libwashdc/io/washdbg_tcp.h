/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef WASHDBG_H_
#define WASHDBG_H_

#include "washdc/debugger.h"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

/*
 * It's safe to have this overlap with the gdb port because you won't use both
 * at the same time.
 */
#define WASHDBG_PORT 1999

extern struct debug_frontend washdbg_frontend;

void washdbg_tcp_init(void);
void washdbg_tcp_cleanup(void);

int washdbg_tcp_puts(char const *str);

#endif
