/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef OVERLAY_HPP_
#define OVERLAY_HPP_

// This is a simple UI that can optionally be drawn on top of the screen.

/*
 * this gets called by opengl_video_present to draw the overlay on top of the
 * screen.
 */
void overlay_draw(void);

void overlay_set_fps(double fps);
void overlay_set_virt_fps(double fps);

void overlay_show(bool do_show);

void overlay_init(void);
void overlay_cleanup(void);

// vertex position (x, y, z)
#define UI_SLOT_VERT_POS 0

// vertex texture coordinates (s, t)
#define UI_SLOT_VERT_ST 1

#define UI_SLOT_TRANS_MAT 2

#define UI_SLOT_TEX_MAT 3

#endif
