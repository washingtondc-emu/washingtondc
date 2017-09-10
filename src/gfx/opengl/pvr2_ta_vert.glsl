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

#extension GL_ARB_explicit_uniform_location : enable

layout (location = 0) in vec3 vert_pos;
layout (location = 1) uniform vec2 half_screen_dims;
layout (location = 2) in vec4 color;
layout (location = 3) uniform vec2 clip_min_max;

#ifdef TEX_ENABLE
layout (location = 4) in vec2 tex_coord_in;
#endif

out vec4 vert_color;
#ifdef TEX_ENABLE
out vec2 st;
#endif

/*
 * This function performs texture coordinate transformations if textures are
 * enabled.
 */
void tex_transform() {
#ifdef TEX_ENABLE
    st = tex_coord_in;
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
     * Orthographic projection.  Map all coordinates into the (-1, -1, -1) to
     * (1, 1, 1).  Anything less than -half_screen_dims or greater than
     * half_screen_dims on the x/y axes or anything not between
     * clip_min_max[0]/clip_min_max[1] on the z-axis will be clipped.  Ideally
     * nothing should be clipped on the z-axis because clip_min_max is derived
     * from the minimum and maximum depths.
     */
    mat4 ortho2d = mat4(
                        vec4(1.0 / half_screen_dims.x, 0, 0, 0),
                        vec4(0, -1.0 / half_screen_dims.y, 0, 0),
                        vec4(0, 0, -1.0 / (clip_min_max[1] - clip_min_max[0]), 0),
                        vec4(-1, 1, clip_min_max[0] / (clip_min_max[1] - clip_min_max[0]), 1)
                        );

    gl_Position = ortho2d * vec4(vert_pos, 1);
}

void color_transform() {
#ifdef COLOR_DISABLE
    vert_color = vec4(1.0, 1.0, 1.0, 1.0);
#else
    vert_color = color;
#endif
}

void main() {
    modelview_project_transform();
    color_transform();
    tex_transform();
}
