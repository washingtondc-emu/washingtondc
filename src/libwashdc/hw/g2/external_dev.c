/*******************************************************************************
 *
 * Copyright 2018 snickerbockers
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

#include <stdint.h>

#include "external_dev.h"

static uint8_t ext_dev_read_8(uint32_t addr, void *ctxt);
static void ext_dev_write_8(uint32_t addr, uint8_t val, void *ctxt);
static uint16_t ext_dev_read_16(uint32_t addr, void *ctxt);
static void ext_dev_write_16(uint32_t addr, uint16_t val, void *ctxt);
static uint32_t ext_dev_read_32(uint32_t addr, void *ctxt);
static void ext_dev_write_32(uint32_t addr, uint32_t val, void *ctxt);
static float ext_dev_read_float(uint32_t addr, void *ctxt);
static void ext_dev_write_float(uint32_t addr, float val, void *ctxt);
static double ext_dev_read_double(uint32_t addr, void *ctxt);
static void ext_dev_write_double(uint32_t addr, double val, void *ctxt);

static uint8_t ext_dev_read_8(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_8(uint32_t addr, uint8_t val, void *ctxt) {
}

static uint16_t ext_dev_read_16(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_16(uint32_t addr, uint16_t val, void *ctxt) {
}

static uint32_t ext_dev_read_32(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_32(uint32_t addr, uint32_t val, void *ctxt) {
}

static float ext_dev_read_float(uint32_t addr, void *ctxt) {
    return 0.0f;
}

static void ext_dev_write_float(uint32_t addr, float val, void *ctxt) {
}

static double ext_dev_read_double(uint32_t addr, void *ctxt) {
    return 0.0;
}

static void ext_dev_write_double(uint32_t addr, double val, void *ctxt) {
}

struct memory_interface ext_dev_intf = {
    .read8 = ext_dev_read_8,
    .read16 = ext_dev_read_16,
    .read32 = ext_dev_read_32,
    .readfloat = ext_dev_read_float,
    .readdouble = ext_dev_read_double,

    .write8 = ext_dev_write_8,
    .write16 = ext_dev_write_16,
    .write32 = ext_dev_write_32,
    .writefloat = ext_dev_write_float,
    .writedouble = ext_dev_write_double
};
