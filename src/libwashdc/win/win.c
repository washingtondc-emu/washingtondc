/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#include "washdc/win.h"

static struct win_intf const *win_intf;

void win_set_intf(struct win_intf const *intf) {
    win_intf = intf;
}

/* void win_init(unsigned width, unsigned height) { */
/*     win_intf->init(width, height); */
/* } */

/* void win_cleanup() { */
/*     win_intf->cleanup(); */
/* } */

void win_check_events(void) {
    win_intf->check_events();
}

void win_run_once_on_suspend(void) {
    win_intf->run_once_on_suspend();
}

void win_update(void) {
    win_intf->update();
}

void win_make_context_current(void) {
    win_intf->make_context_current();
}

void win_update_title(void) {
    win_intf->update_title();
}

int win_get_width(void) {
    return win_intf->get_width();
}

int win_get_height(void) {
    return win_intf->get_height();
}
