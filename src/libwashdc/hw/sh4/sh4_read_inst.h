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

#ifndef SH4_READ_INST_H_
#define SH4_READ_INST_H_

/*
 * the point of this file is to separate out a couple functions from sh4_excp.c
 * and sh4_mem.c that I wanted to inline.
 */

#include "washdc/cpu.h"
#include "sh4.h"
#include "sh4_excp.h"
#include "dreamcast.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

#ifdef DEEP_SYSCALL_TRACE
#include "deep_syscall_trace.h"
#endif

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
static inline void sh4_check_interrupts_no_delay_branch_check(Sh4 *sh4) {
    if (sh4->intc.is_irq_pending) {
        sh4_enter_irq_from_meta(sh4, &sh4->intc.pending_irq);
        sh4->intc.is_irq_pending = false;
    }
}

/* check IRQ lines and enter interrupt state if necessary */
static inline void sh4_check_interrupts(Sh4 *sh4) {
    /*
     * for the purposes of interrupt handling, I treat delayed-branch slots
     * as atomic units because if I allowed an interrupt to happen between the
     * two instructions then I would need a way to track the delayed branch slot
     * until the interrupt handler returns, and I would need to account for
     * situations such as interrupt handlers that never return and interrupt
     * handlers that enable interrupts.
     *
     * And the hardware would have to do that too if that was the way it was
     * implemented, so I'm *assuming* that it doesn't allow interrupts in the
     * middle of delay slots either.
     */
    if (!sh4->delayed_branch)
        sh4_check_interrupts_no_delay_branch_check(sh4);
}

static inline cpu_inst_param sh4_do_read_inst(Sh4 *sh4, addr32_t addr) {
    /*
     * XXX for the interpreter, this function is actually a pretty big
     * bottleneck.  The problem is that 99.999% of the time when we want to
     * read an instruction, that instruction comes from the system memory, and
     * the other 0.001% of the time it comes from the bootrom.  This function
     * used to speed things up by explicitly checking the address to see if
     * it's in memory, and if so it would read directly from memory instead of
     * wasting time parsing through the memory map.
     *
     * In the interests of portability and modularity, I had to take out that
     * optimization because SH4 doesn't have a memory pointer, just a
     * memory-map pointer.  This only impacts performance on the interpreter, on
     * the dynarec performance impact is either negligible or nonexistant.
     *
     * Actually the dynarec is running a little faster since I removed this
     * optimization, but I don't understand why.  It shouldn't have had a
     * positive impact at all, I was expecting either a negligibly small
     * negative impact or no impact at all.  I can't explain that but I guess
     * it's good that things are faster lol.
     */
    addr &= 0x1fffffff;
    return memory_map_read_16(sh4->mem.map, addr);
}

static inline cpu_inst_param sh4_read_inst(Sh4 *sh4) {
    return sh4_do_read_inst(sh4, sh4->reg[SH4_REG_PC]);
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

static inline void
sh4_do_exec_inst(Sh4 *sh4, cpu_inst_param inst, InstOpcode const *op) {
    cpu_inst_param oa = inst;

    if (!(sh4->delayed_branch && op->pc_relative)) {
        opcode_func_t op_func = op->func;
        bool delayed_branch_tmp = sh4->delayed_branch;
        addr32_t delayed_branch_addr_tmp = sh4->delayed_branch_addr;

#ifdef DEEP_SYSCALL_TRACE
        deep_syscall_notify_jump(sh4->reg[SH4_REG_PC]);
#endif
        op_func(sh4, oa);

        /*
         * TRAPA is not supposed to increment the PC.  Ideally it's supposed to
         * jump to an exception handler, but since WashDC implements its own
         * debugger, the emulator needs to handle TRAPA itself.  remote GDB
         * expects the PC that it receives from the stub to always point to the
         * TRAPA instruciton and not the instruction after the TRAPA.
         */
        if (sh4_inst_increments_pc(inst))
            sh4->reg[SH4_REG_PC] += 2;

#ifdef ENABLE_DEBUGGER
        if (!sh4->aborted_operation) {
            if (delayed_branch_tmp) {
                sh4->reg[SH4_REG_PC] = delayed_branch_addr_tmp;
                sh4->delayed_branch = false;

                /*
                 * We need to re-check this since any interrupts which happened
                 * during the delay slot will not have been raised.  In the
                 * future, it would be better to handle delay slots and the
                 * instructions which precede them as atomic units so I don't
                 * have to do this.
                 */
                sh4_check_interrupts_no_delay_branch_check(sh4);
            }
        } else {
            sh4->aborted_operation = false;
        }
#else
        if (delayed_branch_tmp) {
            sh4->reg[SH4_REG_PC] = delayed_branch_addr_tmp;
            sh4->delayed_branch = false;

            /*
             * We need to re-check this since any interrupts which happened
             * during the delay slot will not have been raised.  In the
             * future, it would be better to handle delay slots and the
             * instructions which precede them as atomic units so I don't
             * have to do this.
             */
            sh4_check_interrupts_no_delay_branch_check(sh4);
        }
#endif
    } else {
        // raise exception for illegal slot instruction
        sh4_set_exception(sh4, SH4_EXCP_SLOT_ILLEGAL_INST);
    }
}

#endif
