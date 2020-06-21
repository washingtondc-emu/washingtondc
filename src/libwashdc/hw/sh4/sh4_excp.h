/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef SH4_EXCP_H_
#define SH4_EXCP_H_

#include <stdbool.h>

#include "sh4_reg.h"

enum Sh4ExceptionCode {
    // reset-type exceptions
    SH4_EXCP_POWER_ON_RESET           = 0x000,
    SH4_EXCP_MANUAL_RESET             = 0x020,
    SH4_EXCP_HUDI_RESET               = 0x000,
    SH4_EXCP_INST_TLB_MULT_HIT        = 0x140,
    SH4_EXCP_DATA_TLB_MULT_HIT        = 0x140,

    // general exceptions (re-execution type)
    SH4_EXCP_USER_BREAK_BEFORE        = 0x1e0,
    SH4_EXCP_INST_ADDR_ERR            = 0x0e0,
    SH4_EXCP_INST_TLB_MISS            = 0x040,
    SH4_EXCP_INST_TLB_PROT_VIOL       = 0x0a0,
    SH4_EXCP_GEN_ILLEGAL_INST         = 0x180,
    SH4_EXCP_SLOT_ILLEGAL_INST        = 0x1a0,
    SH4_EXCP_GEN_FPU_DISABLE          = 0x800,
    SH4_EXCP_SLOT_FPU_DISABLE         = 0x820,
    SH4_EXCP_DATA_ADDR_READ           = 0x0e0,
    SH4_EXCP_DATA_ADDR_WRITE          = 0x100,
    SH4_EXCP_DATA_TLB_READ_MISS       = 0x040,
    SH4_EXCP_DATA_TLB_WRITE_MISS      = 0x060,
    SH4_EXCP_DATA_TLB_READ_PROT_VIOL  = 0x0a0,
    SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL = 0x0c0,
    SH4_EXCP_FPU                      = 0x120,
    SH4_EXCP_INITIAL_PAGE_WRITE       = 0x080,

    // general exceptions (completion type)
    SH4_EXCP_UNCONDITIONAL_TRAP       = 0x160,
    SH4_EXCP_USER_BREAK_AFTER         = 0x1e0,

    // interrupt (completion type)
    SH4_EXCP_NMI                      = 0x1c0,
    SH4_EXCP_EXT_0                    = 0x200,
    SH4_EXCP_EXT_1                    = 0x220,
    SH4_EXCP_EXT_2                    = 0x240,
    SH4_EXCP_EXT_3                    = 0x260,
    SH4_EXCP_EXT_4                    = 0x280,
    SH4_EXCP_EXT_5                    = 0x2a0,
    SH4_EXCP_EXT_6                    = 0x2c0,
    SH4_EXCP_EXT_7                    = 0x2e0,
    SH4_EXCP_EXT_8                    = 0x300,
    SH4_EXCP_EXT_9                    = 0x320,
    SH4_EXCP_EXT_A                    = 0x340,
    SH4_EXCP_EXT_B                    = 0x360,
    SH4_EXCP_EXT_C                    = 0x380,
    SH4_EXCP_EXT_D                    = 0x3a0,
    SH4_EXCP_EXT_E                    = 0x3c0,
    SH4_EXCP_IRL0                     = 0x240,
    SH4_EXCP_IRL1                     = 0x2a0,
    SH4_EXCP_IRL2                     = 0x300,
    SH4_EXCP_IRL3                     = 0x360,

    //peripheral module interrupts (completion type)
    SH4_EXCP_TMU0_TUNI0               = 0x400,
    SH4_EXCP_TMU1_TUNI1               = 0x420,
    SH4_EXCP_TMU2_TUNI2               = 0x440,
    SH4_EXCP_TMU2_TICPI2              = 0x460,
    SH4_EXCP_RTC_ATI                  = 0x480,
    SH4_EXCP_RTC_PRI                  = 0x4a0,
    SH4_EXCP_RTC_CUI                  = 0x4c0,
    SH4_EXCP_SCI_ERI                  = 0x4e0,
    SH4_EXCP_SCI_RXI                  = 0x500,
    SH4_EXCP_SCI_TXI                  = 0x520,
    SH4_EXCP_SCI_TEI                  = 0x540,
    SH4_EXCP_WDT_ITI                  = 0x560,
    SH4_EXCP_REF_RCMI                 = 0x580,
    SH4_EXCP_REF_ROVI                 = 0x5a0,
    SH4_EXCP_HUDI_HUDI                = 0x600,
    SH4_EXCP_GPIO_GPIOI               = 0x620,

