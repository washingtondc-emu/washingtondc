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
 *
 * TODO: there's no reason why this entire function couldn't be replaced with
 * a single matrix transformation.
 */
void modelview_project_transform() {
    float vert_x = (vert_pos.x - half_screen_dims.x) / half_screen_dims.x;
    float vert_y = -(vert_pos.y - half_screen_dims.y) / half_screen_dims.y;

    float clip_half = (clip_min_max[1] - clip_min_max[0]) * 0.5f;

    /*
     * TODO: I'm not 100% certain I understand why I'm flipping the
     * z-coordinates here.  I think it has to be done because in OpenGL
     * clip-coordinates,  the positive side of the z-axis is closer to the
     * screen and the negative side is farther away.
     *
     * Ultimately, this is something I am doing because I have to do it to get
     * the glDepthFunc functions to behave the way I expect them to; I'm just
     * not 100% sure if the math is actually working out the way I think it is.
     * It does seem weird to multiply the z-coordinates by -1 like this.
     *
     * HYPOTHESIS:
     * the math here is a little wonky because what we see as the Z-value
     * is actually 1 / z.  The perspective divide would have already been done
     * by the guest software before it passed the coordinates to the TA, so this
     * only really effects the depth-sorting.  Nevertheless, this may need to be
     * revisitied in the future since this code was all written under the
     * assumption that the z-coordinate is actually the z-coordinate.
     *
     * the way we quantize the vertex from (clip_min_max[0], clip_min_max[1])
     * to (-1.0, 1.0) is especially an area of concern because mip_min_max
     * represents the minimum/maximum values of 1/z instead of z, but I thought
     * it was z when I write this code.
     *
     * This could very well be why I have to multiply vert_z by -1.  if
     * vert_pos.z is clip min_max[0], then vert_z will be -1.0, which would mean
     * that in clip-coordinates it's the closest vertex to the camera even
     * though having a minimal 1/z means that Z is very large (far away from
     * the camera).
     */
    float vert_z = -(vert_pos.z - clip_half) /
        (clip_min_max[1] - clip_min_max[0]);

    gl_Position = vec4(vert_x, vert_y, vert_z, 1.0);
}

void color_transform() {
    vert_color = color;
}

void main() {
    modelview_project_transform();
    color_transform();
    tex_transform();
}
