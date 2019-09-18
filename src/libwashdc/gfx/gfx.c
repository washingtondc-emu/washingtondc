/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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
#include <err.h>
#include <stdbool.h>

#include "washdc/win.h"
#include "dreamcast.h"
#include "gfx/rend_common.h"
#include "gfx/gfx_tex_cache.h"
#include "log.h"
#include "config.h"

// for the palette_tp stuff
//#include "hw/pvr2/pvr2_core_reg.h"

#include "gfx/gfx.h"

static unsigned win_width, win_height;

static unsigned frame_counter;

static struct washdc_overlay_intf const *overlay_intf;

// Only call gfx_thread_signal and gfx_thread_wait when you hold the lock.
static void gfx_do_init(struct rend_if const * rend_if);

void gfx_init(struct rend_if const * rend_if, unsigned width, unsigned height) {
    win_width = width;
    win_height = height;

    LOG_INFO("GFX: rendering graphics from within the main emulation thread\n");
    gfx_do_init(rend_if);
}

void gfx_cleanup(void) {
    rend_cleanup();
}

void gfx_expose(void) {
    gfx_redraw();
}

void gfx_redraw(void) {
    gfx_rend_ifp->video_present();
    if (overlay_intf && overlay_intf->overlay_draw)
        overlay_intf->overlay_draw();
    win_update();
}

void gfx_resize(int xres, int yres) {
    gfx_rend_ifp->video_present();
    if (overlay_intf && overlay_intf->overlay_draw)
        overlay_intf->overlay_draw();
    win_update();
}

static void gfx_do_init(struct rend_if const * rend_if) {
    win_make_context_current();

    gfx_tex_cache_init();
    rend_init(rend_if);
}

void gfx_post_framebuffer(int obj_handle,
                          unsigned fb_new_width,
                          unsigned fb_new_height, bool do_flip) {
    gfx_rend_ifp->video_new_framebuffer(obj_handle, fb_new_width, fb_new_height,
                                        do_flip);
    gfx_rend_ifp->video_present();
    if (overlay_intf && overlay_intf->overlay_draw)
        overlay_intf->overlay_draw();
    win_update();
    frame_counter++;
}

void gfx_toggle_output_filter(void) {
    gfx_rend_ifp->video_toggle_filter();
}

void gfx_set_overlay_intf(struct washdc_overlay_intf const *intf) {
    overlay_intf = intf;
}
