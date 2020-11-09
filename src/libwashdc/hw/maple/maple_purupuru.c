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

#include "maple_device.h"
#include "washdc/error.h"
#include "log.h"

/*
 * Puru Puru - AKA the "jump pack" (or "rumble pak" if you're a nintendrone)
 *
 * It was marketed as the Jump Pack in NA, but the ID string the dev info
 * command returns calls it a Puru Puru, and that ended up being what the
 * homebrew community calls it.  That's probably what it's called in Japanese.
 */

int maple_purupuru_init(struct maple_device *dev);
static void purupuru_dev_cleanup(struct maple_device *dev);
static void purupuru_dev_info(struct maple_device *dev,
                              struct maple_devinfo *output);
static void purupuru_dev_get_cond(struct maple_device *dev,
                                  struct maple_cond *cond);
static void purupuru_dev_set_cond(struct maple_device *dev,
                                  struct maple_setcond *cond);
static void
purupuru_dev_bwrite(struct maple_device *dev, struct maple_bwrite *bwrite);

/*
 * TODO: need to verify these on real hardware since I don't have access to any
 * of my dreamcasts right now
 *
 * I'm very confident "Puru Puru Pack" is the correct identifier based on old
 * logs captured from real hardware, but the license string may or may not be
 * correct; I'm just assuming that it matches the string on Dreamcast
 * controller.
 */
#define MAPLE_PURUPURU_STRING "Puru Puru Pack               "
#define MAPLE_PURUPURU_LICENSE                                      \
    "Produced By or Under License From SEGA ENTERPRISES,LTD.    "

struct maple_switch_table maple_purupuru_switch_table = {
    .device_type = "purupuru",
    .dev_cleanup = purupuru_dev_cleanup,
    .dev_info = purupuru_dev_info,
    .dev_get_cond = purupuru_dev_get_cond,
    .dev_bwrite = purupuru_dev_bwrite,
    .dev_set_cond = purupuru_dev_set_cond
};

int maple_purupuru_init(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_PURUPURU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(&dev->ctxt, 0, sizeof(dev->ctxt));

    dev->tp = MAPLE_DEVICE_PURUPURU;

    return 0;
}

static void purupuru_dev_cleanup(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_PURUPURU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    // do nothing
}

static void purupuru_dev_info(struct maple_device *dev,
                              struct maple_devinfo *output) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_PURUPURU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(output, 0, sizeof(*output));

    output->func = MAPLE_FUNC_PURUPURU;

    strncpy(output->dev_name, MAPLE_PURUPURU_STRING, MAPLE_DEV_NAME_LEN);
    output->dev_name[MAPLE_DEV_NAME_LEN - 1] = '\0';
    strncpy(output->license, MAPLE_PURUPURU_LICENSE, MAPLE_DEV_LICENSE_LEN);
    output->license[MAPLE_DEV_LICENSE_LEN - 1] = '\0';

    /*
     * TODO: I have no idea what the correct values of these fields should be
     * for Puru Puru.  I'm just copying from Dreamcast Controller!
     */
    output->func_data[0] = 0xfe060f00;
    output->func_data[1] = 0x00000000;
    output->func_data[2] = 0x724400ff;
    output->area_code = 0xff;
    output->dir = 0;
    output->standby_power = 0x01ae;
    output->max_power = 0x01f4;
}

static void
purupuru_dev_bwrite(struct maple_device *dev, struct maple_bwrite *bwrite) {
    LOG_INFO("%s - %u dwords\n", __func__, bwrite->n_dwords);
    unsigned idx;
    for (idx = 0; idx < bwrite->n_dwords; idx++) {
        LOG_INFO("\t%08X\n", (unsigned)bwrite->dat[idx]);
    }
}

static void purupuru_dev_set_cond(struct maple_device *dev,
                                  struct maple_setcond *cond) {
    if (cond->n_dwords >= 2 && (cond->dat[0] & MAPLE_FUNC_PURUPURU)) {
        /*
         * TODO: there's no frontend support for vibrating the controller yet
         * so all we can do is printf that we should be vibrating.
         *
         * Also, I don't know how to decode the vibrate command here.  There's
         * some meaning to cond->dat[1] that effects duration and pattern but
         * I don't have any good docs or homebrew to go off of.  If I had access
         * to a Dreamcast I could write some homebrew and work it out but I'm
         * stuck on the east coast for cancer treatments and all my stuff's
         * still in California.
         */
        printf("***** BZZZZZZZZZZZZZZZ %08X ****\n", (unsigned)cond->dat[1]);
    }
}

static void purupuru_dev_get_cond(struct maple_device *dev,
                                  struct maple_cond *cond) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_CONTROLLER)))
        RAISE_ERROR(ERROR_INTEGRITY);

    RAISE_ERROR(ERROR_UNIMPLEMENTED);
    /* struct maple_controller *cont = &dev->ctxt.cont; */

    /* memset(cond, 0, sizeof(*cond)); */
    /* cond->tp = MAPLE_COND_TYPE_CONTROLLER; */

    /* cond->cont.func = MAPLE_FUNC_CONTROLLER; */

    /* // invert because Dreamcast controller has active-low buttons */
    /* cond->cont.btn = ~cont->btns; */

    /* cond->cont.trig_r = cont->axes[MAPLE_CONTROLLER_AXIS_R_TRIG]; */
    /* cond->cont.trig_l = cont->axes[MAPLE_CONTROLLER_AXIS_L_TRIG]; */

    /* cond->cont.js_x = cont->axes[MAPLE_CONTROLLER_AXIS_JOY1_X]; */
    /* cond->cont.js_y = cont->axes[MAPLE_CONTROLLER_AXIS_JOY1_Y]; */
    /* cond->cont.js_x2 = cont->axes[MAPLE_CONTROLLER_AXIS_JOY2_X]; */
    /* cond->cont.js_y2 = cont->axes[MAPLE_CONTROLLER_AXIS_JOY2_X]; */
}
