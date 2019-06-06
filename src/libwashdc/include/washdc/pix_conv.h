/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef WASHDC_PIX_CONV_H_
#define WASHDC_PIX_CONV_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// converts a given YUV value to 24-bit RGB
void washdc_yuv_to_rgb(uint8_t *rgb_out, unsigned lum,
                       unsigned chrom_b, unsigned chrom_r);

void washdc_conv_yuv422_rgb888(void *rgb_out, void const* yuv_in,
                               unsigned width, unsigned height);

#ifdef __cplusplus
}
#endif

#endif
