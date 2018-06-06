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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"

#include "arm7.h"

static DEF_ERROR_U32_ATTR(arm7_inst)
static DEF_ERROR_U32_ATTR(arm7_pc)

void arm7_init(struct arm7 *arm7, struct dc_clock *clk) {
    memset(arm7, 0, sizeof(*arm7));
    arm7->clk = clk;
}

void arm7_cleanup(struct arm7 *arm7) {
}

void arm7_set_mem_map(struct arm7 *arm7, struct memory_map *arm7_mem_map) {
    arm7->map = arm7_mem_map;
}

void arm7_reset(struct arm7 *arm7, bool val) {
    // TODO: set the ARM7 to supervisor (svc) mode and enter a reset exception.
    printf("%s(%s)\n", __func__, val ? "true" : "false");
    arm7->enabled = val;
}

void arm7_decode(struct arm7 *arm7, struct arm7_decoded_inst *inst_out,
                 arm7_inst inst) {
    error_set_arm7_inst(inst);
    error_set_arm7_pc(arm7->regs[ARM7_REG_R15]);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

arm7_inst arm7_read_inst(struct arm7 *arm7) {
    return memory_map_read_32(arm7->map, arm7->regs[ARM7_REG_R15]);
}
