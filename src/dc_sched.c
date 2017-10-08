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

#include <stddef.h>

#include "dreamcast.h"

#include "dc_sched.h"

dc_cycle_stamp_t dc_sched_target_stamp;

static struct SchedEvent *ev_next;

static void update_target_stamp(void) {
    if (ev_next) {
        dc_sched_target_stamp = ev_next->when;
    } else {
        /*
         * Somehow there are no events scheduled.
         *
         * Hard to say what to do here.  Constantly checking to see if
         * a new event got pushed would be costly.  Instead I just run
         * the cpu a little, but not so much that I drastically overrun
         * anything that might get scheduled.  The number of cycles to
         * run here is arbitrary, but if it's too low then performance
         * will be negatively impacted and if it's too high then
         * accuracy will be negatively impacted.
         *
         * TBH, I'm not even 100% sure this problem can even happen since
         * there's no way to turn off SPG, TMU, etc.
         */
        dc_sched_target_stamp = dc_cycle_stamp() + 16;
    }
}

void sched_event(struct SchedEvent *event) {
    struct SchedEvent *next_ptr = ev_next;
    struct SchedEvent **pprev_ptr = &ev_next;
    while (next_ptr && next_ptr->when < event->when) {
        pprev_ptr = &next_ptr->next_event;
        next_ptr = next_ptr->next_event;
    }
    *pprev_ptr = event;
    if (next_ptr)
        next_ptr->pprev_event = &event->next_event;
    event->next_event = next_ptr;
    event->pprev_event = pprev_ptr;

    update_target_stamp();
}

void cancel_event(struct SchedEvent *event) {
    if (event->next_event)
        event->next_event->pprev_event = event->pprev_event;
    *event->pprev_event = event->next_event;

    // XXX this is unnecessary, but I'm trying to be extra-safe here
    event->next_event = NULL;
    event->pprev_event = NULL;

    update_target_stamp();
}

struct SchedEvent *pop_event() {
    struct SchedEvent *ev_ret = ev_next;

    if (ev_next) {
        ev_next = ev_next->next_event;
        if (ev_next)
            ev_next->pprev_event = &ev_next;
    }

    // XXX this is unnecessary, but I'm trying to be extra-safe here
    if (ev_ret) {
        ev_ret->next_event = NULL;
        ev_ret->pprev_event = NULL;
    }

    update_target_stamp();

    return ev_ret;
}

struct SchedEvent *peek_event() {
    return ev_next;
}
