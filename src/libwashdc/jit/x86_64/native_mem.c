/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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
#include <stdlib.h>

#include "code_block_x86_64.h"
#include "washdc/MemoryMap.h"
#include "exec_mem.h"
#include "dreamcast.h"
#include "abi.h"

#include "native_mem.h"
#include "emit_x86_64.h"

#define BASIC_ALLOC 32

static void* emit_native_mem_read_float(struct memory_map const *map);
static void* emit_native_mem_read_32(struct memory_map const *map);
static void* emit_native_mem_read_8(struct memory_map const *map);
static void* emit_native_mem_read_16(struct memory_map const *map);
static void* emit_native_mem_write_8(struct memory_map const *map);
static void* emit_native_mem_write_32(struct memory_map const *map);
static void* emit_native_mem_write_float(struct memory_map const *map);

static void
emit_ram_read_float(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_read_32(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_read_16(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_read_8(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_write_8(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_write_32(struct memory_map_region const *region, void *ctxt);
static void
emit_ram_write_float(struct memory_map_region const *region, void *ctxt);

struct native_mem_map {
    struct memory_map const *map;
    struct fifo_node node;
    void *read_float_impl, *read_32_impl, *read_16_impl, *read_8_impl,
        *write_8_impl, *write_32_impl, *write_float_impl;
};

static struct fifo_head native_impl;
static struct native_mem_map *mem_map_impl(struct memory_map const *map);

void native_mem_init(void) {
    fifo_init(&native_impl);
}

void native_mem_cleanup(void) {
    while (fifo_len(&native_impl)) {
        struct fifo_node *node = fifo_pop(&native_impl);
        struct native_mem_map *native_map =
            &FIFO_DEREF(node, struct native_mem_map, node);

        exec_mem_free(native_map->read_float_impl);
        exec_mem_free(native_map->read_32_impl);
        exec_mem_free(native_map->read_16_impl);
        exec_mem_free(native_map->read_8_impl);
        exec_mem_free(native_map->write_8_impl);
        exec_mem_free(native_map->write_32_impl);
        exec_mem_free(native_map->write_float_impl);

        free(native_map);
    }
}

void native_mem_read_float(struct code_block_x86_64 *blk,
                           struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->read_float_impl);
    ms_shadow_close();
}

void native_mem_read_32(struct code_block_x86_64 *blk,
                        struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->read_32_impl);
    ms_shadow_close();
}

void native_mem_read_8(struct code_block_x86_64 *blk,
                        struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->read_8_impl);
    x86asm_and_imm32_rax(0x0000ff);
    ms_shadow_close();
}

void native_mem_read_16(struct code_block_x86_64 *blk,
                        struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->read_16_impl);
    x86asm_and_imm32_rax(0x0000ffff);
    ms_shadow_close();
}

void native_mem_write_8(struct code_block_x86_64 *blk,
                        struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_andl_imm32_reg32(0xff, REG_ARG1);
    x86asm_call_ptr(native_map->write_8_impl);
    ms_shadow_close();
}

void native_mem_write_32(struct code_block_x86_64 *blk,
                         struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->write_32_impl);
    ms_shadow_close();
}

void native_mem_write_float(struct code_block_x86_64 *blk,
                            struct memory_map const *map) {
    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    struct native_mem_map *native_map = mem_map_impl(map);
    if (!native_map)
        RAISE_ERROR(ERROR_INTEGRITY);
    x86asm_call_ptr(native_map->write_float_impl);
    ms_shadow_close();
}

static void error_func(void) {
    RAISE_ERROR(ERROR_INTEGRITY);
}

static void* emit_native_mem_read_8(struct memory_map const *map) {
    void *native_mem_read_8_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_8_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint8_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_8(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG1);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->read8, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->read8) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->read8, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_read_8_impl;
}

static void* emit_native_mem_read_16(struct memory_map const *map) {
    void *native_mem_read_16_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_16_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint16_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_16(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG1);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->read16, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->read16) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->read16, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_read_16_impl;
}

