/*******************************************************************************
 *
 * Copyright 2017, 2018 snickerbockers
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

#ifndef DC_SCHED_H_
#define DC_SCHED_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * this is the least common denominator of 13.5MHz (SPG VCLK)
 * and 200MHz (SH4 CPU clock)
 */
#define SCHED_FREQUENCY 5400000000

#define DC_TIMESLICE (SCHED_FREQUENCY / 400)

// simple priority-queue scheduler

typedef uint64_t dc_cycle_stamp_t;

struct SchedEvent;
typedef void(*dc_event_handler_t)(struct SchedEvent *event);

// a scheduled event.
struct SchedEvent {
    dc_cycle_stamp_t when;

    dc_event_handler_t handler;

    void *arg_ptr;

    // linked list, only the scheduler gets to touch these
    struct SchedEvent **pprev_event;
    struct SchedEvent *next_event;
};

enum washdc_clock_idx {
    // countdown until the target
    WASHDC_CLOCK_IDX_COUNTDOWN,

    // the stamp of the next scheduled event
    WASHDC_CLOCK_IDX_TARGET,

    // the current value of this clock
    WASHDC_CLOCK_IDX_STAMP,

    WASHDC_CLOCK_IDX_COUNT
};

typedef struct SchedEvent SchedEvent;

/*
 * A clock is an object which contains a timer and a scheduler based off of
 * that timer.  Each CPU will have its own clock, and that clock will be shared
 * with any system that needs to generate events for that CPU.
 */
struct dc_clock {
    bool (*dispatch)(void *ctxt);
    void *dispatch_ctxt;

    struct SchedEvent timeslice_end_event;

    dc_cycle_stamp_t priv[WASHDC_CLOCK_IDX_COUNT];
    dc_cycle_stamp_t *ptrs_priv;

    // the next scheduled event
    struct SchedEvent *ev_next_priv;
};

void dc_clock_init(struct dc_clock *clk);
void dc_clock_cleanup(struct dc_clock *clk);

bool dc_clock_run_timeslice(struct dc_clock *clk);

/*
 * these methods do not free or otherwise take ownership of the event.
 * This way, users can use global or static SchedEvent structs.
 */
void sched_event(struct dc_clock *clock, struct SchedEvent *event);
void cancel_event(struct dc_clock *clock, struct SchedEvent *event);
struct SchedEvent *pop_event(struct dc_clock *clock);
struct SchedEvent *peek_event(struct dc_clock *clock);

/*
 * This represents the timestamp of the next event.
 * It can change whenever an event is scheduled, canceled, or popped.
 */
dc_cycle_stamp_t clock_target_stamp(struct dc_clock *clock);

static inline void
clock_set_cycle_stamp(struct dc_clock *clock, dc_cycle_stamp_t val) {
    clock->ptrs_priv[WASHDC_CLOCK_IDX_STAMP] = val;
    clock->ptrs_priv[WASHDC_CLOCK_IDX_COUNTDOWN] =
        clock->ptrs_priv[WASHDC_CLOCK_IDX_TARGET] - val;
}

static inline dc_cycle_stamp_t clock_cycle_stamp(struct dc_clock *clock) {
    return clock->ptrs_priv[WASHDC_CLOCK_IDX_TARGET] -
        clock->ptrs_priv[WASHDC_CLOCK_IDX_COUNTDOWN];
}

static inline dc_cycle_stamp_t clock_countdown(struct dc_clock *clock) {
    return clock->ptrs_priv[WASHDC_CLOCK_IDX_COUNTDOWN];
}

/*
 * subtract n_cycles from the countdown.
 *
 * THIS FUNCTION DOES NOT CHECK FOR UNDERFLOWS.  YOU MUST ENSURE THAT
 * n_cycles <= clock_countdown(clock) PRIOR TO CALLING THIS FUNCTION.
 * OTHERWISE, YOU WILL REGRET IT.
 */
static inline void clock_countdown_sub(struct dc_clock *clock,
                                       dc_cycle_stamp_t n_cycles) {
    clock->ptrs_priv[WASHDC_CLOCK_IDX_COUNTDOWN] -= n_cycles;
}

void clock_set_ptrs_priv(struct dc_clock *clock, dc_cycle_stamp_t *ptrs);

#endif
