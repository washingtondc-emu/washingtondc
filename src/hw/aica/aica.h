/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#ifndef AICA_H_
#define AICA_H_

#include "aica_dsp.h"
#include "aica_channel.h"
#include "aica_wave_mem.h"
#include "aica_common.h"

struct arm7;

struct aica {
    struct aica_channel channel;
    struct aica_dsp dsp;
    struct aica_wave_mem mem;
    struct aica_common common;
};

void aica_init(struct aica *aica, struct arm7 *arm7);
void aica_cleanup(struct aica *aica);

#endif
