/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#ifndef AREA4_H_
#define AREA4_H_

#include "washdc/hostfile.h"
#include "washdc/MemoryMap.h"

struct pvr2;

struct area4 {
    struct pvr2 *pvr2;
    struct memory_interface const *ta_fifo_intf, *ta_yuv_intf;
};

void area4_init(struct area4 *area4, struct pvr2 *pvr2,
                washdc_hostfile pvr2_trace_file);
void area4_cleanup(struct area4 *area4);

extern struct memory_interface area4_intf;

#endif
