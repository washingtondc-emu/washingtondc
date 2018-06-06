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

#include "aica_common.h"
#include "aica_channel.h"

#include "aica.h"

void aica_init(struct aica *aica, struct arm7 *arm7) {
    aica_common_init(&aica->common, arm7);
    aica_dsp_init(&aica->dsp);
    aica_channel_init(&aica->channel);
    aica_wave_mem_init(&aica->mem);
}

void aica_cleanup(struct aica *aica) {
    aica_wave_mem_cleanup(&aica->mem);
    aica_channel_cleanup(&aica->channel);
    aica_dsp_cleanup(&aica->dsp);
    aica_common_cleanup(&aica->common);
}
