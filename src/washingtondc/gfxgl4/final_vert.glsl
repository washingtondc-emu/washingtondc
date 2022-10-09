/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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
#extension GL_ARB_explicit_uniform_location : enable

layout (location = 0) in vec3 vert_pos;
layout (location = 1) in vec2 tex_coord;
layout (location = 2) uniform mat4 trans_mat;
layout (location = 3) uniform mat3 tex_mat;

out vec2 st;

void main() {
    gl_Position = trans_mat * vec4(vert_pos.x, vert_pos.y, vert_pos.z, 1.0);
    st = (tex_mat * vec3(tex_coord.x, tex_coord.y, 1.0)).xy;
}
