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

#ifndef SH4_REG_H_
#define SH4_REG_H_

#include <assert.h>
#include <stdbool.h>

#include "washdc/types.h"
#include "sh4_reg_flags.h"
#include "washdc/hw/sh4/sh4_reg_idx.h"

struct Sh4;
typedef struct Sh4 Sh4;

static_assert(SH4_REG_FR15 - SH4_REG_FR0 + 1 == 16,
              "incorrect number of FPU registers");
static_assert(SH4_REG_XF15 - SH4_REG_XF0 + 1 == 16,
              "incorrect number of banked FPU registers");

typedef unsigned sh4_reg_val;

/*
 * for the purpose of these handlers, you may assume that the caller has
 * already checked the permissions.
 */
struct Sh4MemMappedReg;

typedef
sh4_reg_val(*sh4_reg_read_handler)(Sh4 *sh4,
                                   struct Sh4MemMappedReg const *reg_info);
typedef
void(*sh4_reg_write_handler)(Sh4 *sh4,
                             struct Sh4MemMappedReg const *reg_info,
                             sh4_reg_val val);

/*
 * TODO: turn this into a radix tree of some sort.
 *
 * Alternatively, I could turn this into a simple lookup array; this
 * would incur a huge memory overhead (hundreds of MB), but it looks like
 * it would be feasible in the $CURRENT_YEAR and it would net a
 * beautiful O(1) mapping from addr32_t to MemMappedReg.
 */
struct Sh4MemMappedReg {
    char const *reg_name;

    /*
     * Some registers can be referenced over a range of addresses.
     * To check for equality between this register and a given physical
     * address, AND the address with addr_mask and then check for equality
     * with addr
     */
    addr32_t addr;  // addr shoud be the p4 addr, not the area7 addr

    unsigned len;

    /* index of the register in the register file */
    sh4_reg_idx_t reg_idx;

    /*
     * if true, the value will be preserved during a manual ("soft") reset
     * and manual_reset_val will be ignored; else value will be set to
     * manual_reset_val during a manual reset.
     */
    bool hold_on_reset;

    sh4_reg_read_handler on_p4_read;
    sh4_reg_write_handler on_p4_write;

    /*
     * if len < 4, then only the lower "len" bytes of
     * these values will be used.
     */
    reg32_t poweron_reset_val;
    reg32_t manual_reset_val;
};
typedef struct Sh4MemMappedReg Sh4MemMappedReg;

/*
 * this is called from the sh4 constructor to
 * initialize all memory-mapped registers
 */
void sh4_init_regs(Sh4 *sh4);

// set up the memory-mapped registers for a reset;
void sh4_poweron_reset_regs(Sh4 *sh4);

/*
 * called for P4 area write ops that
 * fall in the memory-mapped register range
 */
void sh4_write_mem_mapped_reg_float(Sh4 *sh4, addr32_t addr, float val);
void sh4_write_mem_mapped_reg_double(Sh4 *sh4, addr32_t addr, double val);
void sh4_write_mem_mapped_reg_32(Sh4 *sh4, addr32_t addr, uint32_t val);
void sh4_write_mem_mapped_reg_16(Sh4 *sh4, addr32_t addr, uint16_t val);
void sh4_write_mem_mapped_reg_8(Sh4 *sh4, addr32_t addr, uint8_t val);

/*
 * called for P4 area read ops that
 * fall in the memory-mapped register range
 */
float sh4_read_mem_mapped_reg_float(Sh4 *sh4, addr32_t addr);
double sh4_read_mem_mapped_reg_double(Sh4 *sh4, addr32_t addr);
uint32_t sh4_read_mem_mapped_reg_32(Sh4 *sh4, addr32_t addr);
uint16_t sh4_read_mem_mapped_reg_16(Sh4 *sh4, addr32_t addr);
uint8_t sh4_read_mem_mapped_reg_8(Sh4 *sh4, addr32_t addr);

#endif
