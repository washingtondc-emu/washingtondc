/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef WINDOW_H_
#define WINDOW_H_

void win_init(unsigned width, unsigned height);
void win_cleanup();

void win_check_events(void);

/*
 * this function can safely be called from outside of the window thread
 * It's best if  you call it indirectly through win_update()
 */
void win_update(void);

/*
 * this function can safely be called from outside of the window thread.
 * It should only be called from the gfx_thread.
 *
 * It's best if you call it indirectly through win_make_context_current
 */
void win_make_context_current(void);

void win_update_title(void);

int win_get_width(void);
int win_get_height(void);

#endif
