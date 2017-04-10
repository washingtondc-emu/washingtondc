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

#include <boost/cstdint.hpp>

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
void framebuffer_init(unsigned width, unsigned height);

void framebuffer_render();
// void conv_rgb888_to_rgb888(uint32_t *pixels_out, uint8_t const *pixels_in,
//                            unsigned n_pixels);
