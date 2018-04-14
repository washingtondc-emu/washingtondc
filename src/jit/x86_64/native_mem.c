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

/*
 * TODO: Ideally this would sit in the platform-independent JIT code in the
 * form of functions that emit IL operations.  For now that's not possible
 * because there aren't any branching or looping constructs in my jit il yet.
 */

#include <stddef.h>

#include "emit_x86_64.h"
#include "code_block_x86_64.h"
#include "MemoryMap.h"
#include "exec_mem.h"
#include "dreamcast.h"

#include "native_mem.h"

#define BASIC_ALLOC 32

void *native_mem_read_32_impl, *native_mem_read_16_impl,
    *native_mem_write_32_impl;

static void emit_native_mem_read_32(void);
static void emit_native_mem_read_16(void);
static void emit_native_mem_write_32(void);

static void emit_ram_read_32(struct memory_map_region *region);
static void emit_ram_read_16(struct memory_map_region *region);
static void emit_ram_write_32(struct memory_map_region *region);

void native_mem_init(void) {
    emit_native_mem_read_16();
    emit_native_mem_read_32();
    emit_native_mem_write_32();
}

void native_mem_cleanup(void) {
    exec_mem_free(native_mem_read_32_impl);
    exec_mem_free(native_mem_read_16_impl);
}

void native_mem_read_32(void) {
    x86_64_align_stack();
    x86asm_call_ptr(native_mem_read_32_impl);
}

void native_mem_read_16(void) {
    x86_64_align_stack();
    x86asm_call_ptr(native_mem_read_16_impl);
    x86asm_and_imm32_rax(0x0000ffff);
}

void native_mem_write_32(void) {
    x86_64_align_stack();
    x86asm_call_ptr(native_mem_write_32_impl);
}

static void error_func(void) {
    RAISE_ERROR(ERROR_INTEGRITY);
}

static void emit_native_mem_read_16(void) {
    native_mem_read_16_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_16_impl, BASIC_ALLOC);

    unsigned region_no;
    for (region_no = 0; region_no < MEM_MAP_N_REGIONS; region_no++) {
        struct memory_map_region *region = mm_regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(EDI, EAX);
        x86asm_andl_imm32_reg32(region->range_mask, EAX);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint16_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, EAX);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, EAX);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_16(region);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, EDI);
            x86asm_mov_imm64_reg64((uintptr_t)region->read16, RCX);
            x86asm_jmpq_reg64(RCX);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    // raise an error, the memory addr is not in a region
    x86asm_mov_imm64_reg64((uintptr_t)error_func, RSI);
    x86asm_jmpq_reg64(RSI);
}

static void emit_native_mem_read_32(void) {
    native_mem_read_32_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_32_impl, BASIC_ALLOC);

    unsigned region_no;
    for (region_no = 0; region_no < MEM_MAP_N_REGIONS; region_no++) {
        struct memory_map_region *region = mm_regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(EDI, EAX);
        x86asm_andl_imm32_reg32(region->range_mask, EAX);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint32_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, EAX);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, EAX);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_32(region);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, EDI);
            x86asm_mov_imm64_reg64((uintptr_t)region->read32, RCX);
            x86asm_jmpq_reg64(RCX);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    // raise an error, the memory addr is not in a region
    x86asm_mov_imm64_reg64((uintptr_t)error_func, RSI);
    x86asm_jmpq_reg64(RSI);
}

static void emit_native_mem_write_32(void) {
    native_mem_write_32_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_write_32_impl, BASIC_ALLOC);

    unsigned region_no;
    for (region_no = 0; region_no < MEM_MAP_N_REGIONS; region_no++) {
        struct memory_map_region *region = mm_regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(EDI, EAX);
        x86asm_andl_imm32_reg32(region->range_mask, EAX);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint32_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, EAX);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, EAX);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_write_32(region);
            x86asm_ret();
            break;
        default:
            // tail-call (the value to write is still in ESI)
            x86asm_andl_imm32_reg32(region->mask, EDI);
            x86asm_mov_imm64_reg64((uintptr_t)region->write32, RCX);
            x86asm_jmpq_reg64(RCX);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    // raise an error, the memory addr is not in a region
    x86asm_mov_imm64_reg64((uintptr_t)error_func, RSI);
    x86asm_jmpq_reg64(RSI);
}

static void emit_ram_read_32(struct memory_map_region *region) {
    struct Memory *mem = &dc_mem;

    x86asm_andl_imm32_reg32(region->mask, EDI);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, RSI);
    x86asm_movl_sib_reg(RSI, 1, EDI, EAX);
}

static void emit_ram_read_16(struct memory_map_region *region) {
    struct Memory *mem = &dc_mem;

    x86asm_andl_imm32_reg32(region->mask, EDI);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, RSI);
    x86asm_movw_sib_reg(RSI, 1, EDI, EAX);
}

static void emit_ram_write_32(struct memory_map_region *region) {
    // value to write should be in ESI
    // address should be in EDI
    struct Memory *mem = &dc_mem;

    x86asm_andl_imm32_reg32(region->mask, EDI);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, RAX);
    x86asm_movl_reg_sib(ESI, RAX, 1, EDI);
}
