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

#include <stddef.h>
#include <string.h>

#include "maple.h"
#include "maple_device.h"

#define MAPLE_CONTROLLER_STRING "Dreamcast Controller         "
#define MAPLE_CONTROLLER_LICENSE \
    "Produced By or Under License From SEGA ENTERPRISES,LTD.    "

static int controller_dev_init(struct maple_device *dev);
static void controller_dev_cleanup(struct maple_device *dev);
static void controller_dev_info(struct maple_device *dev,
                                struct maple_devinfo *output);
static void controller_dev_get_cond(struct maple_device *dev,
                                    struct maple_cond *cond);

struct maple_switch_table maple_controller_switch_table = {
    .device_type = "controller",
    .dev_init = controller_dev_init,
    .dev_cleanup = controller_dev_cleanup,
    .dev_info = controller_dev_info,
    .dev_get_cond = controller_dev_get_cond
};

static int controller_dev_init(struct maple_device *dev) {
    dev->ctxt = NULL;
}

static void controller_dev_cleanup(struct maple_device *dev) {
    // do nothing
}

static void controller_dev_info(struct maple_device *dev,
                                struct maple_devinfo *output) {
    // TODO: fill out this structure for real

    memset(output, 0, sizeof(*output));

    output->func = MAPLE_FUNC_CONTROLLER;
    output->func_data[0] = 0xfe060f00;
    output->func_data[1] = 0x00000000;
    output->func_data[2] = 0x724400ff;

    output->area_code = 0xff;
    output->dir = 0;

    strncpy(output->dev_name, MAPLE_CONTROLLER_STRING, MAPLE_DEV_NAME_LEN);
    output->dev_name[MAPLE_DEV_NAME_LEN - 1] = '\0';
    strncpy(output->license, MAPLE_CONTROLLER_LICENSE, MAPLE_DEV_LICENSE_LEN);
    output->license[MAPLE_DEV_LICENSE_LEN - 1] = '\0';

    output->standby_power = 0x01ae;
    output->max_power = 0x01f4;
}

static void controller_dev_get_cond(struct maple_device *dev,
                                    struct maple_cond *cond) {
    memset(cond, 0, sizeof(*cond));

    cond->func = MAPLE_FUNC_CONTROLLER;
    cond->btn = ~0; // Dreamcast controller has active-low buttons, I think
}
