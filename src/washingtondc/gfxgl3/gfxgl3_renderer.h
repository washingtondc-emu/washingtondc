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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#ifndef gfxgl3_renderer_H_
#define gfxgl3_renderer_H_

#include <GL/gl.h>

#include "../renderer.h"

#include "washdc/gfx/gfx_all.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct gfx_rend_if const gfxgl3_rend_if;
extern struct renderer const gfxgl3_renderer;

GLuint gfxgl3_renderer_tex(unsigned obj_no);

unsigned gfxgl3_renderer_tex_get_width(unsigned obj_no);
unsigned gfxgl3_renderer_tex_get_height(unsigned obj_no);

void gfxgl3_renderer_tex_set_dims(unsigned obj_no,
                                  unsigned width, unsigned height);
void gfxgl3_renderer_tex_set_format(unsigned obj_no, GLenum fmt);
void gfxgl3_renderer_tex_set_dat_type(unsigned obj_no, GLenum dat_tp);
void gfxgl3_renderer_tex_set_dirty(unsigned obj_no, bool dirty);
GLenum gfxgl3_renderer_tex_get_format(unsigned obj_no);
GLenum gfxgl3_renderer_tex_get_dat_type(unsigned obj_no);
bool gfxgl3_renderer_tex_get_dirty(unsigned obj_no);

void gfxgl3_renderer_update_tex(unsigned tex_obj);
void gfxgl3_renderer_release_tex(unsigned tex_obj);

#ifdef __cplusplus
}
#endif

#endif
