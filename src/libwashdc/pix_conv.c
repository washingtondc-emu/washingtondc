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

#include "washdc/pix_conv.h"

// pix_conv.c: The future home of all texture and pixel conversion functions

/*
 * converts a given YUV value pair to two 24-bit RGB pixels
 * The source for these values is the wikipedia article on YUV:
 * https://en.wikipedia.org/wiki/YUV#Y′UV444_to_RGB888_conversion
 */
static void
washdc_yuv_to_rgb_2pixels(uint8_t *rgb_out, unsigned lum1, unsigned lum2,
                          int chrom_b, int chrom_r) {
    int adds[3] = {
        (0x166e8 * chrom_r) >> 16,
        (0x5810 * chrom_b + 0xb6c8 * chrom_r) >> 16,
        (0x1c5a0 * chrom_b) >> 16
    };
    int rgb[6] = {
        lum1 + adds[0],
        lum1 + adds[1],
        lum1 + adds[2],
        lum2 + adds[0],
        lum2 + adds[1],
        lum2 + adds[2]
    };

    if (rgb[0] < 0)
        rgb[0] = 0;
    else if (rgb[0] > 255)
        rgb[0] = 255;
    if (rgb[1] < 0)
        rgb[1] = 0;
    else if (rgb[1] > 255)
        rgb[1] = 255;
    if (rgb[2] < 0)
        rgb[2] = 0;
    else if (rgb[2] > 255)
        rgb[2] = 255;
    if (rgb[3] < 0)
        rgb[3] = 0;
    else if (rgb[3] > 255)
        rgb[3] = 255;
    if (rgb[4] < 0)
        rgb[4] = 0;
    else if (rgb[4] > 255)
        rgb[4] = 255;
    if (rgb[5] < 0)
        rgb[5] = 0;
    else if (rgb[5] > 255)
        rgb[5] = 255;

    rgb_out[0] = (uint8_t)rgb[0];
    rgb_out[1] = (uint8_t)rgb[1];
    rgb_out[2] = (uint8_t)rgb[2];
    rgb_out[3] = (uint8_t)rgb[3];
    rgb_out[4] = (uint8_t)rgb[4];
    rgb_out[5] = (uint8_t)rgb[5];
}

void washdc_conv_yuv422_rgb888(void *rgb_out, void const* yuv_in,
                               unsigned width, unsigned height) {
    uint8_t *rgbp = (uint8_t*)rgb_out;
    uint32_t const *tex_in = (uint32_t const *)yuv_in;
    unsigned half_width = width / 2;

    unsigned col, row;
    for (col = 0; col < half_width; col++) {
        for (row = 0; row < height; row++) {
            uint8_t *outp = 3 * (row * width + col * 2) + rgbp;
            uint32_t in = tex_in[row * half_width + col];
            unsigned lum[2] = { (in >> 8) & 0xff, (in >> 24) & 0xff };
            int chrom_b = in & 0xff;
            int chrom_r = (in >> 16) & 0xff;

            washdc_yuv_to_rgb_2pixels(outp, lum[0], lum[1],
                                      chrom_b - 128, chrom_r - 128);
        }
    }
}
