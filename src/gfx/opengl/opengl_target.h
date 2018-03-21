/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#ifndef OPENGL_TARGET_H_
#define OPENGL_TARGET_H_

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

/* code for configuring opengl's rendering target (which is a texture+FBO) */

void opengl_target_init(void);

void opengl_target_bind_obj(int obj_handle);
void opengl_target_unbind_obj(void);

// call this before rendering to the target
void opengl_target_begin(unsigned width, unsigned height);

// call this when done rendering to the target
void opengl_target_end(void);

void opengl_target_render_triangles(float *verts, unsigned n_verts);

#endif
