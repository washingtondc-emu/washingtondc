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

in vec4 vert_color;
out vec4 color;

#ifdef TEX_ENABLE
in vec2 st;
uniform sampler2D bound_tex;
uniform int tex_inst;
#endif

void main() {
#ifdef TEX_ENABLE
    vec4 tex_color = texture(bound_tex, st);

    // TODO: add the offset values to the rgb components
    switch (tex_inst) {
    default:
    case 0:
        // decal
        color.rgb = tex_color.rgb;
        color.a = tex_color.a;
        break;
    case 1:
        // modulate
        color.rgb = tex_color.rgb * vert_color.rgb;
        color.a = tex_color.a;
        break;
    case 2:
        // decal with alpha
        color.rgb = tex_color.rgb * tex_color.a +
            vert_color.rgb * (1.0 - tex_color.a);
        color.a = vert_color.a;
        break;
    case 3:
        // modulate with alpha
        color = tex_color * vert_color;
        break;
    }
#else
    color = vert_color;
#endif
}
