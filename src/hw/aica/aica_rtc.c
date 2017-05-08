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

#include "MemoryMap.h"

#include "aica_rtc.h"

/*
 * HACK
 * We hardcode the RTC value for now and never increment it.
 *
 * this should be approximately january 1 2000 at 12:00 AM.  I say it's
 * approximate because I didn't take leap years or leap seconds
 * into account.  The value of 631152000 comes from KallistiOS and represents
 * the offset between the Dreamcast epoch and the Unix epoch.
 *
 * The Dreamcast epoch is January 1, 1950 at 12:00 AM for some reason.
 * This means that the Sega Dreamcast will experience it's own version of
 * Unix's infamous 2038 problem in 2018.  It's a good thing the RTC battery
 * isn't reliable enough to last that long.
 */
static const uint32_t RTC_DEFAULT = 631152000 + 365 * 24 * 60 * 60 * 30;

static uint8_t aica_rtc[ADDR_AICA_RTC_LAST - ADDR_AICA_RTC_FIRST + 1];

int aica_rtc_read(void *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);

    // HACK
    uint32_t tmp;
    if (addr == 0x710000)
        tmp = RTC_DEFAULT >> 16;
    else if (addr == 0x710004)
        tmp = RTC_DEFAULT & 0xffff;
    else
        tmp = 0;

    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(buf, &tmp, len);
        return MEM_ACCESS_SUCCESS;
    }

    printf("Reading %u bytes from AICA RTC address 0x%08x\n",
           (unsigned)len, (unsigned)addr);

    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int aica_rtc_write(void const *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);
    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(aica_rtc + first_byte, buf, len);
        return MEM_ACCESS_SUCCESS;
    }

    printf("Writing %u bytes to AICA RTC address 0x%08x\n",
           (unsigned)len, (unsigned)addr);

    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}
