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
#include <stdarg.h>
#include <stdbool.h>

#include "dreamcast.h"
#include "MemoryMap.h"
#include "dc_sched.h"

#include "aica_rtc.h"

#define AICA_RTC_TRACE(msg, ...) aica_rtc_do_trace(msg, ##__VA_ARGS__)

// don't call this directly, use the AICA_RTC_TRACE macro instead
void aica_rtc_do_trace(char const *msg, ...);

#define RTC_DEFAULT 0
static uint32_t cur_rtc_val = RTC_DEFAULT;

static struct SchedEvent aica_rtc_event;
static void aica_rtc_event_handler(SchedEvent *ev);

static void sched_aica_rtc_event(void);
static void cancel_aica_rtc_event(void);

#define AICA_RTC_ADDR_HIGH   0x710000
#define AICA_RTC_ADDR_LOW    0x710004
#define AICA_RTC_ADDR_ENABLE 0x710008

static bool write_enable;

void aica_rtc_init(void) {
    sched_aica_rtc_event();
}

int aica_rtc_read(void *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;

    AICA_RTC_TRACE("Reading %u bytes from AICA RTC address 0x%08x\n",
                   (unsigned)len, (unsigned)addr);

    if (len != 4) {
        error_set_feature("Whatever happens when you use an inapproriate "
                          "length while reading from an aica RTC register");
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    uint32_t tmp;
    switch (addr) {
    case AICA_RTC_ADDR_HIGH:
        tmp = cur_rtc_val >> 16;
        AICA_RTC_TRACE("reading %04x from the upper 16-bits\n", (unsigned)tmp);
        break;
    case AICA_RTC_ADDR_LOW:
        tmp = cur_rtc_val & 0xffff;
        AICA_RTC_TRACE("reading %04x from the lower 16-bits\n", (unsigned)tmp);
        break;
    case AICA_RTC_ADDR_ENABLE:
        tmp = write_enable;
        AICA_RTC_TRACE("reading the enable bit (%u)\n", (unsigned)tmp);
        break;
    default:
        /*
         * this should not even be possible because there are only three
         * registers in the AICA RTC's address range.
         */
        RAISE_ERROR(ERROR_INTEGRITY);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, &tmp, sizeof(tmp));
    return MEM_ACCESS_SUCCESS;
}

int aica_rtc_write(void const *buf, size_t addr, size_t len) {
    AICA_RTC_TRACE("Writing %u bytes to address 0x%08x\n",
                   (unsigned)len, (unsigned)addr);

    if (len != 4) {
        error_set_feature("Whatever happens when you use an inapproriate "
                          "length while reading from an aica RTC register");
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    uint32_t val;
    memcpy(&val, buf, sizeof(val));

    uint32_t old_rtc_val = cur_rtc_val;

    switch (addr) {
    case AICA_RTC_ADDR_HIGH:
        if (!write_enable) {
            AICA_RTC_TRACE("failed to write to AICA_RTC_ADDR_HIGH because the "
                           "enable bit is not set\n");
            break;
        }

        cur_rtc_val = (val << 16) | (cur_rtc_val & 0xffff);
        AICA_RTC_TRACE("write to AICA_RTC_ADDR_HIGH - time changed from 0x%08x "
                   "seconds to 0x%08x seconds\n", old_rtc_val, cur_rtc_val);
        break;
    case AICA_RTC_ADDR_LOW:
        if (!write_enable) {
            AICA_RTC_TRACE("failed to write to AICA_RTC_ADDR_LOW because the "
                           "enable bit is not set\n");
            break;
        }

        cur_rtc_val = (val & 0xffff) | (cur_rtc_val & ~0xffff);
        AICA_RTC_TRACE("write to AICA_RTC_ADDR_LOW - time changed from 0x%08x "
                   "seconds to 0x%08x seconds\n", old_rtc_val, cur_rtc_val);

        // reset the countdown to the next tick
        cancel_aica_rtc_event();
        sched_aica_rtc_event();
        break;
    case AICA_RTC_ADDR_ENABLE:
        write_enable = (bool)(val & 1);
        if (write_enable)
            AICA_RTC_TRACE("write enable set!\n");
        else
            AICA_RTC_TRACE("write enable cleared\n");
        break;
    default:
        /*
         * this should not even be possible because there are only three
         * registers in the AICA RTC's address range.
         */
        RAISE_ERROR(ERROR_INTEGRITY);
        return MEM_ACCESS_FAILURE;
    }

    return MEM_ACCESS_SUCCESS;
}

static void aica_rtc_event_handler(SchedEvent *ev) {
    cur_rtc_val++;

    AICA_RTC_TRACE("***BEEEEP*** the time is now 0x%08x seconds\n",
                   cur_rtc_val);

    sched_aica_rtc_event();
}

static void sched_aica_rtc_event(void) {
    aica_rtc_event.when = dc_cycle_stamp() + SCHED_FREQUENCY;
    aica_rtc_event.handler = aica_rtc_event_handler;
    sched_event(&aica_rtc_event);
}

static void cancel_aica_rtc_event(void) {
    cancel_event(&aica_rtc_event);
}

void aica_rtc_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    printf("AICA_RTC: ");

    vprintf(msg, var_args);

    va_end(var_args);
}
