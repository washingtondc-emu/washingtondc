/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#include <string.h>
#include <stdlib.h>

#include "fifo.h"

#include "control_bind.h"

static struct bind_state {
    struct fifo_head bind_list;
} bind_state;

struct ctrl_bind {
    char name[CTRL_BIND_NAME_LEN];
    struct host_ctrl_bind host;
    struct fifo_node list_node;
};

static bool ctrl_get_gamepad_button_state(struct host_gamepad_btn const *btn);
static bool ctrl_get_kbd_button_state(struct host_kbd_ctrl const *btn);
static bool ctrl_get_axis_button_state(struct host_gamepad_axis const *btn);

static float ctrl_get_gamepad_axis_state(struct host_gamepad_btn const *btn);
static float ctrl_get_kbd_axis_state(struct host_kbd_ctrl const *btn);
static float ctrl_get_axis_axis_state(struct host_gamepad_axis const *btn);

void ctrl_bind_init(void) {
    fifo_init(&bind_state.bind_list);
}

void ctrl_bind_cleanup(void) {
    while (!fifo_empty(&bind_state.bind_list)) {
        struct fifo_node *list_node = fifo_pop(&bind_state.bind_list);
        struct ctrl_bind *bind =
            &FIFO_DEREF(list_node, struct ctrl_bind, list_node);
        free(bind);
    }
}

bool ctrl_get_button(char const name[CTRL_BIND_NAME_LEN]) {
    struct host_ctrl_bind *bind = ctrl_get_bind(name);
    if (bind)
        return ctrl_get_bind_button_state(bind);
    return false;
}

float ctrl_get_axis(char const name[CTRL_BIND_NAME_LEN]) {
    struct host_ctrl_bind *bind = ctrl_get_bind(name);
    if (bind)
        return ctrl_get_axis_state(bind);
    return 0.0f;
 }

struct host_ctrl_bind *ctrl_get_bind(char const name[CTRL_BIND_NAME_LEN]) {
    struct fifo_node *curs;
    FIFO_FOREACH(bind_state.bind_list, curs) {
        struct ctrl_bind *bind = &FIFO_DEREF(curs, struct ctrl_bind, list_node);
        if (strcmp(bind->name, name) == 0)
            return &bind->host;
    }
    return NULL;
}

void
ctrl_bind_key(char const bind[CTRL_BIND_NAME_LEN], struct host_ctrl_bind key) {
    struct ctrl_bind *node =
        (struct ctrl_bind*)malloc(sizeof(struct ctrl_bind));

    strncpy(node->name, bind, CTRL_BIND_NAME_LEN);
    node->name[CTRL_BIND_NAME_LEN - 1] = '\0';

    node->host = key;

    fifo_push(&bind_state.bind_list, &node->list_node);
}

void ctrl_bind_kbd_key(char const name[CTRL_BIND_NAME_LEN], GLFWwindow *win, int key) {
    struct host_ctrl_bind bind = {
        .tp = HOST_CTRL_TP_KBD,
        .ctrl = {
            .kbd = {
                .win = win,
                .key = key
            }
        }
    };
    ctrl_bind_key(name, bind);
}

void ctrl_bind_gamepad_btn(char const name[CTRL_BIND_NAME_LEN], int js, unsigned btn) {
    struct host_ctrl_bind bind = {
        .tp = HOST_CTRL_TP_GAMEPAD,
        .ctrl = {
            .gamepad = {
                .js = js,
                .btn = btn
            }
        }
    };
    ctrl_bind_key(name, bind);
}

void
ctrl_bind_axis_btn(char const name[CTRL_BIND_NAME_LEN], int js,
                   int axis, int sign) {
    struct host_ctrl_bind bind = {
        .tp = HOST_CTRL_TP_AXIS,
        .ctrl = {
            .axis = {
                .js = js,
                .axis_no = axis,
                .sign = sign
            }
        }
    };
    ctrl_bind_key(name, bind);
}

bool ctrl_get_bind_button_state(struct host_ctrl_bind const *key) {
    switch(key->tp) {
    case HOST_CTRL_TP_GAMEPAD:
        return ctrl_get_gamepad_button_state(&key->ctrl.gamepad);
    case HOST_CTRL_TP_KBD:
        return ctrl_get_kbd_button_state(&key->ctrl.kbd);
    case HOST_CTRL_TP_AXIS:
        return ctrl_get_axis_button_state(&key->ctrl.axis);
    default:
        return false;
    }
}

float ctrl_get_axis_state(struct host_ctrl_bind const *axis) {
    switch (axis->tp) {
    case HOST_CTRL_TP_GAMEPAD:
        return ctrl_get_gamepad_axis_state(&axis->ctrl.gamepad);
    case HOST_CTRL_TP_KBD:
        return ctrl_get_kbd_axis_state(&axis->ctrl.kbd);
    case HOST_CTRL_TP_AXIS:
        return ctrl_get_axis_axis_state(&axis->ctrl.axis);
    default:
        return 0.0f;
    }
}

static bool ctrl_get_gamepad_button_state(struct host_gamepad_btn const *btn) {
    int len;
    unsigned btn_idx = btn->btn;
    const unsigned char *gamepad_state = glfwGetJoystickButtons(btn->js, &len);
    if (gamepad_state && len > btn_idx)
        return gamepad_state[btn_idx] == GLFW_PRESS;
    return false;
}

static bool ctrl_get_kbd_button_state(struct host_kbd_ctrl const *btn) {
    return glfwGetKey(btn->win, btn->key) == GLFW_PRESS;
}

#define AXIS_BUTTON_THRESH 0.5f

static bool ctrl_get_axis_button_state(struct host_gamepad_axis const *btn) {
    int axis_cnt;
    const float *axis_state = glfwGetJoystickAxes(btn->js, &axis_cnt);

    if (axis_state && axis_cnt > btn->axis_no) {
        if (btn->sign > 0)
            return axis_state[btn->axis_no] > AXIS_BUTTON_THRESH;
        else if (btn->sign < 0)
            return axis_state[btn->axis_no] < -AXIS_BUTTON_THRESH;
    }
    return false;
}

static float ctrl_get_gamepad_axis_state(struct host_gamepad_btn const *btn) {
    int len;
    unsigned btn_idx = btn->btn;
    const unsigned char *gamepad_state = glfwGetJoystickButtons(btn->js, &len);
    if (gamepad_state && len > btn_idx && gamepad_state[btn_idx] == GLFW_PRESS)
        return 1.0f;
    return 0.0f;
}

static float ctrl_get_kbd_axis_state(struct host_kbd_ctrl const *btn) {
    if (glfwGetKey(btn->win, btn->key) == GLFW_PRESS)
        return 1.0f;
    return 0.0f;
}

static float ctrl_get_axis_axis_state(struct host_gamepad_axis const *btn) {
    int axis_cnt;
    const float *axis_state = glfwGetJoystickAxes(btn->js, &axis_cnt);

    if (axis_state && axis_cnt > btn->axis_no) {
        if (btn->sign > 0) {
            if (axis_state[btn->axis_no] > 0.0f)
                return axis_state[btn->axis_no];
            else
                return 0.0f;
        } else if (btn->sign < 0) {
            if (axis_state[btn->axis_no] < 0.0f)
                return -axis_state[btn->axis_no];
            else
                return 0.0f;
        } else {
            return axis_state[btn->axis_no];
        }
    }
    return 0.0f;
}
