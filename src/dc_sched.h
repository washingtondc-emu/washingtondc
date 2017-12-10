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

/*
 * This represents the timestamp of the next event.
 * outside of dc_sched.c, this should be treated as a read-only variable.
 *
 * It can change whenever an event is scheduled, canceled, or popped.
 */
extern dc_cycle_stamp_t dc_sched_target_stamp;

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
 * these methods do not free or otherwise take ownership of the event.
 * This way, users can use global or static SchedEvent structs.
 */
void sched_event(struct SchedEvent *event);
void cancel_event(struct SchedEvent *event);
struct SchedEvent *pop_event();
struct SchedEvent *peek_event();

#endif
