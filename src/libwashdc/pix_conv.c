/*******************************************************************************
 *
 * Copyright 2018 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stdint.h>

#include "washdc/pix_conv.h"

// pix_conv.c: The future home of all texture and pixel conversion functions

/*
 * converts a given YUV value pair to two 24-bit RGB pixels
 * The source for these values is the wikipedia article on YUV:
 * https://en.wikipedia.org/wiki/YUV#Y′UV444_to_RGB888_conversion
 */
static void
washdc_yuv_to_rgba_2pixels(uint8_t *rgba_out, int lum1, int lum2,
                          int chrom_b, int chrom_r) {
    int adds[3] = {
        (0x166e8 * chrom_r) >> 16,
        (0x5810 * chrom_b + 0xb6c8 * chrom_r) >> 16,
        (0x1c5a0 * chrom_b) >> 16
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
