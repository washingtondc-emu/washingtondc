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
layout (location = 0) in vec4 vert_pos;
layout (location = 1) in vec4 base_color;
layout (location = 2) in vec4 offs_color;

uniform mat4 trans_mat;

out float w_coord;

out vec4 vert_base_color, vert_offs_color;

/*
 * translate coordinates from the Dreamcast's coordinate system (which is
 * screen-coordinates with an origin in the upper-left) to OpenGL
 * coordinates (which are bounded from -1.0 to 1.0, with the upper-left
 * coordinate being at (-1.0, 1.0)
 */
void modelview_project_transform() {
    /*
     * trans_mat is an orthographic transformation, so the z-coordinate
     * passed through to the fragment shader is the original 1/z value
     * from the Dreamcast game.
     */
    w_coord = vert_pos.z;
    gl_Position = trans_mat * vert_pos;
}

void tex_transform(vec4 vert_pos);
vec4 compute_vert_base_color(vec4 vert, vec4 base_color);
vec4 compute_vert_offs_color(vec4 vert, vec4 offs_color);

void main() {
    modelview_project_transform();
    vert_base_color = compute_vert_base_color(vert_pos, base_color);
    vert_offs_color = compute_vert_offs_color(vert_pos, offs_color);
    tex_transform(vert_pos);
}
