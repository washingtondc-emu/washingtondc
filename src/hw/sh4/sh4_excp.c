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

#include "sh4.h"
#include "sh4_excp.h"
#include "error.h"

static DEF_ERROR_INT_ATTR(sh4_exception_code)

// structure containing all data necessary to activate a pending IRQ
struct sh4_irq_meta {
    bool is_irl;
    int code;

    // interrupt line, only valid if is_irl is false
    unsigned line;
};

static void sh4_enter_irq_from_meta(Sh4 *sh4, struct sh4_irq_meta *irq_meta);
static int sh4_get_next_irq_line(Sh4 *sh4, struct sh4_irq_meta *irq_meta);

static Sh4ExcpMeta const sh4_excp_meta[SH4_EXCP_COUNT] = {
    // exception code                         prio_level   prio_order   offset
    { SH4_EXCP_POWER_ON_RESET,                    1,           1,           0      },
    { SH4_EXCP_MANUAL_RESET,                      1,           2,           0      },
    { SH4_EXCP_HUDI_RESET,                        1,           1,           0      },
    { SH4_EXCP_INST_TLB_MULT_HIT,                 1,           3,           0      },
    { SH4_EXCP_DATA_TLB_MULT_HIT,                 1,           4,           0      },
    { SH4_EXCP_USER_BREAK_BEFORE,                 2,           0,           0x100  },
    { SH4_EXCP_INST_ADDR_ERR,                     2,           1,           0x100  },
    { SH4_EXCP_INST_TLB_MISS,                     2,           2,           0x400  },
    { SH4_EXCP_INST_TLB_PROT_VIOL,                2,           3,           0x100  },
    { SH4_EXCP_GEN_ILLEGAL_INST,                  2,           4,           0x100  },
    { SH4_EXCP_SLOT_ILLEGAL_INST,                 2,           4,           0x100  },
    { SH4_EXCP_GEN_FPU_DISABLE,                   2,           4,           0x100  },
    { SH4_EXCP_SLOT_FPU_DISABLE,                  2,           4,           0x100  },
    { SH4_EXCP_DATA_ADDR_READ,                    2,           5,           0x100  },
    { SH4_EXCP_DATA_ADDR_WRITE,                   2,           5,           0x100  },
    { SH4_EXCP_DATA_TLB_READ_MISS,                2,           6,           0x400  },
    { SH4_EXCP_DATA_TLB_WRITE_MISS,               2,           6,           0x400  },
    { SH4_EXCP_DATA_TLB_READ_PROT_VIOL,           2,           7,           0x100  },
    { SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL,          2,           7,           0x100  },
    { SH4_EXCP_FPU,                               2,           8,           0x100  },
    { SH4_EXCP_INITIAL_PAGE_WRITE,                2,           9,           0x100  },
    { SH4_EXCP_UNCONDITIONAL_TRAP,                2,           4,           0x100  },
    { SH4_EXCP_USER_BREAK_AFTER,                  2,          10,           0x100  },
    { SH4_EXCP_NMI,                               3,           0,           0x600  },
    { SH4_EXCP_EXT_0,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_1,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_2,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_3,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_4,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_5,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_6,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_7,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_8,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_9,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_A,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_B,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_C,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_D,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_E,                             4,           2,           0x600  },
    { SH4_EXCP_TMU0_TUNI0,                        4,           2,           0x600  },
    { SH4_EXCP_TMU1_TUNI1,                        4,           2,           0x600  },
    { SH4_EXCP_TMU2_TUNI2,                        4,           2,           0x600  },
    { SH4_EXCP_TMU2_TICPI2,                       4,           2,           0x600  },
    { SH4_EXCP_RTC_ATI,                           4,           2,           0x600  },
    { SH4_EXCP_RTC_PRI,                           4,           2,           0x600  },
    { SH4_EXCP_RTC_CUI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_ERI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_RXI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_TXI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_TEI,                           4,           2,           0x600  },
    { SH4_EXCP_WDT_ITI,                           4,           2,           0x600  },
    { SH4_EXCP_REF_RCMI,                          4,           2,           0x600  },
    { SH4_EXCP_REF_ROVI,                          4,           2,           0x600  },
    { SH4_EXCP_GPIO_GPIOI,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE0,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE1,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE2,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE3,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMAE,                         4,           2,           0x600  },
    { SH4_EXCP_SCIF_ERI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_RXI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_BRI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_TXI,                          4,           2,           0x600  }
};

