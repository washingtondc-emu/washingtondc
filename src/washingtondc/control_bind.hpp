/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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
    unsigned btn;
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
