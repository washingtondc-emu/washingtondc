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

#ifdef __cplusplus
extern "C" {
#endif

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

/*
 * these methods do not free or otherwise take ownership of the event.
 * This way, users can use global or static SchedEvent structs.
 */
void sched_event(struct SchedEvent *event);
void cancel_event(struct SchedEvent *event);
struct SchedEvent *pop_event();
struct SchedEvent *peek_event();

#ifdef __cplusplus
}
#endif

#endif
