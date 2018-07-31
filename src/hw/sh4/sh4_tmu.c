/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sh4_excp.h"
#include "sh4.h"
#include "dc_sched.h"
#include "dreamcast.h"

#include "sh4_tmu.h"

// number of SH4 ticks per TMU tick
#define TMU_DIV_SHIFT 2
#define TMU_DIV      (1 << TMU_DIV_SHIFT)

/*
 * Very Important, this method updates all the tmu accumulators.
 * it does not raise interrupts or set the underflow flag but it will
 * set chan_unf if there is an undeflow in the corresponding channel
 */
static void tmu_chan_sync(Sh4 *sh4, unsigned chan);

static void tmu_chan_event_handler(SchedEvent *ev);
static inline tmu_cycle_t tmu_cycle_stamp();
static inline bool chan_enabled(Sh4 *sh4, unsigned chan);
static inline bool chan_int_enabled(Sh4 *sh4, unsigned chan);
static inline bool chan_enabled(Sh4 *sh4, unsigned chan);
static inline void chan_raise_int(Sh4 *sh4, unsigned chan);
static inline unsigned chan_clock_div(Sh4 *sh4, unsigned chan);
static void tmu_chan_event_handler(SchedEvent *ev);
static tmu_cycle_t next_chan_event(Sh4 *sh4, unsigned chan);
static void chan_event_sched_next(Sh4 *sh4, unsigned chan);

static void chan_event_unsched(Sh4 *sh4, unsigned chan) {
    cancel_event(sh4->clk, sh4->tmu.tmu_chan_event + chan);
    sh4->tmu.chan_event_scheduled[chan] = false;
}

static inline tmu_cycle_t tmu_cycle_stamp(struct Sh4 *sh4) {
    return sh4_get_cycles(sh4) >> TMU_DIV_SHIFT;
}

// lookup table for TCR register indices
static sh4_reg_idx_t const chan_tcr[3] = {
    SH4_REG_TCR0, SH4_REG_TCR1, SH4_REG_TCR2
};

// lookup table for TCNT register indices
static sh4_reg_idx_t const chan_tcnt[3] = {
    SH4_REG_TCNT0, SH4_REG_TCNT1, SH4_REG_TCNT2
};

// lookup table for TCOR register indices
static sh4_reg_idx_t const chan_tcor[3] = {
    SH4_REG_TCOR0, SH4_REG_TCOR1, SH4_REG_TCOR2
};

static inline tmu_cycle_t chan_get_tcnt(Sh4 *sh4, unsigned chan) {
    return (tmu_cycle_t)sh4->reg[chan_tcnt[chan]];
}

static void chan_set_tcnt(Sh4 *sh4, unsigned chan, uint32_t val) {
    sh4->reg[chan_tcnt[chan]] = val;
}

static inline bool chan_enabled(Sh4 *sh4, unsigned chan) {
    return (bool)(sh4->reg[SH4_REG_TSTR] & (1 << chan));
}

static inline bool chan_int_enabled(Sh4 *sh4, unsigned chan) {
    return sh4->reg[chan_tcr[chan]] & SH4_TCR_UNIE_MASK;
}

