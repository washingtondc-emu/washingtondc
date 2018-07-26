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

#include "pix_conv.h"

// pix_conv.c: The future home of all texture and pixel conversion functions

// converts a given YUV value to 24-bit RGB
void yuv_to_rgb(uint8_t *rgb_out, unsigned lum,
                unsigned chrom_b, unsigned chrom_r) {
    double yuv[3] = {
        lum / 255.0, chrom_b / 255.0, chrom_r / 255.0
    };
    double rgb[3] = {
        yuv[0] + (11.0 / 8.0) * yuv[2] - 0.5,
        yuv[0] - 0.25 * (11.0 / 8.0) * (yuv[1] - 0.5) -
        0.5 * (11.0 / 8.0) * (yuv[2] - 0.5),
        yuv[0] + 1.25 * (11.0 / 8.0) * (yuv[1] - 0.5)
    };

    unsigned red = rgb[0] * 255.0;
    unsigned green = rgb[1] * 255.0;
    unsigned blue = rgb[2] * 255.0;
    if (red > 255)
        red = 255;
    if (green > 255)
        green = 255;
    if (blue > 255)
        blue = 255;

    rgb_out[0] = (uint8_t)red;
    rgb_out[1] = (uint8_t)green;
    rgb_out[2] = (uint8_t)blue;
}

void conv_yuv422_rgb888(void *rgb_out, void const* yuv_in,
                        unsigned width, unsigned height) {
    uint8_t *rgbp = (uint8_t*)rgb_out;
    uint32_t const *tex_in = (uint32_t const *)yuv_in;

    unsigned col, row;
    for (col = 0; col < (width / 2); col++) {
        for (row = 0; row < height; row++) {
            uint8_t *outp = 3 * (row * width + col * 2) + rgbp;
            uint32_t in = tex_in[row * (width / 2) + col];
            unsigned lum[2] = { (in >> 8) & 0xff, (in >> 24) & 0xff };
            unsigned chrom_b = in & 0xff;
            unsigned chrom_r = (in >> 16) & 0xff;

            yuv_to_rgb(outp, lum[0], chrom_b, chrom_r);
            yuv_to_rgb(outp + 3, lum[1], chrom_b, chrom_r);
        }
    }
}
