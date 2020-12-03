/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifndef CONTROL_BIND_H_
#define CONTROL_BIND_H_

#include <GLFW/glfw3.h>

/*
 * For now you can only bind one host-key to one guest-key, which is kinda lame.
 * In the future we'll let people bind N host keys to one guest key.
 */

#define CTRL_BIND_NAME_LEN 64

struct host_joystick_btn {
    // glfw joystick identifier
    int js;
    int btn;
};

struct host_joystick_axis {
    int js;
    int axis_no;
    int sign; // +1 for positive axis movement, -1 for negative axis movement
};

struct host_joystick_hat {
    int js;
    int hat;
    int mask; // GLFW_HAT_UP, GLFW_HAT_DOWN, GLFW_HAT_LEFT, or GLFW_HAT_RIGHT
};

struct host_gamepad_btn {
    // glfw joystick identifier
    int js;
    unsigned btn; // gamepad button index, NOT joystick button index
};

struct host_gamepad_axis {
    int js; // glfw joystick identifier
    int axis_no; // gamepad axis index, NOT joystick axis index
    int sign;
};

struct host_kbd_ctrl {
    GLFWwindow *win;
    // glfw key identifier
    int key;
};

union host_ctrl {
    struct host_joystick_btn joystick;
    struct host_joystick_axis axis;
    struct host_joystick_hat hat;

    struct host_gamepad_btn gp_btn;
    struct host_gamepad_axis gp_axis;

    struct host_kbd_ctrl kbd;
};

enum HOST_CTRL_TP {
    HOST_CTRL_TP_JOYSTICK_BTN,
    HOST_CTRL_TP_JOYSTICK_AXIS,
    HOST_CTRL_TP_JOYSTICK_HAT,
    HOST_CTRL_TP_GAMEPAD_BTN,
    HOST_CTRL_TP_GAMEPAD_AXIS,
    HOST_CTRL_TP_KBD
};

struct host_ctrl_bind {
    union host_ctrl ctrl;
    enum HOST_CTRL_TP tp;
};

struct host_ctrl_bind *ctrl_get_bind(char const name[CTRL_BIND_NAME_LEN]);

void
ctrl_bind_key(char const bind[CTRL_BIND_NAME_LEN], struct host_ctrl_bind key);

bool ctrl_get_bind_button_state(struct host_ctrl_bind const *key);
float ctrl_get_axis_state(struct host_ctrl_bind const *axis);

/*
 * simple convenience function that wraps ctrl_get_bind + ctrl_get_bind_button_state.
 *
 * This has to look up the bind every time it gets called, so it's less optimal
 * than calling ctrl_get_bind once and holding onto that pointer.
 */
bool ctrl_get_button(char const name[CTRL_BIND_NAME_LEN]);

float ctrl_get_axis(char const name[CTRL_BIND_NAME_LEN]);

int ctrl_parse_bind(char const *bindstr, struct host_ctrl_bind *bind);

void ctrl_bind_init(void);
void ctrl_bind_cleanup(void);

#endif
