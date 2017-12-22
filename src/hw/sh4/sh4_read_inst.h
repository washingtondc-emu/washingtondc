/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef SH4_READ_INST_H_
#define SH4_READ_INST_H_

/*
 * the point of this file is to separate out a couple functions from sh4_excp.c
 * and sh4_mem.c that I wanted to inline.
 */

#include "sh4.h"
#include "sh4_excp.h"
#include "dreamcast.h"
#include "mem_areas.h"
#include "MemoryMap.h"

static inline void
sh4_enter_irq_from_meta(Sh4 *sh4, struct sh4_irq_meta *irq_meta) {
    /*
     * TODO: instead of accepting the INTEVT value from whoever raised the
     * interrupt, we should be figuring out what it should be ourselves
     * based on the IRQ line.
     *
     * (the value currently being used here ultimately originates from the
     * intp_code parameter sent to sh4_set_interrupt).
     */
    sh4->reg[SH4_REG_INTEVT] =
        (irq_meta->code << SH4_INTEVT_CODE_SHIFT) &
        SH4_INTEVT_CODE_MASK;

    sh4_enter_exception(sh4, (Sh4ExceptionCode)irq_meta->code);

    if (irq_meta->is_irl) {
        // TODO: is it right to clear the irl lines like
        //       this after an IRQ has been served?
        sh4_set_irl_interrupt(sh4, 0xf);
    } else {
        sh4->intc.irq_lines[irq_meta->line] = (Sh4ExceptionCode)0;

        // this is safe to call this function here because we're not in CPU
        // context (although we're about to be)
        sh4_refresh_intc(sh4);
    }

    // exit sleep/standby mode
    sh4->exec_state = SH4_EXEC_STATE_NORM;
}

/* check IRQ lines and enter interrupt state if necessary */
static inline void sh4_check_interrupts(Sh4 *sh4) {
    if (sh4->intc.is_irq_pending) {
        sh4_enter_irq_from_meta(sh4, &sh4->intc.pending_irq);
        sh4->intc.is_irq_pending = false;
    }
}

static inline inst_t sh4_read_inst(Sh4 *sh4) {
    addr32_t addr = sh4->reg[SH4_REG_PC] & 0x1fffffff;
    if (addr >= ADDR_AREA3_FIRST && addr <= ADDR_AREA3_LAST) {
        return memory_read_16(&dc_mem, addr & ADDR_AREA3_MASK);
    } else {
        inst_t instr;
        if (memory_map_read(&instr, addr, sizeof(instr)) !=
            MEM_ACCESS_SUCCESS) {
            error_set_address(addr);
            error_set_length(2);
            error_set_feature("reading sh4 program instructions from areas "
                              "other than the RAM and the firmware");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        return instr;
    }

#if 0
    /*
     * this is commented out because you can't leave privileged mode without
     * raising an EROR_UNIMPLEMENTED (see sh4_on_sr_change in sh4.c)
     */
    bool privileged = sh4->reg[SH4_REG_SR] & SH4_SR_MD_MASK ? true : false;

    if (virt_area != SH4_AREA_P0 && !privileged) {
        /*
         * The spec says user-mode processes can only access the U0 area
         * (which overlaps with P0) and the store queue area but I can't find
         * the part where it describes what needs to be done.  Raising the
         * SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL exception seems incorrect since that
         * looks like it's for instances where the page can be looked up in the
         * TLB.
         */
        error_set_feature("CPU exception for unprivileged "
                          "access to high memory areas");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }
#endif
}

static inline unsigned
sh4_do_exec_inst(Sh4 *sh4, inst_t inst, InstOpcode const *op) {
    Sh4OpArgs oa;
    oa.inst = inst;

    opcode_func_t op_func = op->func;

    unsigned cycles = op_func(sh4, oa);

#ifdef ENABLE_DEBUGGER
    sh4->aborted_operation = false;
#endif

    return cycles;
}

#endif
