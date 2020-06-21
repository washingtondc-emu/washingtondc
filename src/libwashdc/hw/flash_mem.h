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

#ifndef FLASH_MEM_H_
#define FLASH_MEM_H_

#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"
#include "washdc/washdc.h"

#define FLASH_MEM_SZ (ADDR_FLASH_LAST - ADDR_FLASH_FIRST + 1)

enum flash_state {
    FLASH_STATE_AA,
    FLASH_STATE_55,
    FLASH_STATE_CMD,
    FLASH_STATE_WRITE,

    FLASH_STATE_ERASE
};

struct flash_mem {
    enum flash_state state;

    /*
     * this is set to true when we receive a FLASH_CMD_PRE_ERASE command.
     * It is cleared upon receiving the FLASH_CMD_ERASE
     */
    bool erase_unlocked;

    // if true, the backing file will be written to from flash_mem_cleanup
    bool writeable;

    uint8_t flash_mem[FLASH_MEM_SZ];

    // path to the backing file
    char file_path[WASHDC_PATH_LEN];
};

void flash_mem_init(struct flash_mem *mem, char const *path, bool writeable);
void flash_mem_cleanup(struct flash_mem *mem);

extern struct memory_interface flash_mem_intf;

#endif
