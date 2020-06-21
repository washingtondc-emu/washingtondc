/*******************************************************************************
 *
 * Copyright 2016-2020 snickerbockers
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

#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <string.h>
#include <stdint.h>

#include "washdc/error.h"
#include "washdc/types.h"
#include "mem_code.h"
#include "washdc/MemoryMap.h"

#define MEMORY_SIZE_SHIFT 24
#define MEMORY_SIZE (1 << MEMORY_SIZE_SHIFT)
#define MEMORY_MASK (MEMORY_SIZE - 1)

struct Memory {
    uint8_t mem[MEMORY_SIZE];
};

void memory_init(struct Memory *mem);

void memory_cleanup(struct Memory *mem);

/* zero out all the memory */
void memory_clear(struct Memory *mem);

static inline int
memory_read(struct Memory const *mem, void *buf, size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (end_addr & ~MEMORY_MASK) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, mem->mem + addr, len);

    return 0;
}

static inline int
memory_write(struct Memory *mem, void const *buf, size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (end_addr & ~MEMORY_MASK) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(mem->mem + addr, buf, len);

    return 0;
}

static inline void
memory_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    memcpy(mem->mem + addr, &val, sizeof(val));
}

static inline void
memory_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    memcpy(mem->mem + addr, &val, sizeof(val));
}

static inline void
memory_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    memcpy(mem->mem + addr, &val, sizeof(val));
}

static inline void
memory_write_float(addr32_t addr, float val, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    memcpy(mem->mem + addr, &val, sizeof(val));
}

static inline void
memory_write_double(addr32_t addr, double val, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    memcpy(mem->mem + addr, &val, sizeof(val));
}

static inline uint8_t
memory_read_8(addr32_t addr, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    uint8_t val;
    memcpy(&val, mem->mem + addr, sizeof(val));
    return val;
}

static inline uint16_t
memory_read_16(addr32_t addr, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    uint16_t val;
    memcpy(&val, mem->mem + addr, sizeof(val));
    return val;
}

static inline uint32_t
memory_read_32(addr32_t addr, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    uint32_t val;
    memcpy(&val, mem->mem + addr, sizeof(val));
    return val;
}

static inline float
memory_read_float(addr32_t addr, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    float val;
    memcpy(&val, mem->mem + addr, sizeof(val));
    return val;
}

static inline double
memory_read_double(addr32_t addr, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;
    double val;
    memcpy(&val, mem->mem + addr, sizeof(val));
    return val;
}

extern struct memory_interface ram_intf;

#endif
