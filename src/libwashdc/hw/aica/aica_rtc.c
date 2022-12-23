/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020, 2022 snickerbockers
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
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#include "dreamcast.h"
#include "washdc/MemoryMap.h"
#include "dc_sched.h"
#include "log.h"
#include "washdc/hostfile.h"
#include "compiler_bullshit.h"
#include "mem_areas.h"

#include "aica_rtc.h"

#define AICA_RTC_TRACE(msg, ...) LOG_DBG("AICA_RTC: "msg, ##__VA_ARGS__)

#define RTC_DEFAULT 0

static void aica_rtc_event_handler(SchedEvent *ev);

static void sched_aica_rtc_event(struct aica_rtc *rtc);
static void cancel_aica_rtc_event(struct aica_rtc *rtc);

#define AICA_RTC_ADDR_MASK ADDR_AREA0_MASK

#define AICA_RTC_ADDR_HIGH   0x710000
#define AICA_RTC_ADDR_LOW    0x710004
#define AICA_RTC_ADDR_ENABLE 0x710008

void aica_rtc_init(struct aica_rtc *rtc, struct dc_clock *clock,
                   char const *path) {
    memset(rtc, 0, sizeof(*rtc));

    if (path) {
        strncpy(rtc->aica_rtc_path, path, sizeof(rtc->aica_rtc_path));
        rtc->aica_rtc_path[AICA_RTC_FILE_MAXPATH - 1] = '\0';
    }

    bool have_clock = false;

    if (strlen(rtc->aica_rtc_path)) {
        LOG_INFO("Attempting to open existing real-time clock state at \"%s\"\n",
                 rtc->aica_rtc_path);
        washdc_hostfile rtc_file =
            washdc_hostfile_open(rtc->aica_rtc_path,
                                 WASHDC_HOSTFILE_READ | WASHDC_HOSTFILE_TEXT);
        if (rtc_file != WASHDC_HOSTFILE_INVALID) {
            char rtc_str[16] = { 0 };
            int n_chars = 0;
            int ch;
            while (n_chars < 15 &&
                   (ch = washdc_hostfile_getc(rtc_file)) != EOF && !isspace(ch))
                rtc_str[n_chars++] = ch;
            rtc_str[15] = '\0';
            if (n_chars > 0 && n_chars < 15) {
                int idx;
                bool alldigits = true;
                for (idx = 0; idx < n_chars; idx++)
                    if (!isdigit(rtc_str[idx]))
                        alldigits = false;
                if (alldigits) {
                    rtc->cur_rtc_val = atoi(rtc_str);
                    have_clock = true;
                }
            }
            washdc_hostfile_close(rtc_file);
        }
    }
    if (!have_clock) {
        LOG_INFO("Unable to access real-time clock state; state will be "
                 "initialized to 0.\n");
        rtc->cur_rtc_val = RTC_DEFAULT;
    }

    rtc->aica_rtc_clk = clock;

    sched_aica_rtc_event(rtc);
}

void aica_rtc_cleanup(struct aica_rtc *rtc) {
    if (strlen(rtc->aica_rtc_path)) {
        LOG_INFO("Attempting to save real-time clock state to \"%s\"\n",
                 rtc->aica_rtc_path);
        LOG_INFO("For the record, the final RTC value is %u\n",
                 (unsigned)rtc->cur_rtc_val);

        washdc_hostfile rtc_file =
            washdc_hostfile_open(rtc->aica_rtc_path,
                                 WASHDC_HOSTFILE_WRITE | WASHDC_HOSTFILE_TEXT);
        if (rtc_file != WASHDC_HOSTFILE_INVALID) {
            washdc_hostfile_printf(rtc_file, "%u\n", rtc->cur_rtc_val);
            washdc_hostfile_close(rtc_file);
        } else {
            LOG_INFO("Unable to save real-time clockstate\n");
        }
    }
}

