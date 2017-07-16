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
#endif

void main() {
#ifdef TEX_ENABLE
    // TODO: how do I combine the texture samplewith the vertex color?
    color = texture(bound_tex, st);
#else
    color = vert_color;
#endif
}
