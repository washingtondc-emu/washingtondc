/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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

namespace overlay {
    void init(bool enabled_debugger);
    void cleanup(void);
    void draw(void);
    void update(void);
    void show(bool do_show);
    void set_fps(double fps);
    void set_virt_fps(double fps);
    void input_text(unsigned codepoint);
}

#endif
