/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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
#include <stdio.h>

#include <stdint.h>

#include "g1_reg.h"

#include "mem_code.h"
#include "washdc/error.h"
#include "washdc/types.h"
#include "mem_areas.h"
#include "log.h"

DEF_MMIO_REGION(g1_reg_32, N_G1_REGS, ADDR_G1_FIRST, uint32_t)
DEF_MMIO_REGION(g1_reg_16, N_G1_REGS, ADDR_G1_FIRST, uint16_t)

static struct mmio_region_g1_reg_32 mmio_region_g1_reg_32;
static struct mmio_region_g1_reg_16 mmio_region_g1_reg_16;

static uint8_t reg_backing[N_G1_REGS];

uint8_t g1_reg_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t g1_reg_read_16(addr32_t addr, void *ctxt) {
    return mmio_region_g1_reg_16_read(&mmio_region_g1_reg_16, addr);
}

void g1_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    mmio_region_g1_reg_16_write(&mmio_region_g1_reg_16, addr, val);
}

uint32_t g1_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_g1_reg_32_read(&mmio_region_g1_reg_32, addr);
}

void g1_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_g1_reg_32_write(&mmio_region_g1_reg_32, addr, val);
}

float g1_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_g1_reg_32_read(&mmio_region_g1_reg_32, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void g1_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_g1_reg_32_write(&mmio_region_g1_reg_32, addr, tmp);
}

double g1_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void g1_reg_init(void) {
    init_mmio_region_g1_reg_32(&mmio_region_g1_reg_32, (void*)reg_backing);
    init_mmio_region_g1_reg_16(&mmio_region_g1_reg_16, (void*)reg_backing);

    /* system boot-rom registers */
    // XXX this is supposed to be write-only, but currently it's readable
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RRC", 0x005f7480,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_16_init_cell(&mmio_region_g1_reg_16,
                                    "SB_G1RWC", 0x5f7484,
                                    mmio_region_g1_reg_16_warn_read_handler,
                                    mmio_region_g1_reg_16_warn_write_handler,
                                    NULL);

    /* flash rom registers */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FRC", 0x5f7488,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1FWC", 0x5f748c,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);

    /* GD PIO timing registers - I guess this is related to GD-ROM ? */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRC", 0x5f7490,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CWC", 0x5f7494,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);

    // TODO: SB_G1SYSM should be read-only
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1SYSM", 0x5f74b0,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1CRDYC", 0x5f74b4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);

    /*
     * on real hardware, programs have to write 0x1fffffff to this as part
     * of the GD-ROM unlock ritual.  Otherwise, there are a bunch of registers
     * that will refuse to cooperate and only ever return all 1s.
     *
     * After that, it sends the 0x1f71 packet command to start the disk.  If it
     * doesn't do this, ISTEXT will never show any activity.
     */
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32,
                                    "SB_G1_RESET", 0x005f74e4,
                                    mmio_region_g1_reg_32_warn_read_handler,
                                    mmio_region_g1_reg_32_warn_write_handler,
                                    NULL);
}

void g1_reg_cleanup(void) {
    cleanup_mmio_region_g1_reg_32(&mmio_region_g1_reg_32);
    cleanup_mmio_region_g1_reg_16(&mmio_region_g1_reg_16);
}

struct memory_interface g1_intf = {
    .read32 = g1_reg_read_32,
    .read16 = g1_reg_read_16,
    .read8 = g1_reg_read_8,
    .readfloat = g1_reg_read_float,
    .readdouble = g1_reg_read_double,

    .write32 = g1_reg_write_32,
    .write16 = g1_reg_write_16,
    .write8 = g1_reg_write_8,
    .writefloat = g1_reg_write_float,
    .writedouble = g1_reg_write_double
};

void g1_mmio_cell_init_32(char const *name, uint32_t addr,
                          mmio_region_g1_reg_32_read_handler on_read,
                          mmio_region_g1_reg_32_write_handler on_write,
                          void *ctxt) {
    mmio_region_g1_reg_32_init_cell(&mmio_region_g1_reg_32, name, addr,
                                    on_read, on_write, ctxt);
}
