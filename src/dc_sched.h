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

#ifndef DC_SCHED_H_
#define DC_SCHED_H_

#include <stdint.h>

/*
 * this is the least common denominator of 13.5MHz (SPG VCLK)
 * and 200MHz (SH4 CPU clock)
 */
#define SCHED_FREQUENCY 5400000000

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

typedef struct SchedEvent SchedEvent;

/*
 * A clock is an object which contains a timer and a scheduler based off of
 * that timer.  Each CPU will have its own clock, and that clock will be shared
 * with any system that needs to generate events for that CPU.
 */
struct dc_clock {
    // the current value of this clock
    dc_cycle_stamp_t cycle_stamp_priv;
    dc_cycle_stamp_t *cycle_stamp_ptr_priv;

    // the stamp of the next scheduled event
    dc_cycle_stamp_t target_stamp_priv;
    dc_cycle_stamp_t *target_stamp_ptr_priv;

    // the next scheduled event
    struct SchedEvent *ev_next_priv;
};

void dc_clock_init(struct dc_clock *clk);
void dc_clock_cleanup(struct dc_clock *clk);

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
    *clock->cycle_stamp_ptr_priv = val;
}

static inline dc_cycle_stamp_t clock_cycle_stamp(struct dc_clock *clock) {
    return *clock->cycle_stamp_ptr_priv;
}

void clock_set_target_pointer(struct dc_clock *clock, dc_cycle_stamp_t *ptr);

void
clock_set_cycle_stamp_pointer(struct dc_clock *clock, dc_cycle_stamp_t *ptr);

#endif
