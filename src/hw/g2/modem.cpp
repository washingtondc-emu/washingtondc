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

#include <cstring>

#include "BaseException.hpp"
#include "modem.hpp"

/* pull down to 0 - this device is not implemented */

int modem_read(void *buf, size_t addr, size_t len) {
    memset(buf, 0, len);

    return 0;
}

int modem_write(void const *buf, size_t addr, size_t len) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("sending data to the modem unit"));
}
