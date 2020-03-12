/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2020 snickerbockers
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "log.h"
#include "washdc/error.h"

#include "maple.h"

#include "maple_device.h"

struct maple_device *maple_device_get(struct maple *maple, unsigned addr) {
    unsigned port, unit;
    maple_addr_unpack(addr, &port, &unit);

    if (port >= MAPLE_PORT_COUNT || unit >= MAPLE_UNIT_COUNT)
        RAISE_ERROR(ERROR_INTEGRITY);
    return &maple->devs[port][unit];
}

int maple_device_init(struct maple *maple, unsigned maple_addr,
                      enum maple_device_type tp) {
    unsigned port, unit;
    maple_addr_unpack(maple_addr, &port, &unit);
    struct maple_device *dev = &maple->devs[port][unit];

    if (dev->enable)
        RAISE_ERROR(ERROR_INTEGRITY);

    switch (tp) {
    case MAPLE_DEVICE_CONTROLLER:
        dev->sw = &maple_controller_switch_table;
        break;
    case MAPLE_DEVICE_KEYBOARD:
        dev->sw = &maple_keyboard_switch_table;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    dev->enable = true;
    dev->tp = tp;
    if (dev->sw->dev_init)
        return dev->sw->dev_init(dev);
    return 0;
}

void maple_device_cleanup(struct maple *maple, unsigned addr) {
    unsigned port, unit;
    maple_addr_unpack(addr, &port, &unit);
    struct maple_device *dev = &maple->devs[port][unit];

    if (dev->sw->dev_cleanup)
        dev->sw->dev_cleanup(dev);
    dev->enable = false;
}

void maple_device_info(struct maple_device *dev,
                       struct maple_devinfo *devinfo) {
    if (dev->sw->dev_info) {
        dev->sw->dev_info(dev, devinfo);
    } else {
        MAPLE_TRACE("no dev_info implementation for %s!?\n",
                    dev->sw->device_type);
        memset(devinfo, 0, sizeof(*devinfo));
    }
}

void maple_device_cond(struct maple_device *dev, struct maple_cond *cond) {
    if (dev->sw->dev_get_cond) {
        dev->sw->dev_get_cond(dev, cond);
    } else {
        MAPLE_TRACE("no get_cond implementation for %s!?\n",
                    dev->sw->device_type);
        memset(cond, 0, sizeof(*cond));
    }
}

void maple_compile_devinfo(struct maple_devinfo const *devinfo_in, void *out) {
    uint8_t *devinfo_out = (uint8_t*)out;

    memcpy(devinfo_out, &devinfo_in->func, sizeof(devinfo_in->func));
    devinfo_out += sizeof(devinfo_in->func);
    memcpy(devinfo_out, devinfo_in->func_data, sizeof(devinfo_in->func_data));
    devinfo_out += sizeof(devinfo_in->func_data);
    memcpy(devinfo_out, &devinfo_in->area_code, sizeof(devinfo_in->area_code));
    devinfo_out += sizeof(devinfo_in->area_code);
    memcpy(devinfo_out, &devinfo_in->dir, sizeof(devinfo_in->dir));
    devinfo_out += sizeof(devinfo_in->dir);
    memcpy(devinfo_out, devinfo_in->dev_name, sizeof(devinfo_in->dev_name));
    devinfo_out += sizeof(devinfo_in->dev_name);
    memcpy(devinfo_out, devinfo_in->license, sizeof(devinfo_in->license));
    devinfo_out += sizeof(devinfo_in->license);
    memcpy(devinfo_out, &devinfo_in->standby_power,
           sizeof(devinfo_in->standby_power));
    devinfo_out += sizeof(devinfo_in->standby_power);
    memcpy(devinfo_out, &devinfo_in->max_power, sizeof(devinfo_in->max_power));
    /* devinfo_out += sizeof(devinfo_in->max_power); */
}

void maple_compile_cond(struct maple_cond const *cond, void *out) {
    char *cond_out = (char*)out;

    switch (cond->tp) {
    case MAPLE_COND_TYPE_CONTROLLER:
        memcpy(cond_out, &cond->cont.func, sizeof(cond->cont.func));
        cond_out += sizeof(cond->cont.func);
        memcpy(cond_out, &cond->cont.btn, sizeof(cond->cont.btn));
        cond_out += sizeof(cond->cont.btn);
        memcpy(cond_out, &cond->cont.trig_r, sizeof(cond->cont.trig_r));
        cond_out += sizeof(cond->cont.trig_r);
        memcpy(cond_out, &cond->cont.trig_l, sizeof(cond->cont.trig_l));
        cond_out += sizeof(cond->cont.trig_l);
        memcpy(cond_out, &cond->cont.js_x, sizeof(cond->cont.js_x));
        cond_out += sizeof(cond->cont.js_x);
        memcpy(cond_out, &cond->cont.js_y, sizeof(cond->cont.js_y));
        cond_out += sizeof(cond->cont.js_y);
        memcpy(cond_out, &cond->cont.js_x2, sizeof(cond->cont.js_x2));
        cond_out += sizeof(cond->cont.js_x2);
        memcpy(cond_out, &cond->cont.js_y2, sizeof(cond->cont.js_y2));
        break;
    case MAPLE_COND_TYPE_KEYBOARD:
        memcpy(cond_out, &cond->kbd.func, sizeof(cond->kbd.func));
        cond_out += sizeof(cond->kbd.func);
        memcpy(cond_out, &cond->kbd.mods, sizeof(cond->kbd.mods));
        cond_out += sizeof(cond->kbd.mods);
        memcpy(cond_out, &cond->kbd.leds, sizeof(cond->kbd.leds));
        cond_out += sizeof(cond->kbd.leds);
        memcpy(cond_out, &cond->kbd.keys, sizeof(cond->kbd.keys));
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}