void sh4_enter_exception(Sh4 *sh4, enum Sh4ExceptionCode vector) {
    struct Sh4ExcpMeta const *meta = NULL;
    reg32_t *reg = sh4->reg;

    for (unsigned idx = 0; idx < SH4_EXCP_COUNT; idx++) {
        if (sh4_excp_meta[idx].code == vector) {
            meta = sh4_excp_meta + idx;
            break;
        }
    }

    if (!meta) {
        error_set_sh4_exception_code((int)vector);
        RAISE_ERROR(ERROR_UNKNOWN_EXCP_CODE);
    }

    reg[SH4_REG_SPC] = reg[SH4_REG_PC];
    reg[SH4_REG_SSR] = reg[SH4_REG_SR];
    reg[SH4_REG_SGR] = reg[SH4_REG_R15];

    reg32_t new_sr = reg[SH4_REG_SR];
    new_sr |= (SH4_SR_BL_MASK | SH4_SR_MD_MASK | SH4_SR_RB_MASK);
    new_sr &= ~SH4_SR_FD_MASK;

    sh4_bank_switch_maybe(sh4, reg[SH4_REG_SR], new_sr);
    reg[SH4_REG_SR] = new_sr;

    if (vector == SH4_EXCP_POWER_ON_RESET ||
        vector == SH4_EXCP_MANUAL_RESET ||
        vector == SH4_EXCP_HUDI_RESET ||
        vector == SH4_EXCP_INST_TLB_MULT_HIT ||
        vector == SH4_EXCP_INST_TLB_MULT_HIT) {
        reg[SH4_REG_PC] = 0xa0000000;
    } else if (vector == SH4_EXCP_USER_BREAK_BEFORE ||
               vector == SH4_EXCP_USER_BREAK_AFTER) {
        // TODO: check brcr.ubde and use DBR instead of VBR if it is set
        reg[SH4_REG_PC] = reg[SH4_REG_VBR] + meta->offset;
    } else {
        reg[SH4_REG_PC] = reg[SH4_REG_VBR] + meta->offset;
    }
}

void sh4_set_exception(Sh4 *sh4, unsigned excp_code) {
    sh4->reg[SH4_REG_EXPEVT] = (excp_code << SH4_EXPEVT_CODE_SHIFT) &
        SH4_EXPEVT_CODE_MASK;

    sh4_enter_exception(sh4, (Sh4ExceptionCode)excp_code);
}

void sh4_set_interrupt(Sh4 *sh4, unsigned irq_line,
                       Sh4ExceptionCode intp_code) {
    sh4->intc.irq_lines[irq_line] = intp_code;
}

void sh4_set_irl_interrupt(Sh4 *sh4, unsigned irl_val) {
    irl_val = ~irl_val;

    if (irl_val & 0x1)
        sh4->intc.irq_lines[SH4_IRQ_IRL0] = SH4_EXCP_IRL0;
    else
        sh4->intc.irq_lines[SH4_IRQ_IRL0] = (Sh4ExceptionCode)0;

    if (irl_val & 0x2)
        sh4->intc.irq_lines[SH4_IRQ_IRL1] = SH4_EXCP_IRL1;
    else
        sh4->intc.irq_lines[SH4_IRQ_IRL1] = (Sh4ExceptionCode)0;

    if (irl_val & 0x4)
        sh4->intc.irq_lines[SH4_IRQ_IRL2] = SH4_EXCP_IRL2;
    else
        sh4->intc.irq_lines[SH4_IRQ_IRL2] = (Sh4ExceptionCode)0;

    if (irl_val & 0x8)
        sh4->intc.irq_lines[SH4_IRQ_IRL3] = SH4_EXCP_IRL3;
    else
        sh4->intc.irq_lines[SH4_IRQ_IRL3] = (Sh4ExceptionCode)0;
}

static void sh4_enter_irq_from_meta(Sh4 *sh4, struct sh4_irq_meta *irq_meta) {
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
    }

    // exit sleep/standby mode
    sh4->exec_state = SH4_EXEC_STATE_NORM;
}

void sh4_check_interrupts(Sh4 *sh4) {
    struct sh4_irq_meta irq_meta;
    if (sh4_get_next_irq_line(sh4, &irq_meta) >= 0)
        sh4_enter_irq_from_meta(sh4, &irq_meta);
}

/*
 * return the highest-priority pending IRQ, or -1 if there are none.
 *
 * the exception code will be placed into *code_ptr.  A valid pointer must be
 * provided even if you don't need it.
 */
