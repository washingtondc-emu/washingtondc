/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef FLASH_MEM_H_
#define FLASH_MEM_H_

#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

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

    uint8_t flash_mem[FLASH_MEM_SZ];
};

void flash_mem_init(struct flash_mem *mem, char const *path);
void flash_mem_cleanup(struct flash_mem *mem);

extern struct memory_interface flash_mem_intf;

#endif