    // Peripheral module interrupt
    SH4_EXCP_DMAC_DMTE0               = 0x640,
    SH4_EXCP_DMAC_DMTE1               = 0x660,
    SH4_EXCP_DMAC_DMTE2               = 0x680,
    SH4_EXCP_DMAC_DMTE3               = 0x6a0,
    SH4_EXCP_DMAC_DMAE                = 0x6c0,
    SH4_EXCP_SCIF_ERI                 = 0x700,
    SH4_EXCP_SCIF_RXI                 = 0x720,
    SH4_EXCP_SCIF_BRI                 = 0x740,
    SH4_EXCP_SCIF_TXI                 = 0x760
};

typedef enum Sh4ExceptionCode Sh4ExceptionCode;

enum {
    SH4_IRQ_RTC,
    SH4_IRQ_TMU2,
    SH4_IRQ_TMU1,
    SH4_IRQ_TMU0,
    SH4_IRQ_RESERVED,
    SH4_IRQ_SCI1,
    SH4_IRQ_REF,
    SH4_IRQ_WDT,
    SH4_IRQ_HUDI,
    SH4_IRQ_SCIF,
    SH4_IRQ_DMAC,
    SH4_IRQ_GPIO,
    SH4_IRQ_IRL3,
    SH4_IRQ_IRL2,
    SH4_IRQ_IRL1,
    SH4_IRQ_IRL0,

    SH4_IRQ_COUNT
};

/*
 * handler functions for each irq lines.
 * These will return zero if the line is not active, and nonzero if it is active.
 *
 * if the function returns nonzero, then *code will contain the exception code.
 */
typedef int(*sh4_irq_line_fn)(Sh4ExceptionCode *code, void *ctx);

// returns the irl value, or 15 for nothing
typedef int(*sh4_irl_line_fn)(void *ctx);


// structure containing all data necessary to activate a pending IRQ
struct sh4_irq_meta {
    int code;
};

struct sh4_intc {
    sh4_irq_line_fn irq_lines[SH4_IRQ_COUNT];
    void *irq_line_args[SH4_IRQ_COUNT];

    sh4_irl_line_fn irl_line;
    void *irl_line_arg;
};

void
sh4_register_irq_line(Sh4 *sh4, int irq_line,
                      sh4_irq_line_fn fn, void *argp);
void sh4_register_irl_line(Sh4 *sh4, sh4_irl_line_fn fn, void *argp);

typedef struct sh4_intc sh4_intc;

/*
 * called by set_exception and set_interrupt.  This function configures
 * the CPU registers to enter an exception state.
 */
void sh4_enter_exception(Sh4 *sh4, enum Sh4ExceptionCode vector);

void sh4_set_exception(Sh4 *sh4, unsigned excp_code);

/*
 * The following registers (in addition to the IMASK and BL bits in SR) all
 * effect the algorithm which decides when interrupt handlers run; ergo the
 * next pending interrupt needs to be reccomputed every time one of these
 * registers changes (in addtion to the aforementioned bits in SR)
 */
void sh4_excp_icr_reg_write_handler(Sh4 *sh4,
                                    struct Sh4MemMappedReg const *reg_info,
                                    sh4_reg_val val);
void sh4_excp_ipra_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val);
void sh4_excp_iprb_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val);
void sh4_excp_iprc_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val);
void sh4_excp_iprd_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val);

// bits in the SR register which (when changed) can effect the intc
#define SH4_INTC_SR_BITS (SH4_SR_IMASK_MASK | SH4_SR_BL_MASK)

void sh4_refresh_intc_deferred(Sh4 *sh4);

// return the highest-priority pending IRQ, or -1 if there are none.
int sh4_get_next_irq_line(Sh4 const *sh4, struct sh4_irq_meta *irq_meta);

#endif
