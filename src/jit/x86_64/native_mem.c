/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include "emit_x86_64.h"
#include "code_block_x86_64.h"
#include "MemoryMap.h"

#include "native_mem.h"

void native_mem_init(void) {
}

void native_mem_cleanup(void) {
}

void native_mem_read_32(void) {
    x86_64_align_stack();
    x86asm_call_ptr(memory_map_read_32);
}

void native_mem_read_16(void) {
    x86_64_align_stack();
    x86asm_call_ptr(memory_map_read_16);
    x86asm_and_imm32_rax(0x0000ffff);
}

void native_mem_write_32(void) {
    x86_64_align_stack();
    x86asm_call_ptr(memory_map_write_32);
}
