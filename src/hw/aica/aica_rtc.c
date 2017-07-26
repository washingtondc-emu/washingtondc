/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include <stdio.h>
#include <stdint.h>

#include "dreamcast.h"
#include "MemoryMap.h"
#include "dc_sched.h"

#include "aica_rtc.h"

#define RTC_DEFAULT 0
static uint32_t cur_rtc_val = RTC_DEFAULT;

static struct SchedEvent aica_rtc_event;
static void aica_rtc_event_handler(SchedEvent *ev);
static void sched_aica_rtc_event(void);

static uint8_t aica_rtc[ADDR_AICA_RTC_LAST - ADDR_AICA_RTC_FIRST + 1];

void aica_rtc_init(void) {
    sched_aica_rtc_event();
}

int aica_rtc_read(void *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);

    printf("Reading %u bytes from AICA RTC address 0x%08x\n",
           (unsigned)len, (unsigned)addr);

    // HACK
    uint32_t tmp;
    if (addr == 0x710000)
        tmp = cur_rtc_val >> 16;
    else if (addr == 0x710004)
        tmp = cur_rtc_val & 0xffff;
    else
        tmp = 0;

    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(buf, &tmp, len);
        return MEM_ACCESS_SUCCESS;
    }

    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int aica_rtc_write(void const *buf, size_t addr, size_t len) {
    printf("Writing %u bytes to AICA RTC address 0x%08x\n",
           (unsigned)len, (unsigned)addr);

    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);
    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(aica_rtc + first_byte, buf, len);
        return MEM_ACCESS_SUCCESS;
    }

    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static void aica_rtc_event_handler(SchedEvent *ev) {
    cur_rtc_val++;

    printf("***BEEEEP*** the time is now %08x\n", cur_rtc_val);

    sched_aica_rtc_event();
}

static void sched_aica_rtc_event(void) {
    aica_rtc_event.when = dc_cycle_stamp() + (200 * 1000 * 1000);
    aica_rtc_event.handler = aica_rtc_event_handler;
    sched_event(&aica_rtc_event);
}
