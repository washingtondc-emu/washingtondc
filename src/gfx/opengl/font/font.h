/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef FONT_H_
#define FONT_H_

// Very simple bitmap font that I intend to not keep around very long

void font_init(void);
void font_cleanup(void);

/* void font_render(char const *txt, float pos_x, float pos_y, float width); */
void font_render(char const *txt, unsigned col, unsigned row,
                 float screen_w, float screen_h);
void font_render_char(char ch, unsigned col, unsigned row,
                      float screen_w, float screen_h);
void font_get_height(float width);

#endif
