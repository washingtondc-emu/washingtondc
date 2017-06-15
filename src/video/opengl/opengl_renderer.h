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

#ifndef OPENGL_RENDERER_H_
#define OPENGL_RENDERER_H_

// this should only be called from the gfx_thread
void render_next_geo_buf(void);

/*
 * block until the geo_buf with the given frame_stamp has rendered.
 *
 * This can only be called from outside of the gfx_thread.
 */
void render_wait_for_frame_stamp(unsigned stamp);

#endif
