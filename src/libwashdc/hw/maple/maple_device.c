/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019 snickerbockers
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

struct maple_device devs[MAPLE_PORT_COUNT][MAPLE_UNIT_COUNT];

struct maple_device *maple_device_get(unsigned addr) {
    unsigned port, unit;
    maple_addr_unpack(addr, &port, &unit);

    if (port >= MAPLE_PORT_COUNT || unit >= MAPLE_UNIT_COUNT)
        RAISE_ERROR(ERROR_INTEGRITY);
    return &devs[port][unit];
}

int maple_device_init(unsigned maple_addr, enum maple_device_type tp) {
    unsigned port, unit;
    maple_addr_unpack(maple_addr, &port, &unit);
    struct maple_device *dev = &devs[port][unit];

    if (dev->enable)
        RAISE_ERROR(ERROR_INTEGRITY);

    switch (tp) {
    case MAPLE_DEVICE_CONTROLLER:
        dev->sw = &maple_controller_switch_table;
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

void maple_device_cleanup(unsigned addr) {
    unsigned port, unit;
    maple_addr_unpack(addr, &port, &unit);
    struct maple_device *dev = &devs[port][unit];

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
    uint8_t *cond_out = (uint8_t*)out;

    memcpy(cond_out, &cond->func, sizeof(cond->func));
    cond_out += sizeof(cond->func);
    memcpy(cond_out, &cond->btn, sizeof(cond->btn));
    cond_out += sizeof(cond->btn);
    memcpy(cond_out, &cond->trig_r, sizeof(cond->trig_r));
    cond_out += sizeof(cond->trig_r);
    memcpy(cond_out, &cond->trig_l, sizeof(cond->trig_l));
    cond_out += sizeof(cond->trig_l);
    memcpy(cond_out, &cond->js_x, sizeof(cond->js_x));
    cond_out += sizeof(cond->js_x);
    memcpy(cond_out, &cond->js_y, sizeof(cond->js_y));
    cond_out += sizeof(cond->js_y);
    memcpy(cond_out, &cond->js_x2, sizeof(cond->js_x2));
    cond_out += sizeof(cond->js_x2);
    memcpy(cond_out, &cond->js_y2, sizeof(cond->js_y2));
}
