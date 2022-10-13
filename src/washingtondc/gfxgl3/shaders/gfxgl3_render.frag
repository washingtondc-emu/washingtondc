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

void punch_through_test(float alpha);
vec4 eval_tex_inst(vec4 vert_base_color, vec4 vert_offs_color, float w_coord);

in vec4 vert_base_color, vert_offs_color;
out vec4 out_color;

in float w_coord;

void user_clip_test();

void main() {
    user_clip_test();
    vec4 color = eval_tex_inst(vert_base_color, vert_offs_color, w_coord);
    punch_through_test(color.a);
    out_color = color;
}
