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

#ifndef PVR2_FRAMEBUFFER_H_
#define PVR2_FRAMEBUFFER_H_

#include <stdint.h>

struct pvr2;

/*
 * The framebuffer runs in the Dreamcast thread.  On vsync events, it is called
 * to copy data from the Dreamcast's framebuffer (in texture memory) to host
 * memory and them wake up the opengl backend (which runs in the windowing
 * thread) so that it  renders the framebuffer as a textured quad.
 */

enum FramebufferFormat {
    RGB_555,
    RGB_565,

    /*
     * the difference between RGB_888 and RGB_0888 is that RGB_888 represents
     * each pixel as 3 8-bit channels stored independently, while RGB_0888
     * represents each pixel as 3 8-bit channels stored in a 32-bit int (meaning
     * that each pixel consists of 8 bits of padding followed by 24 bits of
     * color)
     */
    RGB_888,
    RGB_0888
};

/*
 * HOKAY:
 *
 * This is a lot to think about but we're gonna get through it, folks:
 *
 * the PVR2 will do all its calcualtions as ARGB8888
 * When it writes the output to the framebuffer, it does that in whatever
 * format was specified in the FB_W_CTRL register (although apparently you can't
 * use the 4444 ARGB mode unless rendering to a texture?).  THEN, when the data
 * is being sent to the CRT, it gets converted to 888RGB + chroma bit ?
 *
 * ANYWAYS
 * The algorithm we're going to go with here is:
 * 1. Use OpenGL to handle any fancy 3D PVR2 renderings
 * 2. When the CPU attempts to read from/write to the framebuffer, we shall
 *    first read from the OpenGL color buffer using PBOs and store that in the
 *    Dreamcast's framebuffer
 * 3. If the user then attempts to perform some PVR2 3D rendering, we will then
 *    take the Dreamcast's framebuffer, upload it to an OpenGL texture and
 *    render that texture as a quad which encompasses the entire rendering
 *    area.  Depth buffer writes will be disabled during this pass.
 * 4. WHEN THE VBLANK INTERRUPT ARRIVES, we will copy the OpenGL color buffer
 *    to the DC framebuffer if the DC framebuffer is not the latest version of
 *    what has been rendered.  We will then render the DC framebuffer to the
 *    screen as a textured quad that encompasses the entire screen.
 *
 * This algorithm is not particularly high-performance (especially since we
 * always copy the OpenGL color buffer to framebuffer regardless of whether
 * it's necessary), but it is realatively simple and it avoids the need for
 * some sort of special case in the final output stage.  Right now, performance
 * takes a backseat to basic functionality.
 *
 * The FB_R_CTRL and FB_R_SOF1/FB_R_SOF2 registers control settings for the
 * framebuffer->CRT transfer; the FB_W_CTRL and FB_W_SOF1/FB_W_SOF2 registers
 * control settings for the PVR2->framebuffer transfer.
 */

struct fb_flags {
    uint8_t state : 2;
    uint8_t fmt : 3;
    uint8_t vert_flip : 1;
};

#define FB_HEAP_SIZE 8
struct framebuffer {
    int obj_handle;
    unsigned fb_read_width, fb_read_height;

    // only used for writing back to texture memory
    unsigned linestride;

    uint32_t addr_first[2], addr_last[2];
    uint32_t addr_key; // min of addr_first[0], addr_first[1]

    unsigned stamp;

    // These variables are only valid if flags.state == FB_STATE_GFX
    unsigned tile_w, tile_h, x_clip_min, x_clip_max,
        y_clip_min, y_clip_max;

    struct fb_flags flags;
};

#define OGL_FB_W_MAX (0x3ff + 1)
#define OGL_FB_H_MAX (0x3ff + 1)
#define OGL_FB_BYTES (OGL_FB_W_MAX * OGL_FB_H_MAX * 4)

struct pvr2_fb {
    uint8_t ogl_fb[OGL_FB_BYTES];
    struct framebuffer fb_heap[FB_HEAP_SIZE];
    unsigned stamp;
};

void pvr2_framebuffer_init(struct pvr2 *pvr2);
void pvr2_framebuffer_cleanup(struct pvr2 *pvr2);

void framebuffer_render(struct pvr2 *pvr2);

// old deprecated function that should not be called anymore
static inline void framebuffer_sync_from_host_maybe(void) {
}

int framebuffer_set_render_target(struct pvr2 *pvr2);

void framebuffer_get_render_target_dims(struct pvr2 *pvr2, int tgt,
                                        unsigned *width, unsigned *height);

void pvr2_framebuffer_notify_write(struct pvr2 *pvr2, uint32_t addr,
                                   unsigned n_bytes);

void pvr2_framebuffer_notify_texture(struct pvr2 *pvr2, uint32_t first_tex_addr,
                                     uint32_t last_tex_addr);

#endif
