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

in vec2 st;
uniform sampler2D bound_tex;

vec4 eval_tex_inst(vec4 vert_base_color, vec4 vert_offs_color, float w_coord) {
    /*
     * division by w_coord makes it perspective-correct when combined
     * with multiplication by vert_pos.z in the vertex shader.
     */
    vec4 base_color = vert_base_color / w_coord;
    vec4 offs_color = vert_offs_color / w_coord;
    vec4 tex_color = texture(bound_tex, st / w_coord);
    vec4 color;

    color.rgb = tex_color.rgb * base_color.rgb + offs_color.rgb;
    color.a = tex_color.a;

    return color;
}
