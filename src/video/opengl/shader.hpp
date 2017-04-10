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

#include <GL/gl.h>

struct shader {
    GLuint vert_shader;
    GLuint frag_shader;
    GLuint shader_prog_obj;
};

void shader_init_from_file(struct shader *out,
                           char const *vert_shader_path,
                           char const *frag_shader_path);

void shader_init(struct shader *out,
                 char const *vert_shader_src,
                 char const *frag_shader_src);
void shader_cleanup(struct shader const *shader);
