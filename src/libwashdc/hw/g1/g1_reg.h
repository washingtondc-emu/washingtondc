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

#ifndef G1_REG_H_
#define G1_REG_H_

#include <stddef.h>
#include <stdint.h>

#include "mmio.h"
#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

#define N_G1_REGS (ADDR_G1_LAST - ADDR_G1_FIRST + 1)
DECL_MMIO_REGION(g1_reg_32, N_G1_REGS, ADDR_G1_FIRST, uint32_t)
DECL_MMIO_REGION(g1_reg_16, N_G1_REGS, ADDR_G1_FIRST, uint16_t)

uint8_t g1_reg_read_8(addr32_t addr, void *ctxt);
void g1_reg_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t g1_reg_read_16(addr32_t addr, void *ctxt);
void g1_reg_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t g1_reg_read_32(addr32_t addr, void *ctxt);
void g1_reg_write_32(addr32_t addr, uint32_t val, void *ctxt);
float g1_reg_read_float(addr32_t addr, void *ctxt);
void g1_reg_write_float(addr32_t addr, float val, void *ctxt);
double g1_reg_read_double(addr32_t addr, void *ctxt);
void g1_reg_write_double(addr32_t addr, double val, void *ctxt);

void g1_reg_init(void);
void g1_reg_cleanup(void);

extern struct memory_interface g1_intf;

void g1_mmio_cell_init_32(char const *name, uint32_t addr,
                          mmio_region_g1_reg_32_read_handler on_read,
                          mmio_region_g1_reg_32_write_handler on_write,
                          void *ctxt);

#endif
