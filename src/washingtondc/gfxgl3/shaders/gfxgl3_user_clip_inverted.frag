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

/*
 * user_clip.x - x_min
 * user_clip.y - y_min
 * user_clip.z - x_max
 * user_clip.w - y_max
 */
uniform vec4 user_clip;

void user_clip_test() {
    bool in_rect = gl_FragCoord.x >= user_clip[0] &&
        gl_FragCoord.x <= user_clip[2] &&
        gl_FragCoord.y >= user_clip[1] &&
        gl_FragCoord.y <= user_clip[3];
    if (in_rect)
        discard;
}
