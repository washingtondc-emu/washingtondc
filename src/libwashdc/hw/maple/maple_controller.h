/*******************************************************************************
 *
 * Copyright 2017, 2020 snickerbockers
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

#ifndef MAPLE_CONTROLLER_H_
#define MAPLE_CONTROLLER_H_

#define MAPLE_CONT_BTN_C_SHIFT 0
#define MAPLE_CONT_BTN_C_MASK (1 << MAPLE_CONT_BTN_C_SHIFT)

#define MAPLE_CONT_BTN_B_SHIFT 1
#define MAPLE_CONT_BTN_B_MASK (1 << MAPLE_CONT_BTN_B_SHIFT)

#define MAPLE_CONT_BTN_A_SHIFT 2
#define MAPLE_CONT_BTN_A_MASK (1 << MAPLE_CONT_BTN_A_SHIFT)

#define MAPLE_CONT_BTN_START_SHIFT 3
#define MAPLE_CONT_BTN_START_MASK (1 << MAPLE_CONT_BTN_START_SHIFT)

#define MAPLE_CONT_BTN_DPAD_UP_SHIFT 4
#define MAPLE_CONT_BTN_DPAD_UP_MASK (1 << MAPLE_CONT_BTN_DPAD_UP_SHIFT)

#define MAPLE_CONT_BTN_DPAD_DOWN_SHIFT 5
#define MAPLE_CONT_BTN_DPAD_DOWN_MASK (1 << MAPLE_CONT_BTN_DPAD_DOWN_SHIFT)

#define MAPLE_CONT_BTN_DPAD_LEFT_SHIFT 6
#define MAPLE_CONT_BTN_DPAD_LEFT_MASK (1 << MAPLE_CONT_BTN_DPAD_LEFT_SHIFT)

#define MAPLE_CONT_BTN_DPAD_RIGHT_SHIFT 7
#define MAPLE_CONT_BTN_DPAD_RIGHT_MASK (1 << MAPLE_CONT_BTN_DPAD_RIGHT_SHIFT)

#define MAPLE_CONT_BTN_Z_SHIFT 8
#define MAPLE_CONT_BTN_Z_MASK (1 << MAPLE_CONT_BTN_Z_SHIFT)

#define MAPLE_CONT_BTN_Y_SHIFT 9
#define MAPLE_CONT_BTN_Y_MASK (1 << MAPLE_CONT_BTN_Y_SHIFT)

#define MAPLE_CONT_BTN_X_SHIFT 10
#define MAPLE_CONT_BTN_X_MASK (1 << MAPLE_CONT_BTN_X_SHIFT)

#define MAPLE_CONT_BTN_D_SHIFT 11
#define MAPLE_CONT_BTN_D_MASK (1 << MAPLE_CONT_BTN_D_SHIFT)

#define MAPLE_CONT_BTN_DPAD2_UP_SHIFT 12
#define MAPLE_CONT_BTN_DPAD2_UP_MASK (1 << MAPLE_CONT_BTN_DPAD2_UP_SHIFT)

#define MAPLE_CONT_BTN_DPAD2_DOWN_SHIFT 13
#define MAPLE_CONT_BTN_DPAD2_DOWN_MASK (1 << MAPLE_CONT_BTN_DPAD2_DOWN_SHIFT)

#define MAPLE_CONT_BTN_DPAD2_LEFT_SHIFT 14
#define MAPLE_CONT_BTN_DPAD2_LEFT_MASK (1 << MAPLE_CONT_BTN_DPAD2_LEFT_SHIFT)

#define MAPLE_CONT_BTN_DPAD2_RIGHT_SHIFT 15
#define MAPLE_CONT_BTN_DPAD2_RIGHT_MASK (1 << MAPLE_CONT_BTN_DPAD2_RIGHT_SHIFT)

enum {
    MAPLE_CONTROLLER_AXIS_R_TRIG,
    MAPLE_CONTROLLER_AXIS_L_TRIG,
    MAPLE_CONTROLLER_AXIS_JOY1_X,
    MAPLE_CONTROLLER_AXIS_JOY1_Y,
    MAPLE_CONTROLLER_AXIS_JOY2_X,
    MAPLE_CONTROLLER_AXIS_JOY2_Y,

    MAPLE_CONTROLLER_N_AXES
};

extern struct maple_switch_table maple_controller_switch_table;

struct maple_controller {
    uint32_t btns;
    unsigned axes[MAPLE_CONTROLLER_N_AXES];
};

struct maple;

/*
 * CONTROLLER API
 * These two functions can be safely called from any thread.
 */
// mark all buttons in btns as being pressed
void maple_controller_press_btns(struct maple *maple, unsigned port_no,
                                 uint32_t btns);

// mark all buttons in btns as being released
void maple_controller_release_btns(struct maple *maple, unsigned port_no,
                                   uint32_t btns);

// 0 = min, 255 = max, 128 = half
void maple_controller_set_axis(struct maple *maple, unsigned port_no,
                               unsigned axis, unsigned val);

struct maple_device;

int maple_controller_init(struct maple_device *dev);

#endif
