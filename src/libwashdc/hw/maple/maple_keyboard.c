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

#include <string.h>

#include "log.h"
#include "washdc/error.h"
#include "maple.h"
#include "maple_device.h"

#include "maple_keyboard.h"

#define MAPLE_KEYBOARD_STRING "Keyboard                      "
#define MAPLE_KEYBOARD_LICENSE                                      \
    "Produced By or Under License From SEGA ENTERPRISES,LTD.     "

static void keyboard_dev_cleanup(struct maple_device *dev);
static void keyboard_dev_info(struct maple_device *dev,
                              struct maple_devinfo *output);
static void keyboard_dev_get_cond(struct maple_device *dev,
                                  struct maple_cond *cond);

struct maple_switch_table maple_keyboard_switch_table = {
    .device_type = "keyboard",
    .dev_cleanup = keyboard_dev_cleanup,
    .dev_info = keyboard_dev_info,
    .dev_get_cond = keyboard_dev_get_cond
};

int maple_keyboard_init(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(&dev->ctxt.kbd, 0, sizeof(dev->ctxt.kbd));

    return 0;
}

static void keyboard_dev_cleanup(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD)))
        RAISE_ERROR(ERROR_INTEGRITY);

    // do nothing
}

static void keyboard_dev_info(struct maple_device *dev,
                              struct maple_devinfo *output) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(output, 0, sizeof(*output));

    output->func = MAPLE_FUNC_KEYBOARD;
    output->func_data[0] = 0x80000502;
    output->func_data[1] = 0x00000000;
    output->func_data[2] = 0x00000000;

    output->area_code = 1;
    output->dir = 0;

    memcpy(output->dev_name, MAPLE_KEYBOARD_STRING, MAPLE_DEV_NAME_LEN);
    memcpy(output->license, MAPLE_KEYBOARD_LICENSE, MAPLE_DEV_LICENSE_LEN);

    output->standby_power = 0x012c;
    output->max_power = 0x0190;
}

static void keyboard_dev_get_cond(struct maple_device *dev,
                                  struct maple_cond *cond) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(cond, 0, sizeof(*cond));
    cond->tp = MAPLE_COND_TYPE_KEYBOARD;
    cond->kbd.func = MAPLE_FUNC_KEYBOARD;

    /*
     * if an element is 0 that means there is no key pressed.
     *
     * need to arrange output such that every non-zero element goes before the
     * first 0 element.  Games will iterate through all 6 keys and assume that
     * no more keys are pressed once they see the first 0.
     */
    memset(cond->kbd.keys, 0, sizeof(cond->kbd.keys));
    int idx;
    int out_idx = 0;
    for (idx = 0; idx < MAPLE_KEYBOARD_ROLLOVER; idx++)
        if (dev->ctxt.kbd.key_states[idx]) {
            cond->kbd.keys[out_idx++] = dev->ctxt.kbd.key_states[idx];
        }

    cond->kbd.mods = (uint8_t)dev->ctxt.kbd.special_keys;
    cond->kbd.leds = 0;
    if (dev->ctxt.kbd.num_lock_led)
        cond->kbd.leds |= 1;
    if (dev->ctxt.kbd.caps_lock_led)
        cond->kbd.leds |= 2;
    if (dev->ctxt.kbd.scroll_lock_led)
        cond->kbd.leds |= 4;
}

void
maple_keyboard_press_key(struct maple *maple, unsigned port_no,
                         unsigned which_key, bool is_pressed) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no keyboard plugged in.\n", port_no);
        return;
    }

    if (which_key == 0x53)
        dev->ctxt.kbd.num_lock_led = is_pressed;
    else if (which_key == 0x39)
        dev->ctxt.kbd.caps_lock_led = is_pressed;
    else if (which_key == 0x47)
        dev->ctxt.kbd.scroll_lock_led = is_pressed;

    uint8_t *key_states = dev->ctxt.kbd.key_states;
    int idx;
    if (is_pressed) {
        for (idx = 0; idx < MAPLE_KEYBOARD_ROLLOVER; idx++)
            if (key_states[idx] == which_key) // already pressed
                return;
        for (idx = 0; idx < MAPLE_KEYBOARD_ROLLOVER; idx++)
            if (!key_states[idx]) {
                key_states[idx] = which_key;
                return;
            }
    } else {
        for (idx = 0; idx < MAPLE_KEYBOARD_ROLLOVER; idx++)
            if (key_states[idx] == which_key)
                key_states[idx] = 0;
    }
}

void
maple_keyboard_press_special(struct maple *maple, unsigned port_no,
                             enum maple_keyboard_special_keys which) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no keyboard plugged in.\n", port_no);
        return;
    }

    dev->ctxt.kbd.special_keys |= which;
}

void
maple_keyboard_release_special(struct maple *maple, unsigned port_no,
                               enum maple_keyboard_special_keys which) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_KEYBOARD))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no keyboard plugged in.\n", port_no);
        return;
    }

    dev->ctxt.kbd.special_keys &= ~which;
}
