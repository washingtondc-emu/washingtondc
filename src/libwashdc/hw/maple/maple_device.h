/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2020 snickerbockers
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

#ifndef MAPLE_DEVICE_H_
#define MAPLE_DEVICE_H_

#include <stdint.h>
#include <stdbool.h>

#include "maple_controller.h"
#include "maple_keyboard.h"

struct maple_device;
struct maple;

#define MAPLE_FUNC_CONTROLLER 0x01000000
#define MAPLE_FUNC_KEYBOARD   0x40000000

#define MAPLE_DEV_NAME_LEN 30
#define MAPLE_DEV_LICENSE_LEN 60

// I don't trust structure padding
#define MAPLE_DEVINFO_SIZE (    \
        sizeof(uint32_t) +      \
        sizeof(uint32_t) * 3 +  \
        sizeof(uint8_t) +       \
        sizeof(uint8_t) +       \
        MAPLE_DEV_NAME_LEN +    \
        MAPLE_DEV_LICENSE_LEN + \
        sizeof(uint16_t) +      \
        sizeof(uint16_t))

#define MAPLE_CONTROLLER_COND_SIZE ( \
     sizeof(uint32_t) +              \
     sizeof(uint16_t) +              \
     sizeof(uint8_t) * 6)

#define MAPLE_KEYBOARD_COND_SIZE (sizeof(uint32_t) + sizeof(uint8_t) * 8)

// device information (response to MAPLE_CMD_DEVINFO)
struct maple_devinfo {
    uint32_t func;
    uint32_t func_data[3];
    uint8_t area_code;
    uint8_t dir;

    // should be padded with spaces instead of 0s.
    // TODO: IDK if the last byte should be a 0 or a space
    char dev_name[MAPLE_DEV_NAME_LEN];

    char license[MAPLE_DEV_LICENSE_LEN];
    uint16_t standby_power;
    uint16_t max_power;
};

struct maple_controller_cond {
    uint32_t func;

    // button flags
    uint16_t btn;

    // right and left analog triggers
    uint8_t trig_r;
    uint8_t trig_l;

    // analog stick horizontal and vertical axes
    uint8_t js_x;
    uint8_t js_y;

    // apparently the protocol has support for two analog sticks
    uint8_t js_x2;
    uint8_t js_y2;
};

struct maple_keyboard_cond {
    uint32_t func;

    uint8_t mods;
    uint8_t leds;
    uint8_t keys[6];
};

enum MAPLE_COND_TYPE {
    MAPLE_COND_TYPE_CONTROLLER,
    MAPLE_COND_TYPE_KEYBOARD
};

// controller state (response to MAPLE_CMD_GETCOND)
struct maple_cond {
    enum MAPLE_COND_TYPE tp;
    union {
        struct maple_controller_cond cont;
        struct maple_keyboard_cond kbd;
    };
};

struct maple_switch_table {
    // used solely for logging within WashingtonDC
    char const *device_type;

    // initialize newly-created maple_device
    int (*dev_init)(struct maple_device*);

    // cleanup maple-device
    void (*dev_cleanup)(struct maple_device*);

    // fetch device info structure on behalf of the guest program
    void (*dev_info)(struct maple_device*, struct maple_devinfo*);

    void (*dev_get_cond)(struct maple_device*, struct maple_cond*);
};

enum maple_device_type {
    MAPLE_DEVICE_CONTROLLER,
    MAPLE_DEVICE_KEYBOARD
};

union maple_device_ctxt {
    struct maple_controller cont;
    struct maple_keyboard kbd;
};

struct maple_device {
    struct maple_switch_table const *sw;

    enum maple_device_type tp;

    // if true, this device is plugged in
    // if false, this device is not plugged in
    bool enable;

    union maple_device_ctxt ctxt;
};

int maple_device_init(struct maple *maple, unsigned maple_addr,
                      enum maple_device_type tp);

void maple_device_cleanup(struct maple *maple, unsigned addr);

void maple_device_info(struct maple_device *dev, struct maple_devinfo *devinfo);

void maple_device_cond(struct maple_device *dev, struct maple_cond *cond);

extern struct maple_switch_table maple_controller_switch_table;

struct maple_device *maple_device_get(struct maple *maple, unsigned addr);

// out must be at least MAPLE_DEVINFO_SIZE bytes long
void maple_compile_devinfo(struct maple_devinfo const *devinfo, void *out);

// out must be at least MAPLE_CONTROLLER_COND_SIZE bytes long
void maple_compile_cond(struct maple_cond const *cond, void *out);

#endif
