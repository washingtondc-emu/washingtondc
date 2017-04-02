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

#include "dc_sched.hpp"

static SchedEvent *ev_next;

void sched_event(SchedEvent *event) {
    SchedEvent *next_ptr = ev_next;
    SchedEvent **pprev_ptr = &ev_next;
    while (next_ptr && next_ptr->when < event->when) {
        pprev_ptr = &next_ptr->next_event;
        next_ptr = next_ptr->next_event;
    }
    *pprev_ptr = event;
    if (next_ptr)
        next_ptr->pprev_event = &event->next_event;
    event->next_event = next_ptr;
    event->pprev_event = pprev_ptr;
}

void cancel_event(SchedEvent *event) {
    if (event->next_event)
        event->next_event->pprev_event = event->pprev_event;
    *event->pprev_event = event->next_event;

    // XXX this is unnecessary, but I'm trying to be extra-safe here
    event->next_event = NULL;
    event->pprev_event = NULL;
}

SchedEvent *pop_event() {
    SchedEvent *ev_ret = ev_next;

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

    return ev_ret;
}

SchedEvent *peek_event() {
    return ev_next;
}
