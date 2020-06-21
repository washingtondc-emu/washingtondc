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

#include <string.h>

#include "log.h"
#include "washdc/error.h"
#include "modem.h"
#include "mem_code.h"

float modem_read_float(addr32_t addr, void *ctxt) {
    return 0.0f;
}

void modem_write_float(addr32_t addr, float val, void *ctxt) {
    LOG_DBG("%s - Writing %f to the modem\n", __func__, (double)val);
}

double modem_read_double(addr32_t addr, void *ctxt) {
    return 0.0;
}

void modem_write_double(addr32_t addr, double val, void *ctxt) {
    LOG_DBG("%s - Writing %f to the modem\n", __func__, val);
}

uint8_t modem_read_8(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%02x to the modem\n", __func__, (unsigned)val);
}

uint16_t modem_read_16(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%04x to the modem\n", __func__, (unsigned)val);
}

uint32_t modem_read_32(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%08x to the modem\n", __func__, (unsigned)val);
}

struct memory_interface modem_intf = {
    .read32 = modem_read_32,
    .read16 = modem_read_16,
    .read8 = modem_read_8,
    .readfloat = modem_read_float,
    .readdouble = modem_read_double,

    .write32 = modem_write_32,
    .write16 = modem_write_16,
    .write8 = modem_write_8,
    .writefloat = modem_write_float,
    .writedouble = modem_write_double
};
