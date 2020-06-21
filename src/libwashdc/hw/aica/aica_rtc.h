/*******************************************************************************
 *
 * Copyright 2017-2019 snickerbockers
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

#ifndef AICA_RTC_H_
#define AICA_RTC_H_

#include <stddef.h>
#include <stdint.h>

#include "dc_sched.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"

#define AICA_RTC_FILE_MAXPATH 512

struct aica_rtc {
    struct dc_clock *aica_rtc_clk;
    struct SchedEvent aica_rtc_event;
    uint32_t cur_rtc_val;
    bool write_enable;
    char aica_rtc_path[AICA_RTC_FILE_MAXPATH];
};

/*
 * The AICA's RTC is ironically not available to AICA, so this clock should
 * point to the SH4's clock, not the ARM7's clock.
 */
void aica_rtc_init(struct aica_rtc *rtc, struct dc_clock *clock,
                   char const *path);
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