static inline void chan_raise_int(Sh4 *sh4, unsigned chan) {
    Sh4ExceptionCode code;
    int line = SH4_IRQ_TMU0;
    switch (chan) {
    case 0:
        code = SH4_EXCP_TMU0_TUNI0;
        line = SH4_IRQ_TMU0;
        break;
    case 1:
        code = SH4_EXCP_TMU1_TUNI1;
        line = SH4_IRQ_TMU1;
        break;
    case 2:
        code = SH4_EXCP_TMU2_TUNI2;
        line = SH4_IRQ_TMU2;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4_set_interrupt(sh4, line, code);
}

/*
 * returns the amount by which the TMU clock divides to get he channel clock.
 * multiply this by TMU_DIV to get the SH4 clock.
 */
static inline unsigned chan_clock_div(Sh4 *sh4, unsigned chan) {
    switch(sh4->reg[chan_tcr[chan]] & SH4_TCR_TPSC_MASK) {
    case 0:
        return 4;
    case 1:
        return 16;
    case 2:
        return 64;
    case 3:
        return 256;
    case 4:
        return 1024;
    default:
        // software shouldn't be doing this anyways
        error_set_value(sh4->reg[chan_tcr[chan]] & SH4_TCR_TPSC_MASK);
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }
}

static void tmu_chan_event_handler(SchedEvent *ev) {
    Sh4 *sh4 = (Sh4*)ev->arg_ptr;
    unsigned chan = ev - sh4->tmu.tmu_chan_event;

    tmu_chan_sync(sh4, chan);

    chan_event_sched_next(sh4, chan);
    if (sh4->tmu.chan_unf[chan]) {// TODO: should I even check this?
        sh4->tmu.chan_unf[chan] = false;
        sh4->reg[chan_tcr[chan]] |= SH4_TCR_UNF_MASK;

        if (chan_int_enabled(sh4, chan))
            chan_raise_int(sh4, chan);
    }
}

/*
 * TODO: need to hook into the sh4->exec_state so we know to do a tmu_sync
 * when it enters standby, and also not to sync again (other than updating
 * stamp_last_sync) until it leaves standby mode.
 */
static void tmu_chan_sync(Sh4 *sh4, unsigned chan) {
    tmu_cycle_t stamp_cur = tmu_cycle_stamp(sh4);
    tmu_cycle_t elapsed = stamp_cur - sh4->tmu.stamp_last_sync[chan];
    sh4->tmu.stamp_last_sync[chan] = stamp_cur;

    // chan_unf[chan] = false;
    if (!elapsed)
        return; // nothing to do here

    if (!chan_enabled(sh4, chan))
        return;

    /*
     * TODO: These clock dividers are all powers of two,
     * I could be right-shifting here instead of dividing
     */
    unsigned div = chan_clock_div(sh4, chan);
    sh4->tmu.chan_accum[chan] += elapsed;

    if (sh4->tmu.chan_accum[chan] >= div) {
        tmu_cycle_t chan_cycles = sh4->tmu.chan_accum[chan] / div;
        if (chan_cycles > chan_get_tcnt(sh4, chan)) {
            sh4->tmu.chan_unf[chan] = true;
            chan_set_tcnt(sh4, chan, sh4->reg[chan_tcor[chan]]);
            sh4->reg[chan_tcr[chan]] |= SH4_TCR_UNF_MASK;
        } else {
            chan_set_tcnt(sh4, chan, chan_get_tcnt(sh4, chan) - chan_cycles);
        }
        sh4->tmu.chan_accum[chan] %= div;
    }
}

void sh4_tmu_init(Sh4 *sh4) {
    struct sh4_tmu *tmu = &sh4->tmu;

    memset(tmu, 0, sizeof(*tmu));

    unsigned chan;
    for (chan = 0; chan < 3; chan++) {
        sh4->tmu.tmu_chan_event[chan].handler = tmu_chan_event_handler;
        sh4->tmu.tmu_chan_event[chan].arg_ptr = sh4;
    }
}

void sh4_tmu_cleanup(Sh4 *sh4) {
}

/*
 * return the TMU timestamp of the next interrupt on the given channel,
 * assuming that current conditions remain constant.
 *
 * This function does not schedule the event, it only tells you when the
 * event should happen.  Also, it's the caller's responsibility to check
 * if interrupts are even enabled for the given channel.
 *
 * It's also the caller's responsibility to call tmu_chan_sync prior to this
 * function.
 */
static tmu_cycle_t next_chan_event(Sh4 *sh4, unsigned chan) {
    unsigned clk_div = chan_clock_div(sh4, chan);
    return (chan_get_tcnt(sh4, chan) + 1) * clk_div - sh4->tmu.chan_accum[chan];
}

/*
 * schedule the next interrupt for the given channel.
 *
 * Make sure it's not already scheduled before you call this.
 * This function will check to make sure that the given channel is
 * enabled and that interrupts are enabled for that channel before it schedules
 * the interrupt.
 */
static void chan_event_sched_next(Sh4 *sh4, unsigned chan) {
    SchedEvent *ev = sh4->tmu.tmu_chan_event + chan;

    /*
     * It is not a mistake that the following line checks chan_enabled but not
     * chan_int_enabled.  If the use has enabled the timer channel but not
     * interrupts for the timer channel, then we want to schedule an event to
     * reset the TCNT and set the underflow flag.  It's up to the handler to
     * decide if there needs to be an interrupt when the timer underflows.
     */
    if (!chan_enabled(sh4, chan)) {
        sh4->tmu.chan_event_scheduled[chan] = false;
        return;
    }

    ev->when = (next_chan_event(sh4, chan) +
                clock_cycle_stamp(sh4->clk) / (TMU_DIV * SH4_CLOCK_SCALE)) *
        (TMU_DIV * SH4_CLOCK_SCALE);
    sh4->tmu.chan_event_scheduled[chan] = true;
    sched_event(sh4->clk, ev);
}

sh4_reg_val
sh4_tmu_tocr_read_handler(Sh4 *sh4,
                          struct Sh4MemMappedReg const *reg_info) {
    return 1;
}

void sh4_tmu_tocr_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val) {
    // sh4 spec says you can only write to the least-significant bit.
    // Dreamcast documents say this is always 1.
    sh4->reg[SH4_REG_TOCR] = 1;
}

sh4_reg_val sh4_tmu_tstr_read_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info) {
    return sh4->reg[SH4_REG_TSTR];
}

