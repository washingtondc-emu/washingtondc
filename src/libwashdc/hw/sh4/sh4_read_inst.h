/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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
#include "intmath.h"
#include "log.h"
#include "sh4_mem.h"

#ifdef DEEP_SYSCALL_TRACE
#include "deep_syscall_trace.h"
#endif

static inline void
sh4_enter_irq_from_meta(Sh4 *sh4, struct sh4_irq_meta *irq_meta) {
    sh4->reg[SH4_REG_INTEVT] =
        (irq_meta->code << SH4_INTEVT_CODE_SHIFT) &
        SH4_INTEVT_CODE_MASK;

    /*
     * XXX this check should be unnecessary since (in INVARIANTS mode at least)
     * we already would have done that in sh4_refresh_intc
     */
    if (sh4->delayed_branch)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    sh4_enter_exception(sh4, (Sh4ExceptionCode)irq_meta->code);

    // exit sleep/standby mode
    sh4->exec_state = SH4_EXEC_STATE_NORM;
}

/* check IRQ lines and enter interrupt state if necessary */
static inline void sh4_check_interrupts_no_delay_branch_check(Sh4 *sh4) {
    struct sh4_irq_meta irq_meta;
    if (sh4_get_next_irq_line(sh4, &irq_meta) >= 0)
        sh4_enter_irq_from_meta(sh4, &irq_meta);
}

/*
 * call this every time the interrupt controller's state may have changed to
 * check if there are any interrupts that should be pending.
 *
 * DO NOT CALL THIS FROM CPU CONTEXT!
 * If you need this from CPU context, then called sh4_refresh_intc_deferred
 * (see sh4_excp.h) instead.
 */
static inline void sh4_refresh_intc(Sh4 *sh4) {
#ifdef INVARIANTS
    if (sh4->delayed_branch)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    sh4_check_interrupts_no_delay_branch_check(sh4);
}