float aica_rtc_read_float(addr32_t addr, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    uint32_t tmp = aica_rtc_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void aica_rtc_write_float(addr32_t addr, float val, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    aica_rtc_write_32(addr, tmp, ctxt);
}

double aica_rtc_read_double(addr32_t addr, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_rtc_write_double(addr32_t addr, double val, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t aica_rtc_read_32(addr32_t addr, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    struct aica_rtc *rtc = (struct aica_rtc*)ctxt;

    AICA_RTC_TRACE("Reading 4 bytes from AICA RTC address 0x%08x\n",
                   (unsigned)addr);

    uint32_t tmp;
    switch (addr) {
    case AICA_RTC_ADDR_HIGH:
        tmp = rtc->cur_rtc_val >> 16;
        AICA_RTC_TRACE("reading %04x from the upper 16-bits\n", (unsigned)tmp);
        break;
    case AICA_RTC_ADDR_LOW:
        tmp = rtc->cur_rtc_val & 0xffff;
        AICA_RTC_TRACE("reading %04x from the lower 16-bits\n", (unsigned)tmp);
        break;
    case AICA_RTC_ADDR_ENABLE:
        tmp = rtc->write_enable;
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

    return tmp;
}

void aica_rtc_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    struct aica_rtc *rtc = (struct aica_rtc*)ctxt;

    AICA_RTC_TRACE("Writing 4 bytes to address 0x%08x\n",
                   (unsigned)addr);

    WASHDC_UNUSED uint32_t old_rtc_val = rtc->cur_rtc_val;

    switch (addr) {
    case AICA_RTC_ADDR_HIGH:
        if (!rtc->write_enable) {
            AICA_RTC_TRACE("failed to write to AICA_RTC_ADDR_HIGH because the "
                           "enable bit is not set\n");
            break;
        }

        rtc->cur_rtc_val = (val << 16) | (rtc->cur_rtc_val & 0xffff);
        AICA_RTC_TRACE("write to AICA_RTC_ADDR_HIGH - time changed from 0x%08x "
                       "seconds to 0x%08x seconds\n",
                       old_rtc_val, rtc->cur_rtc_val);
        break;
    case AICA_RTC_ADDR_LOW:
        if (!rtc->write_enable) {
            AICA_RTC_TRACE("failed to write to AICA_RTC_ADDR_LOW because the "
                           "enable bit is not set\n");
            break;
        }

        rtc->cur_rtc_val = (val & 0xffff) | (rtc->cur_rtc_val & ~0xffff);
        AICA_RTC_TRACE("write to AICA_RTC_ADDR_LOW - time changed from 0x%08x "
                       "seconds to 0x%08x seconds\n",
                       old_rtc_val, rtc->cur_rtc_val);

        // reset the countdown to the next tick
        cancel_aica_rtc_event(rtc);
        sched_aica_rtc_event(rtc);
        break;
    case AICA_RTC_ADDR_ENABLE:
        rtc->write_enable = (bool)(val & 1);
        if (rtc->write_enable)
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
    }
}

uint16_t aica_rtc_read_16(addr32_t addr, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_rtc_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t aica_rtc_read_8(addr32_t addr, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_rtc_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    addr &= AICA_RTC_ADDR_MASK;
    error_set_feature("Whatever happens when you use an inapproriate "
                      "length while reading from an aica RTC register");
    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_rtc_event_handler(SchedEvent *ev) {
    struct aica_rtc *rtc = (struct aica_rtc*)ev->arg_ptr;

    rtc->cur_rtc_val++;

    AICA_RTC_TRACE("***BEEEEP*** the time is now 0x%08x seconds\n",
                   rtc->cur_rtc_val);

    sched_aica_rtc_event(rtc);
}

static void sched_aica_rtc_event(struct aica_rtc *rtc) {
    rtc->aica_rtc_event.when =
        clock_cycle_stamp(rtc->aica_rtc_clk) + SCHED_FREQUENCY;
    rtc->aica_rtc_event.handler = aica_rtc_event_handler;
    rtc->aica_rtc_event.arg_ptr = rtc;
    sched_event(rtc->aica_rtc_clk, &rtc->aica_rtc_event);
}

static void cancel_aica_rtc_event(struct aica_rtc *rtc) {
    cancel_event(rtc->aica_rtc_clk, &rtc->aica_rtc_event);
}

struct memory_interface aica_rtc_intf = {
    .read32 = aica_rtc_read_32,
    .read16 = aica_rtc_read_16,
    .read8 = aica_rtc_read_8,
    .readfloat = aica_rtc_read_float,
    .readdouble = aica_rtc_read_double,

    .write32 = aica_rtc_write_32,
    .write16 = aica_rtc_write_16,
    .write8 = aica_rtc_write_8,
    .writefloat = aica_rtc_write_float,
    .writedouble = aica_rtc_write_double
};
