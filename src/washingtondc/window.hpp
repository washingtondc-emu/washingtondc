/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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

#ifndef WINDOW_HPP_
#define WINDOW_HPP_

#include "washdc/win.h"

struct win_intf const* get_win_intf_glfw(void);

int win_glfw_get_width(void);
int win_glfw_get_height(void);

bool win_glfw_get_mouse_btn(unsigned btn);

void win_glfw_get_mouse_pos(double *mouse_x, double *mouse_y);
void win_glfw_get_mouse_scroll(double *mouse_x, double *mouse_y);

void win_glfw_update(void);

#endif
