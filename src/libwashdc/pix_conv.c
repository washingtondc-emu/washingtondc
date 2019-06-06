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
 * converts a given YUV value to 24-bit RGB
 * The source for these values is the wikipedia article on YUV:
 * https://en.wikipedia.org/wiki/YUV#Y′UV444_to_RGB888_conversion
 */
void washdc_yuv_to_rgb(uint8_t *rgb_out, unsigned lum,
                       unsigned chrom_b, unsigned chrom_r) {
    double yuv[3] = {
        lum, chrom_b, chrom_r
    };
    double rgb[3] = {
        yuv[0] + 1.402 *  (yuv[2] - 128),
        yuv[0] - 0.344 * (yuv[1] - 128) - 0.714 * (yuv[2] - 128),
        yuv[0] + 1.772 * (yuv[1] - 128)
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

    rgb_out[0] = (uint8_t)rgb[0];
    rgb_out[1] = (uint8_t)rgb[1];
    rgb_out[2] = (uint8_t)rgb[2];
}

void washdc_conv_yuv422_rgb888(void *rgb_out, void const* yuv_in,
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

            washdc_yuv_to_rgb(outp, lum[0], chrom_b, chrom_r);
            washdc_yuv_to_rgb(outp + 3, lum[1], chrom_b, chrom_r);
        }
    }
}
