/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include <stdio.h>
#include <string.h>

#include "io/washdbg_tcp.h"

#include "dbg/washdbg_core.h"

#define BUF_LEN 1024

static char in_buf[BUF_LEN];
unsigned in_buf_pos;

static void washdbg_process_line(void);

bool washdbg_on_step(addr32_t addr, void *argp) {
    return true;
}

void washdbg_do_continue(void) {
    washdbg_puts("Continuing execution\n");

    debug_request_continue();
}

void washdbg_input_ch(char ch) {
    if (ch == '\r')
        return;
    if (ch == '\n') {
        washdbg_process_line();
        return;
    }

    // in_buf[1023] will always be \0
    if (in_buf_pos <= 1022)
        in_buf[in_buf_pos++] = ch;
}

static void washdbg_process_line(void) {
    if (strcmp(in_buf, "c") == 0) {
        washdbg_puts("continue!\n");
        washdbg_do_continue();
    } else {
        washdbg_puts("Unrecognized input \"");
        washdbg_puts(in_buf);
        washdbg_puts("\"\n");
    }

    memset(in_buf, 0, sizeof(in_buf));
    in_buf_pos = 0;
}

void washdbg_input_text(char const *txt) {
    while (*txt)
        washdbg_input_ch(*txt++);
}
