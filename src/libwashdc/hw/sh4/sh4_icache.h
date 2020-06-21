/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
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

#ifndef SH4_ICACHE_H_
#define SH4_ICACHE_H_

#include "sh4.h"
#include "washdc/types.h"

#define SH4_IC_ADDR_ARRAY_FIRST 0xf0000000
#define SH4_IC_ADDR_ARRAY_LAST  0xf0ffffff

void sh4_icache_write_addr_array_float(Sh4 *sh4, addr32_t paddr, float val);
void sh4_icache_write_addr_array_double(Sh4 *sh4, addr32_t paddr, double val);
void sh4_icache_write_addr_array_32(Sh4 *sh4, addr32_t paddr, uint32_t val);
void sh4_icache_write_addr_array_16(Sh4 *sh4, addr32_t paddr, uint16_t val);
void sh4_icache_write_addr_array_8(Sh4 *sh4, addr32_t paddr, uint8_t val);

float sh4_icache_read_addr_array_float(Sh4 *sh4, addr32_t paddr);
double sh4_icache_read_addr_array_double(Sh4 *sh4, addr32_t paddr);
uint32_t sh4_icache_read_addr_array_32(Sh4 *sh4, addr32_t paddr);
uint16_t sh4_icache_read_addr_array_16(Sh4 *sh4, addr32_t paddr);
uint8_t sh4_icache_read_addr_array_8(Sh4 *sh4, addr32_t paddr);

#endif
