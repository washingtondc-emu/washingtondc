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

/*
 * SH-4 Instruction cache.
 * Currently this is not emulated, and it probably never will be.
 *
 * The instruction cache address array allows programs to query what's in the
 * cache and selectively invalidate certain lines, so we do have to at least
 * implement some bare skeleton functionality for that.
 *
 * As far as I know, Virtua Fighter 3tb is the only game that uses this.  At
 * boot-time, it invalidates  all 512 lines individually using this
 * functionality.  Doing this with the CCR register would have been better, but
 * that's not how VF3tb rolls.
 */

#include "sh4_icache.h"
#include "jit/code_cache.h"
#include "config.h"
#include "log.h"

#define SH4_ICACHE_READ_ADDR_ARRAY_TMPL(type, postfix)                  \
    type sh4_icache_read_addr_array_##postfix(Sh4 *sh4,                 \
                                              addr32_t paddr) {         \
        /* return 0 because I'm not implementing the icache. */         \
        /* Ideally this would return data corresponding to a cache */   \
        /* entry in the i-cache. */                                     \
        return (type)0;                                                 \
    }

SH4_ICACHE_READ_ADDR_ARRAY_TMPL(float, float)
SH4_ICACHE_READ_ADDR_ARRAY_TMPL(double, double)
SH4_ICACHE_READ_ADDR_ARRAY_TMPL(uint32_t, 32)
SH4_ICACHE_READ_ADDR_ARRAY_TMPL(uint16_t, 16)
SH4_ICACHE_READ_ADDR_ARRAY_TMPL(uint8_t, 8)

#define SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(type, postfix)                 \
    void sh4_icache_write_addr_array_##postfix(Sh4 *sh4,                \
                                               addr32_t paddr,          \
                                               type val) {              \
        LOG_INFO("Write %08x (%u bytes) to ic address array addr "      \
                 "0x%08x\n", (unsigned)val, (unsigned)sizeof(val),      \
                 (unsigned)paddr);                                      \
                                                                        \
        /* according to SH4 hardware manual, programs can write to */   \
        /* the IC address array to invalidate specific cache */         \
        /* entries. */                                                  \
        /* TODO: only invalidate the parts that need to be */           \
        /* invalidated instead of the entire cache.  Also, check the */ \
        /* v-bit in the value being written.  I think the invalidate */ \
        /* is only if the v-bit being written is zero, but then that */ \
        /* makes me wonder why they even let you specify a non-zero */  \
        /* V bit if that does nothing. */                               \
                                                                        \
        if (config_get_jit())                                           \
            code_cache_invalidate_all();                                \
    }

SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(float, float)
SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(double, double)
SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(uint32_t, 32)
SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(uint16_t, 16)
SH4_ICACHE_WRITE_ADDR_ARRAY_TMPL(uint8_t, 8)