static void* emit_native_mem_read_float(struct memory_map const *map) {
    void *native_mem_read_float_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_float_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    // not actually used as an arg, I just need something volatile here
    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(float) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_float(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG1);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->readfloat, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->readfloat) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->readfloat, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_read_float_impl;
}

static void* emit_native_mem_read_32(struct memory_map const *map) {
    void *native_mem_read_32_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_read_32_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    // not actually used as an arg, I just need something volatile here
    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint32_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_read_32(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG1);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->read32, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->read32) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->read32, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_read_32_impl;
}

static void* emit_native_mem_write_8(struct memory_map const *map) {
    void *native_mem_write_8_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_write_8_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    // not actually used as an arg, I just need something volatile here
    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint8_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_write_8(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call (the value to write is still in ESI)
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG2);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->write8, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->write8) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->write8, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_write_8_impl;
}

static void* emit_native_mem_write_32(struct memory_map const *map) {
    void *native_mem_write_32_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_write_32_impl, NULL, BASIC_ALLOC);

    static unsigned const addr_reg = REG_RET;

    // not actually used as an arg, I just need something volatile here
    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_andl_imm32_reg32(region->range_mask, addr_reg);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(uint32_t) - 1);

        x86asm_cmpl_imm32_reg32(region_start, addr_reg);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, addr_reg);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_write_32(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call (the value to write is still in ESI)
            x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, REG_ARG2);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->write32, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->write32) {
        x86asm_mov_reg32_reg32(REG_ARG0, addr_reg);
        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->write32, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_write_32_impl;
}

static void* emit_native_mem_write_float(struct memory_map const *map) {
    /*
     * XXX: if ADDR_REG is ever not REG_ARG0, this function will need to be
     * changed...
     */
    static unsigned const ADDR_REG = REG_ARG0;

#if defined(ABI_MICROSOFT)
    static unsigned const VAL_REG = REG_ARG1_XMM;
    static unsigned const CTXT_REG = REG_ARG2;
#elif defined(ABI_UNIX)
    static unsigned const VAL_REG = REG_ARG0_XMM;
    static unsigned const CTXT_REG = REG_ARG1;
#else
#error unknown abi
#endif

    void *native_mem_write_float_impl = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_mem_write_float_impl, NULL, BASIC_ALLOC);

    // this corresponds to the addr AND'd with the comparison mask
    static unsigned const CMP_ADDR_REG = REG_RET;

    // not actually used as an arg, I just need something volatile here
    static unsigned const func_call_reg = REG_ARG3;

    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region const *region = map->regions + region_no;

        struct x86asm_lbl8 check_next;
        x86asm_lbl8_init(&check_next);

        // this is safe to do since the address is never moved from ADDR_REG/REG_ARG0
        x86asm_mov_reg32_reg32(ADDR_REG, CMP_ADDR_REG);
        x86asm_andl_imm32_reg32(region->range_mask, CMP_ADDR_REG);

        uint32_t region_start = region->first_addr,
            region_end = region->last_addr - (sizeof(float) - 1);

        x86asm_cmpl_imm32_reg32(region_start, CMP_ADDR_REG);
        x86asm_jb_lbl8(&check_next);

        x86asm_cmpl_imm32_reg32(region_end, CMP_ADDR_REG);
        x86asm_ja_lbl8(&check_next);

        switch (region->id) {
        case MEMORY_MAP_REGION_RAM:
            emit_ram_write_float(region, region->ctxt);
            x86asm_ret();
            break;
        default:
            // tail-call (the value to write is still in VAL_REG)
            x86asm_andl_imm32_reg32(region->mask, ADDR_REG);
            x86asm_mov_imm64_reg64((uintptr_t)region->ctxt, CTXT_REG);
            x86asm_mov_imm64_reg64((uintptr_t)region->intf->writefloat, func_call_reg);
            x86asm_jmpq_reg64(func_call_reg);
        }

        // check next region
        x86asm_lbl8_define(&check_next);
        x86asm_lbl8_cleanup(&check_next);
    }

    struct memory_interface const *unmap = map->unmap;
    if (unmap && unmap->writefloat) {
        // TODO this seems wrong since we AND'd it with region mask above
        x86asm_mov_reg32_reg32(CMP_ADDR_REG, REG_ARG0);

        x86asm_mov_imm64_reg64((uintptr_t)map->unmap_ctxt, REG_ARG1);
        x86asm_mov_imm64_reg64((uintptr_t)unmap->writefloat, func_call_reg);
        x86asm_jmpq_reg64(func_call_reg);
    } else {
        // raise an error, the memory addr is not in a region
        x86asm_mov_imm64_reg64((uintptr_t)error_func, REG_VOL1);
        x86asm_jmpq_reg64(REG_VOL1);
    }

    return native_mem_write_float_impl;
}

