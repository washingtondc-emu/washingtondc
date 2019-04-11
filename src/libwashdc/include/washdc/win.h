/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#ifndef WIN_H_
#define WIN_H_

#ifdef __cplusplus
extern "C" {
#endif

struct win_intf {
    void (*init)(unsigned width, unsigned height);
    void (*cleanup)(void);
    void (*check_events)(void);
    void (*update)(void);
    void (*make_context_current)(void);
    void (*update_title)(void);
    int (*get_width)(void);
    int (*get_height)(void);
};

void win_set_intf(struct win_intf const *intf);

void win_init(unsigned width, unsigned height);
void win_cleanup();
void win_check_events(void);
void win_update(void);
void win_make_context_current(void);
void win_update_title(void);
int win_get_width(void);
int win_get_height(void);

#ifdef __cplusplus
}
#endif

#endif
