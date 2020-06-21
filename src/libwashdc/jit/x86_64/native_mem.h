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

#ifndef NATIVE_MEM_H_
#define NATIVE_MEM_H_

#include "washdc/MemoryMap.h"

void native_mem_init(void);
void native_mem_cleanup(void);

void native_mem_register(struct memory_map const *map);

/*
 * Normal calling convention rules about which registers are and are not saved
 * apply here.  Obviously dst_reg will not get preserved no matter what.
 */
void native_mem_read_8(struct code_block_x86_64 *blk,
                       struct memory_map const *map);
void native_mem_read_16(struct code_block_x86_64 *blk,
                        struct memory_map const *map);
void native_mem_read_32(struct code_block_x86_64 *blk,
                        struct memory_map const *map);
void native_mem_read_float(struct code_block_x86_64 *blk,
                           struct memory_map const *map);
void native_mem_write_8(struct code_block_x86_64 *blk,
                        struct memory_map const *map);
void native_mem_write_32(struct code_block_x86_64 *blk,
                         struct memory_map const *map);
void native_mem_write_float(struct code_block_x86_64 *blk,
                            struct memory_map const *map);

#endif