static void
emit_ram_read_float(struct memory_map_region const *region, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_ARG1);
    x86asm_movss_sib_xmm(REG_ARG1, 1, REG_ARG0, REG_RET_XMM);
}

static void
emit_ram_read_32(struct memory_map_region const *region, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_ARG1);
    x86asm_movl_sib_reg(REG_ARG1, 1, REG_ARG0, REG_RET);
}

static void
emit_ram_read_16(struct memory_map_region const *region, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_ARG1);
    x86asm_xorl_reg32_reg32(REG_RET, REG_RET);
    x86asm_movw_sib_reg(REG_ARG1, 1, REG_ARG0, REG_RET);
}

static void
emit_ram_read_8(struct memory_map_region const *region, void *ctxt) {
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_ARG1);
    x86asm_xorl_reg32_reg32(REG_RET, REG_RET);
    x86asm_movb_sib_reg(REG_ARG1, 1, REG_ARG0, REG_RET);
}

static void
emit_ram_write_8(struct memory_map_region const *region, void *ctxt) {
    // value to write should be in ESI
    // address should be in EDI
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_RET);
    x86asm_mov_reg32_reg32(REG_ARG1, REG_ARG3);
    x86asm_movb_reg_sib(REG_ARG3, REG_RET, 1, REG_ARG0);
}

static void
emit_ram_write_32(struct memory_map_region const *region, void *ctxt) {
    // value to write should be in ESI
    // address should be in EDI
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_RET);
    x86asm_movl_reg_sib(REG_ARG1, REG_RET, 1, REG_ARG0);
}

static void
emit_ram_write_float(struct memory_map_region const *region, void *ctxt) {
    // address should be in EDI
    struct Memory *mem = (struct Memory*)ctxt;

    x86asm_andl_imm32_reg32(region->mask, REG_ARG0);
    x86asm_mov_imm64_reg64((uintptr_t)mem->mem, REG_RET);

#if defined(ABI_MICROSOFT)
    x86asm_movss_xmm_sib(REG_ARG1_XMM, REG_RET, 1, REG_ARG0);
#elif defined(ABI_UNIX)
    x86asm_movss_xmm_sib(REG_ARG0_XMM, REG_RET, 1, REG_ARG0);
#else
#error unknown abi
#endif
}

static struct native_mem_map *mem_map_impl(struct memory_map const *map) {
    struct fifo_node *curs;
    struct native_mem_map *native_map;
    FIFO_FOREACH(native_impl, curs) {
        native_map = &FIFO_DEREF(curs, struct native_mem_map, node);
        if (native_map->map == map)
            return native_map;
    }

    return NULL;
}
void native_mem_register(struct memory_map const *map) {
    // create a new map
    struct native_mem_map *native_map =
        (struct native_mem_map*)malloc(sizeof(struct native_mem_map));

    native_map->map = map;
    native_map->read_float_impl = emit_native_mem_read_float(map);
    native_map->read_32_impl = emit_native_mem_read_32(map);
    native_map->read_16_impl = emit_native_mem_read_16(map);
    native_map->read_8_impl = emit_native_mem_read_8(map);
    native_map->write_8_impl = emit_native_mem_write_8(map);
    native_map->write_32_impl = emit_native_mem_write_32(map);
    native_map->write_float_impl = emit_native_mem_write_float(map);

    fifo_push(&native_impl, &native_map->node);
}
