/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
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

#ifndef RENDERER_H_
#define RENDERER_H_

#include "washdc/gfx/gfx_all.h"

#ifdef __cplusplus
extern "C" {
#endif

struct renderer_callbacks {
    // tells the window to check for events.  This is optional and can be NULL
    void (*win_update)(void);

    // tells the overlay to draw.  This is optional and can be NULL
    void (*overlay_draw)(void);
};

struct renderer {
    // for receiving rendering commands from washdc's gfx infrastructure
    struct gfx_rend_if const* rend_if;

    void (*set_callbacks)(struct renderer_callbacks const* callbacks);

    // optional, can be NULL (but probably shouldn't)
    void (*video_present)(void);

    // optional, can be NULL
    void (*toggle_video_filter)(void);

    // optional, can be NULL if your renderer doesn't support renderdoc
    void (*capture_renderdoc)(void);
};

#ifdef __cplusplus
}
#endif

#endif
