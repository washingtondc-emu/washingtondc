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

#include <string.h>

#include "washdbg.h"

#include "washdbg_core.h"

bool washdbg_on_step(addr32_t addr, void *argp) {
    return true;
}

void washdbg_do_continue(void) {
    washdbg_puts("Continuing execution\n");

    debug_request_continue();
}

void washdbg_input_text(char const *txt) {
    /*
     * TODO: THIS IS NOT THREAD-SAFE.
     *
     * WE NEED A TEXT RING!
     */
    washdbg_puts(txt);
    if (strcmp(txt, "c") == 0)
        washdbg_do_continue();
    else
        washdbg_puts("Unrecognized input\n");
}
