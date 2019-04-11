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

#ifndef ENABLE_TCP_CMD
#error ENABLE_TCP_CMD is not enabled
#endif

#include <stdio.h>

#include "io/cmd_tcp.h"
#include "cmd_sys.h"

#include "cmd_tcp_link.h"

/*
 * the code in this file runs in the emu thread and is used to facilitate
 * transfers between the cmd system and the cmd_tcp code in the io_thread.
 */
void cmd_tcp_link_put_text(char const *txt) {
    cmd_tcp_put_text(txt);
}
