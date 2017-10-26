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
layout (location = 1) uniform mat4 trans_mat;
layout (location = 2) in vec4 base_color;
layout (location = 3) in vec4 offs_color;

#ifdef TEX_ENABLE
layout (location = 4) in vec2 tex_coord_in;
#endif

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
     * Given that Dreamcast does all its vertex transformations in software on
     * the SH-4, you might think that it's alright to disregard the perspective
     * divide and just pass through 1.0 for the w coordinate...and you'd be
     * wrong for thinking that.
     *
     * OpenGL doesn't just use the w-coordinate for perspective divide, it also
     * uses it for perspect-correct texture-mapping later in the fragment stage.
     * If the w-coordinate for all vertices in a polygon is the same, then what
     * you get is effictively the same as affine texture-mapping.  Affine
     * texture mapping linearly-interpolates the u and v coordinates, and it
     * looks distorted for polygons where the orthonormal vector doesn't align
     * with the camera direction because it doesn't take the third-dimension
     * into account.  This is because fragments closer to the viewer should
     * sample texels that are closer together to each other than fragments
     * farther away will (i think), and the affine/linear transformation forces
     * them all to linearly sample texels that are the same distance from texels
     * sampled by adjacent fragments.
     *
     * ANYWAYS, perspective-correct texture mapping fixes this by taking the
     * depth-component into account, and it gets that from the w coordinate,
     * which is the value you divide by for perspective-divide; ergo I must use
     * the actual depth coordinate for the perspective divide.  Since the
     * perspective-divide will divide all components by w (which is actually z),
     * I have to multiply all of them by z.
     */
    vec4 pos = trans_mat * vec4(vert_pos, 1.0);
    gl_Position = vec4(pos.x * vert_pos.z, pos.y * vert_pos.z, pos.z * vert_pos.z, vert_pos.z);
}

void color_transform() {
#ifdef COLOR_DISABLE
    vert_base_color = vec4(1.0, 1.0, 1.0, 1.0);
    vert_offs_color = vec4(0.0, 0.0, 0.0, 0.0);
#else
    vert_base_color = base_color;
    vert_offs_color = offs_color;
#endif
}

void main() {
    modelview_project_transform();
    color_transform();
    tex_transform();
}
