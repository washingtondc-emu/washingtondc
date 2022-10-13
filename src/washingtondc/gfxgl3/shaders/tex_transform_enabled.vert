#version 330

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

layout (location = 3) in vec2 tex_coord_in;
uniform mat2 tex_matrix;

out vec2 st;

/*
 * This function performs texture coordinate transformations if textures are
 * enabled.
 */
void tex_transform(vec4 vert_pos) {
    st = tex_matrix * tex_coord_in * vert_pos.z;
}
