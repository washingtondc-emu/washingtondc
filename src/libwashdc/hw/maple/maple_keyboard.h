/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
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

#ifndef WASHDC_MAPLE_KEYBOARD_H_
#define WASHDC_MAPLE_KEYBOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MAPLE_KEYBOARD_KEY_COUNT 256

#define MAPLE_KEYBOARD_ROLLOVER 6

enum maple_keyboard_special_keys {
    MAPLE_KEYBOARD_NONE = 0,

    MAPLE_KEYBOARD_LEFT_CTRL = 1,
    MAPLE_KEYBOARD_LEFT_SHIFT = 2,
    MAPLE_KEYBOARD_LEFT_ALT = 4,
    MAPLE_KEYBOARD_S1 = 8,
    MAPLE_KEYBOARD_RIGHT_CTRL = 16,
    MAPLE_KEYBOARD_RIGHT_SHIFT = 32,
    MAPLE_KEYBOARD_RIGHT_ALT = 64,
    MAPLE_KEYBOARD_S2 = 128
};

struct maple_keyboard {
    uint8_t key_states[MAPLE_KEYBOARD_ROLLOVER];
    enum maple_keyboard_special_keys special_keys;
    bool num_lock_led;
    bool caps_lock_led;
    bool scroll_lock_led;
};

extern struct maple_switch_table maple_keyboard_switch_table;

void
maple_keyboard_press_special(unsigned port_no,
                             enum maple_keyboard_special_keys which);
void
maple_keyboard_release_special(unsigned port_no,
                               enum maple_keyboard_special_keys which);
void
maple_keyboard_press_key(unsigned port_no,
                         unsigned which_key, bool is_pressed);

#ifdef __cplusplus
}
#endif

#endif
