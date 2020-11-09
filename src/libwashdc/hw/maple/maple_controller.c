/*******************************************************************************
 *
 * Copyright 2017, 2019, 2020 snickerbockers
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

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "washdc/error.h"
#include "maple.h"
#include "maple_controller.h"
#include "maple_device.h"
#include "log.h"

#define MAPLE_CONTROLLER_STRING "Dreamcast Controller         "
#define MAPLE_CONTROLLER_LICENSE \
    "Produced By or Under License From SEGA ENTERPRISES,LTD.    "

/*
 * TODO: the current controller implementation has a single global state which
 * effects all controllers, meaning that they all have the same buttons pressed
 * at the same time.  Obviously this will need to be reworked when I add support
 * for multiple controllers.
 */

static void controller_dev_cleanup(struct maple_device *dev);
static void controller_dev_info(struct maple_device *dev,
                                struct maple_devinfo *output);
static void controller_dev_get_cond(struct maple_device *dev,
                                    struct maple_cond *cond);

struct maple_switch_table maple_controller_switch_table = {
    .device_type = "controller",
    .dev_cleanup = controller_dev_cleanup,
    .dev_info = controller_dev_info,
    .dev_get_cond = controller_dev_get_cond
};

int maple_controller_init(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER)))
        RAISE_ERROR(ERROR_INTEGRITY);

    dev->ctxt.cont.btns = 0;
    memset(dev->ctxt.cont.axes, 0, sizeof(dev->ctxt.cont.axes));
    dev->ctxt.cont.axes[MAPLE_CONTROLLER_AXIS_JOY1_X] = 128;
    dev->ctxt.cont.axes[MAPLE_CONTROLLER_AXIS_JOY1_Y] = 128;

    return 0;
}

static void controller_dev_cleanup(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER)))
        RAISE_ERROR(ERROR_INTEGRITY);

    // do nothing
}

static void controller_dev_info(struct maple_device *dev,
                                struct maple_devinfo *output) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER)))
        RAISE_ERROR(ERROR_INTEGRITY);

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
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER)))
        RAISE_ERROR(ERROR_INTEGRITY);

    struct maple_controller *cont = &dev->ctxt.cont;

    memset(cond, 0, sizeof(*cond));
    cond->tp = MAPLE_COND_TYPE_CONTROLLER;

    cond->cont.func = MAPLE_FUNC_CONTROLLER;

    // invert because Dreamcast controller has active-low buttons
    cond->cont.btn = ~cont->btns;

    cond->cont.trig_r = cont->axes[MAPLE_CONTROLLER_AXIS_R_TRIG];
    cond->cont.trig_l = cont->axes[MAPLE_CONTROLLER_AXIS_L_TRIG];

    cond->cont.js_x = cont->axes[MAPLE_CONTROLLER_AXIS_JOY1_X];
    cond->cont.js_y = cont->axes[MAPLE_CONTROLLER_AXIS_JOY1_Y];
    cond->cont.js_x2 = cont->axes[MAPLE_CONTROLLER_AXIS_JOY2_X];
    cond->cont.js_y2 = cont->axes[MAPLE_CONTROLLER_AXIS_JOY2_X];
}

// mark all buttons in btns as being pressed
void maple_controller_press_btns(struct maple *maple, unsigned port_no,
                                 uint32_t btns) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no controller plugged in.\n", port_no);
        return;
    }

    struct maple_controller *cont = &dev->ctxt.cont;

    cont->btns |= btns;
}

// mark all buttons in btns as being released
void maple_controller_release_btns(struct maple *maple, unsigned port_no,
                                   uint32_t btns) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no controller plugged in.\n", port_no);
        return;
    }

    struct maple_controller *cont = &dev->ctxt.cont;

    cont->btns &= ~btns;
}

void maple_controller_set_axis(struct maple *maple, unsigned port_no,
                               unsigned axis, unsigned val) {
    unsigned addr = maple_addr_pack(port_no, 0);
    struct maple_device *dev = maple_device_get(maple, addr);

    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER))) {
        LOG_ERROR("Error: unable to press buttons on port %u because "
                  "there is no controller plugged in.\n", port_no);
        return;
    }

    struct maple_controller *cont = &dev->ctxt.cont;

    if (axis >= MAPLE_CONTROLLER_N_AXES)
        RAISE_ERROR(ERROR_INTEGRITY);

    cont->axes[axis] = val;
}