void sh4_tmu_tstr_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val) {
    uint8_t tmp = val;
    tmp &= 7;

    /*
     * If we don't do a tmu_sync immediately before setting TSTR, then on the
     * next call to tmu_sync, it will think that TSR was set for the entire
     * duration from the last call of tmu_sync to the next call to tmu_sync.
     */
    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN0_MASK) ^
        (tmp & SH4_TSTR_CHAN0_MASK)) {

        tmu_chan_sync(sh4, 0);
        sh4->tmu.chan_accum[0] = 0;

        if (tmp & SH4_TSTR_CHAN0_MASK) {
            if (!sh4->tmu.chan_event_scheduled[0])
                chan_event_sched_next(sh4, 0);
        } else {
            if (sh4->tmu.chan_event_scheduled[0])
                chan_event_unsched(sh4, 0);
        }
    }
    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN1_MASK) ^
        (tmp & SH4_TSTR_CHAN1_MASK)) {

        tmu_chan_sync(sh4, 1);
        sh4->tmu.chan_accum[1] = 0;

        if (tmp & SH4_TSTR_CHAN1_MASK) {
            if (!sh4->tmu.chan_event_scheduled[1])
                chan_event_sched_next(sh4, 1);
        } else {
            if (sh4->tmu.chan_event_scheduled[1])
                chan_event_unsched(sh4, 1);
        }
    }
    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN2_MASK) ^
        (tmp & SH4_TSTR_CHAN2_MASK)) {

        tmu_chan_sync(sh4, 2);
        sh4->tmu.chan_accum[2] = 0;

        if (tmp & SH4_TSTR_CHAN2_MASK) {
            if (!sh4->tmu.chan_event_scheduled[2])
                chan_event_sched_next(sh4, 2);
        } else {
            if (sh4->tmu.chan_event_scheduled[2])
                chan_event_unsched(sh4, 2);
        }
    }

    sh4->reg[SH4_REG_TSTR] = tmp;

    for (unsigned chan = 0; chan < 3; chan++) {
        tmu_chan_sync(sh4, chan);
        if (sh4->tmu.chan_event_scheduled[chan])
            chan_event_unsched(sh4, chan);
        chan_event_sched_next(sh4, chan);
    }
}

sh4_reg_val sh4_tmu_tcr_read_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info) {
    unsigned chan;
    unsigned reg_idx = reg_info->reg_idx;
    if (reg_idx == SH4_REG_TCR0)
        chan = 0;
    else if (reg_idx == SH4_REG_TCR1)
        chan = 1;
    else
        chan = 2;
    tmu_chan_sync(sh4, chan);

    return sh4->reg[reg_idx];
}

void sh4_tmu_tcr_write_handler(Sh4 *sh4,
                               struct Sh4MemMappedReg const *reg_info,
                               sh4_reg_val val) {
    uint16_t new_val = val;

    uint16_t old_val = sh4->reg[reg_info->reg_idx];

    unsigned chan;
    unsigned reg_idx = reg_info->reg_idx;
    if (reg_idx == SH4_REG_TCR0)
        chan = 0;
    else if (reg_idx == SH4_REG_TCR1)
        chan = 1;
    else
        chan = 2;
    tmu_chan_sync(sh4, chan);

    if ((new_val & SH4_TCR_ICPF_MASK) && !(old_val & SH4_TCR_ICPF_MASK))
        new_val &= ~SH4_TCR_ICPF_MASK;

    if ((new_val & SH4_TCR_UNF_MASK) && !(old_val & SH4_TCR_UNF_MASK))
        new_val &= ~SH4_TCR_UNF_MASK;

    if ((old_val & SH4_TCR_TPSC_MASK) != (new_val & SH4_TCR_TPSC_MASK)) {
        // changing clock source; clear accumulated ticks
        sh4->tmu.chan_accum[chan] = 0;
    }

    sh4->reg[reg_idx] = new_val;

    tmu_chan_sync(sh4, chan);

    if (sh4->tmu.chan_event_scheduled[chan])
        chan_event_unsched(sh4, chan);
    chan_event_sched_next(sh4, chan);
}


sh4_reg_val sh4_tmu_tcnt_read_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info) {
    unsigned chan;
    unsigned reg_idx = reg_info->reg_idx;
    switch (reg_idx) {
    case SH4_REG_TCNT0:
        chan = 0;
        break;
    case SH4_REG_TCNT1:
        chan = 1;
        break;
    case SH4_REG_TCNT2:
        chan = 2;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    tmu_chan_sync(sh4, chan);

    return sh4->reg[reg_idx];
}

void sh4_tmu_tcnt_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val) {
    unsigned chan;
    unsigned reg_idx = reg_info->reg_idx;
    switch(reg_info->reg_idx) {
    case SH4_REG_TCNT0:
        chan = 0;
        break;
    case SH4_REG_TCNT1:
        chan = 1;
        break;
    case SH4_REG_TCNT2:
        chan = 2;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    tmu_chan_sync(sh4, chan);
    sh4->reg[reg_idx] = val;
    tmu_chan_sync(sh4, chan);
    if (sh4->tmu.chan_event_scheduled[chan])
        chan_event_unsched(sh4, chan);
    chan_event_sched_next(sh4, chan);
}
