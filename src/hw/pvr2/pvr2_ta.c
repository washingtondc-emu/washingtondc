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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "pvr2_ta.h"

int pvr2_ta_fifo_poly_read(void *buf, size_t addr, size_t len) {
    fprintf(stderr, "WARNING: trying to read %u bytes from the TA polygon FIFO "
            "(you get all 0s)\n", (unsigned)len);
    memset(buf, 0, len);

    return 0;
}

int pvr2_ta_fifo_poly_write(void const *buf, size_t addr, size_t len) {
    fprintf(stderr, "WARNING: writing %u bytes to TA polygon FIFO:\n",
            (unsigned)len);

    if (len % 4 == 0) {
        uint32_t *ptr = (uint32_t*)buf;
        while (len) {
            fprintf(stderr, "\t%08x\n", (unsigned)*ptr++);
            len -= 4;
        }
    } else {
        uint8_t *ptr = (uint8_t*)buf;
        while (len--)
            fprintf(stderr, "\t%02x\n", (unsigned)*ptr++);
    }

    return 0;
}
