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

#ifdef TEX_ENABLE
layout (location = 3) in vec2 tex_coord_in;
uniform mat2 tex_matrix;
#endif

uniform mat4 trans_mat;

out float w_coord;

out vec4 vert_base_color, vert_offs_color;
#ifdef TEX_ENABLE
out vec2 st;
#endif

/*
 * This function performs texture coordinate transformations if textures are
 * enabled.
 */
void tex_transform() {
#ifdef TEX_ENABLE
    st = tex_matrix * tex_coord_in * vert_pos.z;
#endif
}

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

void color_transform() {
#ifdef COLOR_ENABLE
    vert_base_color = base_color * vert_pos.z;
    vert_offs_color = offs_color * vert_pos.z;
#else
    vert_base_color = vec4(vert_pos.z);
    vert_offs_color = vec4(0.0);
#endif
}

void main() {
    modelview_project_transform();
    color_transform();
    tex_transform();
}
