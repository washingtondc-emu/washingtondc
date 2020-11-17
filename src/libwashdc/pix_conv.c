/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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

#include <stdint.h>

#include "washdc/pix_conv.h"

// pix_conv.c: The future home of all texture and pixel conversion functions

static void
washdc_yuv_to_rgba_2pixels(uint8_t *rgba_out, int lum1, int lum2,
                          int chrom_b, int chrom_r) {
    int adds[3] = {
        (0x16000  * chrom_r) >> 16,
        -((0x5800 * chrom_b + 0xb000 * chrom_r) >> 16),
        (0x1b800 * chrom_b) >> 16
    };
    int rgba[8] = {
        lum1 + adds[0],
        lum1 + adds[1],
        lum1 + adds[2],
        255,
        lum2 + adds[0],
        lum2 + adds[1],
        lum2 + adds[2],
        255
    };

    if (rgba[0] < 0)
        rgba[0] = 0;
    else if (rgba[0] > 255)
        rgba[0] = 255;
    if (rgba[1] < 0)
        rgba[1] = 0;
    else if (rgba[1] > 255)
        rgba[1] = 255;
    if (rgba[2] < 0)
        rgba[2] = 0;
    else if (rgba[2] > 255)
        rgba[2] = 255;
    if (rgba[3] < 0)
        rgba[3] = 0;
    else if (rgba[3] > 255)
        rgba[3] = 255;
    if (rgba[4] < 0)
        rgba[4] = 0;
    else if (rgba[4] > 255)
        rgba[4] = 255;
    if (rgba[5] < 0)
        rgba[5] = 0;
    else if (rgba[5] > 255)
        rgba[5] = 255;
    if (rgba[6] < 0)
        rgba[6] = 0;
    else if (rgba[6] > 255)
        rgba[6] = 255;
    if (rgba[7] < 0)
        rgba[7] = 0;
    else if (rgba[7] > 255)
        rgba[7] = 255;

    rgba_out[0] = (uint8_t)rgba[0];
    rgba_out[1] = (uint8_t)rgba[1];
    rgba_out[2] = (uint8_t)rgba[2];
    rgba_out[3] = (uint8_t)rgba[3];
    rgba_out[4] = (uint8_t)rgba[4];
    rgba_out[5] = (uint8_t)rgba[5];
    rgba_out[6] = (uint8_t)rgba[6];
    rgba_out[7] = (uint8_t)rgba[7];
}

void washdc_conv_yuv422_rgba8888(void *rgba_out, void const* yuv_in,
                                 unsigned width, unsigned height) {
    uint8_t *rgbap = (uint8_t*)rgba_out;
    uint32_t const *tex_in = (uint32_t const *)yuv_in;
    unsigned half_width = width / 2;

    unsigned col, row;
    for (row = 0; row < height; row++) {
        for (col = 0; col < half_width; col++) {
            uint32_t in = *tex_in++;
            unsigned lum[2] = { (in >> 8) & 0xff, (in >> 24) & 0xff };
            int chrom_b = in & 0xff;
            int chrom_r = (in >> 16) & 0xff;

            washdc_yuv_to_rgba_2pixels(rgbap, lum[0], lum[1],
                                       chrom_b - 128, chrom_r - 128);
            rgbap += 8;
        }
    }
}
