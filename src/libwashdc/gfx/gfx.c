/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#include "washdc/win.h"
#include "dreamcast.h"
#include "gfx/rend_common.h"
#include "log.h"
#include "config.h"

// for the palette_tp stuff
//#include "hw/pvr2/pvr2_core_reg.h"

#include "gfx/gfx.h"

// Only call gfx_thread_signal and gfx_thread_wait when you hold the lock.
static void gfx_do_init(struct gfx_rend_if const * rend_if);

void gfx_init(struct gfx_rend_if const * rend_if) {
    LOG_INFO("GFX: rendering graphics from within the main emulation thread\n");
    gfx_do_init(rend_if);
}

void gfx_cleanup(void) {
    rend_cleanup();
}

static void gfx_do_init(struct gfx_rend_if const * rend_if) {
    win_make_context_current();

    rend_init(rend_if);
}
