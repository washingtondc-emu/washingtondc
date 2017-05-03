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

#ifndef GFX_THREAD_H_
#define GFX_THREAD_H_

#ifdef __cplusplus
extern "C"{
#endif

/*
 * The purpose of the GFX thread is to handle all the OpenGL and windowing
 * related things.
 */

void gfx_thread_launch(unsigned width, unsigned height);

void gfx_thread_kill();

// signals the gfx thread to wake up and make the opengl backend redraw
void gfx_thread_redraw();

#ifdef __cplusplus
}
#endif

#endif
