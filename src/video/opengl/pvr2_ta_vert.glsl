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

#version 330 core
#extension GL_ARB_explicit_uniform_location : enable

/*
 * TODO: there's no reason why this entire shader couldn't be replaced with
 * a single matrix transformation.
 */

layout (location = 0) in vec3 vert_pos;
layout (location = 1) uniform vec2 half_screen_dims;
layout (location = 2) in vec4 color;
layout (location = 3) uniform vec2 clip_min_max;

out vec4 vert_color;

void main() {
    /*
     * translate coordinates from the Dreamcast's coordinate system (which is
     * screen-coordinates with an origin in the upper-left) to OpenGL
     * coordinates (which are bounded from -1.0 to 1.0, with the upper-left
     * coordinate being at (-1.0, 1.0)
     */
    float vert_x = (vert_pos.x - half_screen_dims.x) / half_screen_dims.x;
    float vert_y = -(vert_pos.y - half_screen_dims.y) / half_screen_dims.y;

    float clip_half = (clip_min_max[1] - clip_min_max[0]) * 0.5f;
    float vert_z = (vert_pos.z - clip_half) /
        (clip_min_max[1] - clip_min_max[0]);

    gl_Position = vec4(vert_x, vert_y, vert_z, 1.0);

    vert_color = color;
}