static int sh4_get_next_irq_line(Sh4 *sh4, struct sh4_irq_meta *irq_meta) {
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
    if (sh4->delayed_branch)
        return -1;
    if (sh4->reg[SH4_REG_SR] & SH4_SR_BL_MASK)
        return -1;

    /* TODO - NMIs */

    int max_prio = -1;
    unsigned max_prio_line = -1;

    /*
     * Skip over SH4_IRQ_IRL3 through SH4_IRQ_IRL0 if those four bits are
     * configured as a 4-bit IRQ bus.
     */
    unsigned last_line = SH4_IRQ_COUNT - 1;
    if (!(sh4->reg[SH4_REG_ICR] & SH4_ICR_IRLM_MASK))
        last_line = SH4_IRQ_GPIO;

    unsigned line;
    for (line = 0; line <= last_line; line++) {
        unsigned ipr_reg_idx = SH4_REG_IPRA + line / 4;
        unsigned prio_shift_amt = 4 * (line % 4);
        unsigned mask = 0xf << prio_shift_amt;
        int prio = (mask & sh4->reg[ipr_reg_idx]) >> prio_shift_amt;

        /* check the sh4's interrupt mask */
        if (prio > (int)((sh4->reg[SH4_REG_SR] & SH4_SR_IMASK_MASK) >>
                         SH4_SR_IMASK_SHIFT)) {
            // only take the highest priority irq
            // TODO: priority order
            if (sh4->intc.irq_lines[line] && (prio > max_prio)) {
                max_prio = prio;
                max_prio_line = line;
            }
        }
    }

    // Now handle the four-bit IRL interrupt as a special case if it's enabled
    if (!(sh4->reg[SH4_REG_ICR] & SH4_ICR_IRLM_MASK)) {
        unsigned irl_val = 0;

        if (sh4->intc.irq_lines[SH4_IRQ_IRL0])
            irl_val |= 1;
        if (sh4->intc.irq_lines[SH4_IRQ_IRL1])
            irl_val |= 2;
        if (sh4->intc.irq_lines[SH4_IRQ_IRL2])
            irl_val |= 4;
        if (sh4->intc.irq_lines[SH4_IRQ_IRL3])
            irl_val |= 8;

        // now make it active-low as it should be
        irl_val = (~irl_val) & 0xf;

        // since it's active-low, 0xf == no interrupt
        if (irl_val != 0xf) {
            int prio;
            Sh4ExceptionCode code;

            /*
             * Yeah, yeah I know that a switch statement
             * isn't the best way to do this...
             */
            switch (irl_val) {
            case 0x0:
                prio = 15;
                code = SH4_EXCP_EXT_0;
                break;
            case 0x1:
                prio = 14;
                code = SH4_EXCP_EXT_1;
                break;
            case 0x2:
                prio = 13;
                code = SH4_EXCP_EXT_2;
                break;
            case 0x3:
                prio = 12;
                code = SH4_EXCP_EXT_3;
                break;
            case 0x4:
                prio = 11;
                code = SH4_EXCP_EXT_4;
                break;
            case 0x5:
                prio = 10;
                code = SH4_EXCP_EXT_5;
                break;
            case 0x6:
                prio = 9;
                code = SH4_EXCP_EXT_6;
                break;
            case 0x7:
                prio = 8;
                code = SH4_EXCP_EXT_7;
                break;
            case 0x8:
                prio = 7;
                code = SH4_EXCP_EXT_8;
                break;
            case 0x9:
                prio = 6;
                code = SH4_EXCP_EXT_9;
                break;
            case 0xa:
                prio = 5;
                code = SH4_EXCP_EXT_A;
                break;
            case 0xb:
                prio = 4;
                code = SH4_EXCP_EXT_B;
                break;
            case 0xc:
                prio = 3;
                code = SH4_EXCP_EXT_C;
                break;
            case 0xd:
                prio = 2;
                code = SH4_EXCP_EXT_D;
                break;
            case 0xe:
                prio = 1;
                code = SH4_EXCP_EXT_E;
                break;
            default:
                RAISE_ERROR(ERROR_INTEGRITY);
            }

            // TODO: priority order
            if (prio > max_prio &&
                (prio > (int)((sh4->reg[SH4_REG_SR] & SH4_SR_IMASK_MASK) >>
                              SH4_SR_IMASK_SHIFT))) {
                irq_meta->is_irl = true;
                irq_meta->code = code;

                return prio;
            }
        }
    }

    if (max_prio >= 0) {
        irq_meta->is_irl = false;
        irq_meta->code = sh4->intc.irq_lines[max_prio_line];
        irq_meta->line = max_prio_line;
        return max_prio;
    }

    return -1;
}
