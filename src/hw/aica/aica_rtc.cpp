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

#include "MemoryMap.hpp"

#include "aica_rtc.hpp"

static uint8_t aica_rtc[ADDR_AICA_RTC_LAST - ADDR_AICA_RTC_FIRST + 1];

int aica_rtc_read(void *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);
    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(buf, aica_rtc + first_byte, len);
        return 0;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("Whatever happens when "
                                          "you use an inapproriate "
                                          "length while reading "
                                          "from an aica RTC "
                                          "register") <<
                          errinfo_guest_addr(addr) <<
                          errinfo_length(len));
}

int aica_rtc_write(void const *buf, size_t addr, size_t len) {
    addr32_t first_byte = addr;
    addr32_t last_byte = addr + (len - 1);
    if (first_byte >= ADDR_AICA_RTC_FIRST && first_byte <= ADDR_AICA_RTC_LAST &&
        last_byte >= ADDR_AICA_RTC_FIRST && last_byte <= ADDR_AICA_RTC_LAST) {
        memcpy(aica_rtc + first_byte, buf, len);
        return 0;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("Whatever happens when "
                                          "you use an inapproriate "
                                          "length while writing "
                                          "to an aica RTC "
                                          "register") <<
                          errinfo_guest_addr(addr) <<
                          errinfo_length(len));
}
