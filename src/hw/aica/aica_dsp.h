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

#ifndef AICA_DSP_H_
#define AICA_DSP_H_

#include <stdint.h>

#include "mem_areas.h"

#define AICA_DSP_LEN (ADDR_AICA_DSP_LAST - ADDR_AICA_DSP_FIRST + 1)

struct aica_dsp {
    // treating this as a simple ram device for now
    uint8_t backing[AICA_DSP_LEN];
};

void aica_dsp_init(struct aica_dsp *data);
void aica_dsp_cleanup(struct aica_dsp *data);

extern struct memory_interface aica_dsp_intf;

#endif
