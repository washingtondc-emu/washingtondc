/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#ifndef WASHDC_SOUND_INTF_H_
#define WASHDC_SOUND_INTF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t washdc_sample_type;

struct washdc_sound_intf {
    void (*init)(void);
    void (*cleanup)(void);
    void (*submit_samples)(washdc_sample_type *samples, unsigned count);
};

#ifdef __cplusplus
}
#endif

#endif
