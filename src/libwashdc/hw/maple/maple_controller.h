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

#endif
