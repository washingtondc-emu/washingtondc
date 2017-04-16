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

#ifndef OPENGL_BACKEND_HPP_
#define OPENGL_BACKEND_HPP_

#include <boost/cstdint.hpp>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

/*
 * this gets called every time the framebuffer has a new frame to render.
 * fb_new belongs to the caller, and its contents will be copied into a new
 * storage area.
 *
 * this function is safe to call from outside of the graphics thread
 */
void backend_new_framebuffer(uint32_t const *fb_new,
                             unsigned fb_new_width, unsigned fb_new_height);

void opengl_backend_init();
void opengl_backend_cleanup();

#endif