static inline int
sh4_do_read_inst(Sh4 *sh4, addr32_t addr, cpu_inst_param *inst_p) {
#ifdef ENABLE_MMU
    switch (sh4_itlb_translate_address(sh4, &addr)) {
    case SH4_ITLB_SUCCESS:
        break;
    case SH4_ITLB_MISS:
        sh4->reg[SH4_REG_TEA] = addr;
        sh4->reg[SH4_REG_PTEH] &= ~BIT_RANGE(10, 31);
        sh4->reg[SH4_REG_PTEH] |= (addr & BIT_RANGE(10, 31));
        sh4_set_exception(sh4, SH4_EXCP_INST_TLB_MISS);

        /*
         * sh4_set_exception would have set this to true.  Here we override it
         * because we weren't going to increment the PC after a failed
         * instruction fetch anyways.
         */
        sh4->dont_increment_pc = false;

        SH4_MEM_TRACE("ITLB MISS\n");
        return -1;
    case SH4_ITLB_PROT_VIOL:
        sh4->reg[SH4_REG_TEA] = addr;
        sh4->reg[SH4_REG_PTEH] &= ~BIT_RANGE(10, 31);
        sh4->reg[SH4_REG_PTEH] |= (addr & BIT_RANGE(10, 31));
        sh4_set_exception(sh4, SH4_EXCP_INST_TLB_PROT_VIOL);

        /*
         * sh4_set_exception would have set this to true.  Here we override it
         * because we weren't going to increment the PC after a failed
         * instruction fetch anyways.
         */
        sh4->dont_increment_pc = false;

        SH4_MEM_TRACE("ITLB protection violation %08X\n", (unsigned)addr);
        return -1;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

#endif

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

    *inst_p = memory_map_read_16(sh4->mem.map, addr);
    return 0;
}

static inline int sh4_read_inst(Sh4 *sh4, cpu_inst_param *inst_p, uint32_t pc) {
#ifdef ENABLE_MMU
    if (pc & 1) {
        // instruction address error for non-aligned PC fetch.

        SH4_MEM_TRACE("INSTRUCTION FETCH ADDRESS ERROR AT PC=%08X\n"
                      "\tR4=%08X, R5=%08X, R6=%08X, R7=%08X\n",
                      (pc & BIT_RANGE(0, 28)),
                      *sh4_gen_reg(sh4, 4), *sh4_gen_reg(sh4, 5),
                      *sh4_gen_reg(sh4, 6), *sh4_gen_reg(sh4, 7));

        sh4->reg[SH4_REG_TEA] = pc;
        sh4->reg[SH4_REG_PTEH] &= ~BIT_RANGE(10, 31);
        sh4->reg[SH4_REG_PTEH] |= (pc & BIT_RANGE(10, 31));
        sh4_set_exception(sh4, SH4_EXCP_INST_ADDR_ERR);

        /*
         * sh4_set_exception would have set this to true.  Here we override it
         * because we weren't going to increment the PC after a failed
         * instruction fetch anyways.
         */
        sh4->dont_increment_pc = false;
        return -1;
    } else if ((pc & BIT_RANGE(29, 31)) == BIT_RANGE(29, 31)) {
        /*
         * I'm pretty sure this should be an instruction fetch address exception
         * like above, but AFAIK the SH4 spec doesn't explicitly say that except
         * for the case where the CPU is not in priveleged mode.  So for now we
         * treat it as an unimplemented behavior error even though I'm pretty
         * sure I know exactly what to do.
         */
        error_set_address(pc);
        error_set_feature("P4 instruction execution address error exception");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
#endif
    return sh4_do_read_inst(sh4, pc, inst_p);
}

static inline unsigned
sh4_do_exec_inst(Sh4 *sh4) {
#ifdef INVARIANTS
    if (sh4->delayed_branch || sh4->dont_increment_pc)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

#ifdef DEEP_SYSCALL_TRACE
    deep_syscall_notify_jump(sh4->reg[SH4_REG_PC]);
#endif

    cpu_inst_param inst;
    if (sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC]) != 0)
        return 0;
    InstOpcode const *op = sh4_decode_inst(inst);

    unsigned n_cycles = sh4_count_inst_cycles(op, &sh4->last_inst_type);
    op->func(sh4, inst);

    if (sh4->dont_increment_pc) {
        // an exception was just raised
        sh4->dont_increment_pc = false;
        goto the_end;
    }

    if (sh4->delayed_branch) {
        if (sh4_read_inst(sh4, &inst, sh4->reg[SH4_REG_PC] + 2) != 0) {
            sh4->dont_increment_pc = false;
            goto the_end;
        }
        op = sh4_decode_inst(inst);
        n_cycles += sh4_count_inst_cycles(op, &sh4->last_inst_type);

        if (op->pc_relative) {
            // raise exception for illegal slot instruction
            LOG_ERROR("**** RAISING SLOT-ILLEGAL INSTRUCTION EXCEPTION ****\n");
            sh4_set_exception(sh4, SH4_EXCP_SLOT_ILLEGAL_INST);
            goto the_end;
        }
        op->func(sh4, inst);

        if (sh4->dont_increment_pc) {
            sh4->dont_increment_pc = false;
            goto the_end;
        }
        sh4->reg[SH4_REG_PC] = sh4->delayed_branch_addr;

        /*
         * XXX (March 2020) I don't remember what I was on about when I wrote
         * the below comment.  This function call may not even be necessary
         * anymore since delay slots and the branches that precede them are
         * indeed treated beingas atomic units now.
         *
         * We need to re-check this since any interrupts which happened
         * during the delay slot will not have been raised.  In the
         * future, it would be better to handle delay slots and the
         * instructions which precede them as atomic units so I don't
         * have to do this.
         */
        sh4->delayed_branch = false;
        sh4_check_interrupts_no_delay_branch_check(sh4);
    } else {
        sh4->reg[SH4_REG_PC] += 2;
    }
 the_end:
    sh4->delayed_branch = false;
    return n_cycles;
}

#endif
