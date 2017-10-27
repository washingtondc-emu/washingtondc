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

#include <err.h>
#include <stdlib.h>

#include "cmd.h"
#include "cons.h"
#include "dreamcast.h"
#include "cmd_tcp_link.h"

#include "cmd_sys.h"

static void cmd_sys_drain_cons_tx(void);
static void cmd_sys_drain_cons_rx(void);

// dump the given string onto all of the cmd frontends
static void cmd_sys_print_no_lock(char const *txt);

void cmd_run_once(void) {
    /*
     * the ordering here is very important.  We have to drain the tx last
     * because we want to make sure that any calls to cons_put that come
     * from the cmd system get processed.
     */
    cmd_sys_drain_cons_rx();
    cmd_sys_drain_cons_tx();
}

/*
 * this function drains the cons tx ring and sends data one line at a time.
 *
 * If there's any data left over after the last line or there are lines longer
 * than 1024 characters, then this function can send partial lines.
 */
#define CONS_BUF_LINE_LEN_SHIFT 10
#define CONS_BUF_LINE_LEN (1 << CONS_BUF_LINE_LEN_SHIFT)
static void cmd_sys_drain_cons_tx(void) {
    static char cons_buf_line[CONS_BUF_LINE_LEN];
    unsigned idx = 0;
    char ch;

    while (cons_tx_drain_single(&ch)) {
        cons_buf_line[idx++] = ch;

        if ((idx == (CONS_BUF_LINE_LEN - 1)) || (ch == '\n')) {
            cons_buf_line[idx] = '\0';
            cmd_sys_print_no_lock(cons_buf_line);
            idx = 0;
        }
    }

    if (idx) {
        cons_buf_line[idx] = '\0';
        cmd_sys_print_no_lock(cons_buf_line);
    }
}

static void cmd_sys_drain_cons_rx(void) {
    char ch;
    while (cons_getc(&ch))
        cmd_put_char(ch);
}

static void cmd_sys_print_no_lock(char const *txt) {
    // TODO: pass data on to other frontends here, too
    cmd_tcp_link_put_text(txt);
}
