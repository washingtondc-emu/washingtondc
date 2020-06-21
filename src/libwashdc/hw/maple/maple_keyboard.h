/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
maple_keyboard_press_special(struct maple *maple, unsigned port_no,
                             enum maple_keyboard_special_keys which);
void
maple_keyboard_release_special(struct maple *maple, unsigned port_no,
                               enum maple_keyboard_special_keys which);
void
maple_keyboard_press_key(struct maple *maple, unsigned port_no,
                         unsigned which_key, bool is_pressed);

#ifdef __cplusplus
}
#endif

#endif
