/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef AICA_RTC_H_
#define AICA_RTC_H_

#include <stddef.h>
#include <stdint.h>

#include "dc_sched.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"

struct aica_rtc {
    struct dc_clock *aica_rtc_clk;
    struct SchedEvent aica_rtc_event;
    uint32_t cur_rtc_val;
    bool write_enable;
};

/*
 * The AICA's RTC is ironically not available to AICA, so this clock should
 * point to the SH4's clock, not the ARM7's clock.
 */
void aica_rtc_init(struct aica_rtc *rtc, struct dc_clock *clock);
void aica_rtc_cleanup(struct aica_rtc *rtc);

float aica_rtc_read_float(addr32_t addr, void *ctxt);
void aica_rtc_write_float(addr32_t addr, float val, void *ctxt);
double aica_rtc_read_double(addr32_t addr, void *ctxt);
void aica_rtc_write_double(addr32_t addr, double val, void *ctxt);
uint32_t aica_rtc_read_32(addr32_t addr, void *ctxt);
void aica_rtc_write_32(addr32_t addr, uint32_t val, void *ctxt);
uint16_t aica_rtc_read_16(addr32_t addr, void *ctxt);
void aica_rtc_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint8_t aica_rtc_read_8(addr32_t addr, void *ctxt);
void aica_rtc_write_8(addr32_t addr, uint8_t val, void *ctxt);

extern struct memory_interface aica_rtc_intf;

#endif
