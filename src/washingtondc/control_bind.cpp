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

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <list>

#include "control_bind.hpp"

struct ctrl_bind {
    char name[CTRL_BIND_NAME_LEN];
    struct host_ctrl_bind host;
};

static struct bind_state {
    std::list<ctrl_bind> bind_list;
} bind_state;

static bool ctrl_get_joystick_button_state(struct host_joystick_btn const *btn);
static bool ctrl_get_kbd_button_state(struct host_kbd_ctrl const *btn);
static bool ctrl_get_axis_button_state(struct host_joystick_axis const *btn);
static bool ctrl_get_joystick_hat_state(struct host_joystick_hat const *btn);

static float ctrl_get_joystick_axis_state(struct host_joystick_btn const *btn);
static float ctrl_get_kbd_axis_state(struct host_kbd_ctrl const *btn);
static float ctrl_get_axis_axis_state(struct host_joystick_axis const *btn);
static float
ctrl_get_joystick_hat_axis_state(struct host_joystick_hat const *btn);

static bool ctrl_get_gamepad_button_state(struct host_gamepad_btn const *btn);
static float
ctrl_get_gamepad_button_axis_state(struct host_gamepad_btn const *btn);
static float
ctrl_get_gamepad_axis_axis_state(struct host_gamepad_axis const *axis);
static bool
ctrl_get_gamepad_axis_button_state(struct host_gamepad_axis const *axis);

int ctrl_parse_bind(char const *bindstr, struct host_ctrl_bind *bind);

static int get_glfw3_key(char const *keystr, int *key);

void ctrl_bind_init(void) {
}

void ctrl_bind_cleanup(void) {
    bind_state.bind_list.clear();
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
    return -1.0f;
 }

struct host_ctrl_bind *ctrl_get_bind(char const name[CTRL_BIND_NAME_LEN]) {
    for (ctrl_bind &bind : bind_state.bind_list) {
        if (strcmp(bind.name, name) == 0)
            return &bind.host;
    }
    return NULL;
}

void
ctrl_bind_key(char const bind[CTRL_BIND_NAME_LEN], struct host_ctrl_bind key) {
    struct ctrl_bind node;

    strncpy(node.name, bind, CTRL_BIND_NAME_LEN);
    node.name[CTRL_BIND_NAME_LEN - 1] = '\0';

    node.host = key;

    bind_state.bind_list.push_front(node);
}

bool ctrl_get_bind_button_state(struct host_ctrl_bind const *key) {
    switch(key->tp) {
    case HOST_CTRL_TP_JOYSTICK_BTN:
        return ctrl_get_joystick_button_state(&key->ctrl.joystick);
    case HOST_CTRL_TP_KBD:
        return ctrl_get_kbd_button_state(&key->ctrl.kbd);
    case HOST_CTRL_TP_JOYSTICK_AXIS:
        return ctrl_get_axis_button_state(&key->ctrl.axis);
    case HOST_CTRL_TP_JOYSTICK_HAT:
        return ctrl_get_joystick_hat_state(&key->ctrl.hat);
    case HOST_CTRL_TP_GAMEPAD_BTN:
        return ctrl_get_gamepad_button_state(&key->ctrl.gp_btn);
    case HOST_CTRL_TP_GAMEPAD_AXIS:
        return ctrl_get_gamepad_axis_button_state(&key->ctrl.gp_axis);
    default:
        return false;
    }
}

float ctrl_get_axis_state(struct host_ctrl_bind const *axis) {
    switch (axis->tp) {
    case HOST_CTRL_TP_JOYSTICK_BTN:
        return ctrl_get_joystick_axis_state(&axis->ctrl.joystick);
    case HOST_CTRL_TP_KBD:
        return ctrl_get_kbd_axis_state(&axis->ctrl.kbd);
    case HOST_CTRL_TP_JOYSTICK_AXIS:
        return ctrl_get_axis_axis_state(&axis->ctrl.axis);
    case HOST_CTRL_TP_JOYSTICK_HAT:
        return ctrl_get_joystick_hat_axis_state(&axis->ctrl.hat);
    case HOST_CTRL_TP_GAMEPAD_BTN:
        return ctrl_get_gamepad_button_axis_state(&axis->ctrl.gp_btn);
    case HOST_CTRL_TP_GAMEPAD_AXIS:
        return ctrl_get_gamepad_axis_axis_state(&axis->ctrl.gp_axis);
    default:
        return -1.0f;
    }
}

static bool ctrl_get_joystick_button_state(struct host_joystick_btn const *btn) {
    int len;
    int btn_idx = btn->btn;
    const unsigned char *joystick_state = glfwGetJoystickButtons(btn->js, &len);
    if (joystick_state && len > btn_idx)
        return joystick_state[btn_idx] == GLFW_PRESS;
    return false;
}

static bool ctrl_get_kbd_button_state(struct host_kbd_ctrl const *btn) {
    return glfwGetKey(btn->win, btn->key) == GLFW_PRESS;
}

static bool ctrl_get_gamepad_button_state(struct host_gamepad_btn const *btn) {
    unsigned btn_idx = btn->btn;
    GLFWgamepadstate gp_state;
    if ((btn_idx <= GLFW_GAMEPAD_BUTTON_LAST) &&
        glfwGetGamepadState(btn->js, &gp_state)) {
        return gp_state.buttons[btn_idx] == GLFW_PRESS;
    }
    return false;
}

static bool ctrl_get_joystick_hat_state(struct host_joystick_hat const *btn) {
    int len;
    int hat_idx = btn->hat;
    const unsigned char *hat_state = glfwGetJoystickHats(btn->js, &len);
    if (hat_state && len > hat_idx)
        return hat_state[hat_idx] & btn->mask ? true : false;
    return false;
}

#define AXIS_BUTTON_THRESH 0.5f

static bool ctrl_get_axis_button_state(struct host_joystick_axis const *btn) {
    int axis_cnt;
    const float *axis_state = glfwGetJoystickAxes(btn->js, &axis_cnt);

    if (axis_state && axis_cnt > btn->axis_no) {
        if (btn->sign >= 0)
            return axis_state[btn->axis_no] > AXIS_BUTTON_THRESH;
        else if (btn->sign < 0)
            return axis_state[btn->axis_no] < -AXIS_BUTTON_THRESH;
    }
    return false;
}

static bool
ctrl_get_gamepad_axis_button_state(struct host_gamepad_axis const *axis) {
    int axis_no = axis->axis_no;
    GLFWgamepadstate gp_state;

    if ((axis_no <= GLFW_GAMEPAD_AXIS_LAST) &&
        glfwGetGamepadState(axis->js, &gp_state)) {
        if (axis->sign >= 0) {
            return gp_state.axes[axis_no] > AXIS_BUTTON_THRESH;
        } else if (axis->sign < 0) {
            return gp_state.axes[axis_no] < -AXIS_BUTTON_THRESH;
        }
    }
    return false;
}

static float ctrl_get_joystick_axis_state(struct host_joystick_btn const *btn) {
    int len;
    int btn_idx = btn->btn;
    const unsigned char *joystick_state = glfwGetJoystickButtons(btn->js, &len);
    if (joystick_state && len > btn_idx && joystick_state[btn_idx] == GLFW_PRESS)
        return 1.0f;
    return -1.0f;
}

static float
ctrl_get_gamepad_button_axis_state(struct host_gamepad_btn const *btn) {
    unsigned btn_idx = btn->btn;
    GLFWgamepadstate gp_state;
    if ((btn_idx <= GLFW_GAMEPAD_BUTTON_LAST) &&
        glfwGetGamepadState(btn->js, &gp_state) &&
        (gp_state.buttons[btn_idx] == GLFW_PRESS)) {
        return 1.0f;
    }
    return -1.0f;
}


static float ctrl_get_kbd_axis_state(struct host_kbd_ctrl const *btn) {
    if (glfwGetKey(btn->win, btn->key) == GLFW_PRESS)
        return 1.0f;
    return -1.0f;
}

static float ctrl_get_axis_axis_state(struct host_joystick_axis const *btn) {
    int axis_cnt;
    const float *axis_state = glfwGetJoystickAxes(btn->js, &axis_cnt);

    if (axis_state && axis_cnt > btn->axis_no) {
        if (btn->sign >= 0) {
                return axis_state[btn->axis_no];
        } else if (btn->sign < 0) {
                return -axis_state[btn->axis_no];
        }
    }
    return -1.0f;
}

static float
ctrl_get_gamepad_axis_axis_state(struct host_gamepad_axis const *axis) {
    int axis_no = axis->axis_no;
    GLFWgamepadstate gp_state;

    if ((axis_no <= GLFW_GAMEPAD_AXIS_LAST) &&
        glfwGetGamepadState(axis->js, &gp_state)) {
        if (axis->sign >= 0) {
            return gp_state.axes[axis_no];
        } else if (axis->sign < 0) {
            return -gp_state.axes[axis_no];
        }
    }
    return -1.0f;
}

static float
ctrl_get_joystick_hat_axis_state(struct host_joystick_hat const *btn) {
    int len;
    int hat_idx = btn->hat;
    const unsigned char *hat_state = glfwGetJoystickHats(btn->js, &len);
    if (hat_state && len > hat_idx && (hat_state[hat_idx] & btn->mask))
        return 1.0f;
    return -1.0f;
}

#define BINDSTR_COMPONENT_MAX 16

int ctrl_parse_bind(char const *bindstr, struct host_ctrl_bind *bind) {
    bool have_dev = false;
    char dev[BINDSTR_COMPONENT_MAX] = { 0 };
    unsigned dev_len = 0;

    // first get device

    while (*bindstr) {
        if (*bindstr == '.') {
            bindstr++;
            have_dev = true;
            break;
        }
        if (dev_len >= BINDSTR_COMPONENT_MAX - 1)
            return -1;
        dev[dev_len++] = *bindstr++;
    }

    if (!have_dev)
        return -1;

    dev[dev_len] = '\0';

    if (dev_len == 3 && strcmp(dev, "kbd") == 0) {
        // we have a keyboard binding
        int keyval;
        if (get_glfw3_key(bindstr, &keyval) != 0)
            return -1;
        bind->tp = HOST_CTRL_TP_KBD;
        bind->ctrl.kbd.key = keyval;
        bind->ctrl.kbd.win = NULL; // caller needs to fill that in himself
        return 0;
    }

    if (dev_len == 3 && dev[0] == 'j' && dev[1] == 's' && isdigit(dev[2])) {
        int jsno = dev[2] - '0';

        // have a joystick binding - either an axis or a button or a hat
        bool have_intf = false;
        char intf[BINDSTR_COMPONENT_MAX] = { 0 };
        unsigned intf_len = 0;

        while (*bindstr) {
            have_intf = true;
            if (*bindstr == '.') {
                bindstr++;
                break;
            }
            if (intf_len >= BINDSTR_COMPONENT_MAX - 1)
                return -1;
            intf[intf_len++] = *bindstr++;
        }

        if (!have_intf)
            return -1;

        intf[intf_len] = '\0';

        if (intf_len == 4 && intf[0] == 'b' && intf[1] == 't' &&
            intf[2] == 'n' && isdigit(intf[3])) {
            bind->tp = HOST_CTRL_TP_JOYSTICK_BTN;
            bind->ctrl.joystick.js = jsno;
            bind->ctrl.joystick.btn = intf[3] - '0';
            return 0;
        } else if ((intf_len == 5 && intf[0] == 'a' && intf[1] == 'x' &&
                    intf[2] == 'i' && intf[3] == 's' && isdigit(intf[4])) ||
                   (intf_len == 6 && intf[0] == 'a' && intf[1] == 'x' &&
                    intf[2] == 'i' && intf[3] == 's' && isdigit(intf[4]) &&
                    (intf[5] == '+' || intf[5] == '-'))) {
            // axis
            int sign;
            if (intf_len == 5)
                sign = 0;
            else if (intf_len == 6 && intf[5] == '+')
                sign = 1;
            else if (intf_len == 6 && intf[5] == '-')
                sign = -1;
            else
                return -1;

            int axis = intf[4] - '0';

            bind->tp = HOST_CTRL_TP_JOYSTICK_AXIS;
            bind->ctrl.axis.js = jsno;
            bind->ctrl.axis.axis_no = axis;
            bind->ctrl.axis.sign = sign;
            return 0;
        } else if (intf_len == 4 && intf[0] == 'h' && intf[1] == 'a' &&
                   intf[2] == 't' && isdigit(intf[3])) {
            char dir[BINDSTR_COMPONENT_MAX] = { 0 };
            bool have_dir = false;
            unsigned dir_len = 0;

            while (*bindstr) {
                have_dir = true;
                if (*bindstr == '.') {
                    bindstr++;
                    break;
                }
                if (dir_len >= BINDSTR_COMPONENT_MAX - 1)
                    return -1;
                dir[dir_len++] = *bindstr++;
            }

            if (!have_dir)
                return false;

            if (strcmp(dir, "up") == 0)
                bind->ctrl.hat.mask = GLFW_HAT_UP;
            else if (strcmp(dir, "left") == 0)
                bind->ctrl.hat.mask = GLFW_HAT_LEFT;
            else if (strcmp(dir, "down") == 0)
                bind->ctrl.hat.mask = GLFW_HAT_DOWN;
            else if (strcmp(dir, "right") == 0)
                bind->ctrl.hat.mask = GLFW_HAT_RIGHT;
            else
                return -1;

            bind->tp = HOST_CTRL_TP_JOYSTICK_HAT;
            bind->ctrl.hat.js = jsno;
            bind->ctrl.hat.hat = intf[3] - '0';
            return 0;
        }

        return -1;
    } else if (dev_len == 3 && dev[0] == 'g' && dev[1] == 'p' && isdigit(dev[2])) {
        int jsno = dev[2] - '0';

        // have a gamepad binding - either an axis or a button
        bool have_intf = false;
        char intf[BINDSTR_COMPONENT_MAX] = { 0 };
        unsigned intf_len = 0;

        while (*bindstr) {
            have_intf = true;
            if (*bindstr == '.') {
                bindstr++;
                break;
            }
            if (intf_len >= BINDSTR_COMPONENT_MAX - 1)
                return -1;
            intf[intf_len++] = *bindstr++;
        }

        if (!have_intf)
            return -1;

        intf[intf_len] = '\0';

        if (strcmp(intf, "btn") == 0) {
            bool have_btn = false;
            char btn[BINDSTR_COMPONENT_MAX] = { 0 };
            unsigned btn_len = 0;

            while (*bindstr) {
                have_btn = true;
                if (btn_len >= BINDSTR_COMPONENT_MAX - 1)
                    return -1;
                btn[btn_len++] = toupper(*bindstr++);
            }

            if (!have_btn)
                return -1;
            btn[btn_len] = '\0';

            unsigned button;
            if (strcmp(btn, "A") == 0)
                button = GLFW_GAMEPAD_BUTTON_A;
            else if (strcmp(btn, "B") == 0)
                button = GLFW_GAMEPAD_BUTTON_B;
            else if (strcmp(btn, "X") == 0)
                button = GLFW_GAMEPAD_BUTTON_X;
            else if (strcmp(btn, "Y") == 0)
                button = GLFW_GAMEPAD_BUTTON_Y;
            else if (strcmp(btn, "L1") == 0)
                button = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
            else if (strcmp(btn, "R1") == 0)
                button = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
            else if (strcmp(btn, "BACK") == 0)
                button = GLFW_GAMEPAD_BUTTON_BACK;
            else if (strcmp(btn, "START") == 0)
                button = GLFW_GAMEPAD_BUTTON_START;
            else if (strcmp(btn, "GUIDE") == 0)
                button = GLFW_GAMEPAD_BUTTON_GUIDE;
            else if (strcmp(btn, "L3") == 0)
                button = GLFW_GAMEPAD_BUTTON_LEFT_THUMB;
            else if (strcmp(btn, "R3") == 0)
                button = GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;
            else if (strcmp(btn, "UP") == 0)
                button = GLFW_GAMEPAD_BUTTON_DPAD_UP;
            else if (strcmp(btn, "RIGHT") == 0)
                button = GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
            else if (strcmp(btn, "DOWN") == 0)
                button = GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
            else if (strcmp(btn, "LEFT") == 0)
                button = GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
            else
                return -1;

            bind->tp = HOST_CTRL_TP_GAMEPAD_BTN;
            bind->ctrl.gp_btn.js = jsno;
            bind->ctrl.gp_btn.btn = button;
            return 0;
        } else if (strcmp(intf, "axis") == 0) {
            bool have_axis = false;
            char axis[BINDSTR_COMPONENT_MAX] = { 0 };
            unsigned axis_len = 0;
            int sign;
            unsigned axis_no;

            while (*bindstr && *bindstr != '+' && *bindstr != '-') {
                have_axis = true;
                if (axis_len >= BINDSTR_COMPONENT_MAX - 1)
                    return -1;
                axis[axis_len++] = toupper(*bindstr++);
            }

            if (!have_axis)
                return -1;
            axis[axis_len] = '\0';

            if (*bindstr == '-')
                sign = -1;
            else if (*bindstr == '+')
                sign = 1;
            else
                sign = 0;

            if (strcmp(axis, "LHOR") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_LEFT_X;
            else if (strcmp(axis, "LVERT") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_LEFT_Y;
            else if (strcmp(axis, "RHOR") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_RIGHT_X;
            else if (strcmp(axis, "RVERT") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_RIGHT_Y;
            else if (strcmp(axis, "L2") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
            else if (strcmp(axis, "R2") == 0)
                axis_no = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
            else
                return -1;

            bind->tp = HOST_CTRL_TP_GAMEPAD_AXIS;
            bind->ctrl.gp_axis.js = jsno;
            bind->ctrl.gp_axis.axis_no = axis_no;
            bind->ctrl.gp_axis.sign = sign;
            return 0;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

static struct keystr_map {
    char const *str;
    int keyval;
} keystr_map[] = {
    { "space", GLFW_KEY_SPACE },
    { "singlequote", GLFW_KEY_APOSTROPHE },
    { "comma", GLFW_KEY_COMMA },
    { "minus", GLFW_KEY_MINUS },
    { "dot", GLFW_KEY_PERIOD },
    { "slash", GLFW_KEY_SLASH },
    { "0", GLFW_KEY_0 },
    { "1", GLFW_KEY_1 },
    { "2", GLFW_KEY_2 },
    { "3", GLFW_KEY_3 },
    { "4", GLFW_KEY_4 },
    { "5", GLFW_KEY_5 },
    { "6", GLFW_KEY_6 },
    { "7", GLFW_KEY_7 },
    { "8", GLFW_KEY_8 },
    { "9", GLFW_KEY_9 },
    { "semicolon", GLFW_KEY_SEMICOLON },
    { "equal", GLFW_KEY_EQUAL },
    { "a", GLFW_KEY_A },
    { "b", GLFW_KEY_B },
    { "c", GLFW_KEY_C },
    { "d", GLFW_KEY_D },
    { "e", GLFW_KEY_E },
    { "f", GLFW_KEY_F },
    { "g", GLFW_KEY_G },
    { "h", GLFW_KEY_H },
    { "i", GLFW_KEY_I },
    { "j", GLFW_KEY_J },
    { "k", GLFW_KEY_K },
    { "l", GLFW_KEY_L },
    { "m", GLFW_KEY_M },
    { "n", GLFW_KEY_N },
    { "o", GLFW_KEY_O },
    { "p", GLFW_KEY_P },
    { "q", GLFW_KEY_Q },
    { "r", GLFW_KEY_R },
    { "s", GLFW_KEY_S },
    { "t", GLFW_KEY_T },
    { "u", GLFW_KEY_U },
    { "v", GLFW_KEY_V },
    { "w", GLFW_KEY_W },
    { "x", GLFW_KEY_X },
    { "y", GLFW_KEY_Y },
    { "z", GLFW_KEY_Z },
    { "leftbrace", GLFW_KEY_LEFT_BRACKET },
    { "backslash", GLFW_KEY_BACKSLASH },
    { "rightbrace", GLFW_KEY_RIGHT_BRACKET },
    { "backquote", GLFW_KEY_GRAVE_ACCENT },
    { "world1", GLFW_KEY_WORLD_1 },
    { "world2", GLFW_KEY_WORLD_2 },
    { "escape", GLFW_KEY_ESCAPE },
    { "enter", GLFW_KEY_ENTER },
    { "tab", GLFW_KEY_TAB },
    { "backspace", GLFW_KEY_BACKSPACE },
    { "insert", GLFW_KEY_INSERT },
    { "delete", GLFW_KEY_DELETE },
    { "right", GLFW_KEY_RIGHT },
    { "left", GLFW_KEY_LEFT },
    { "down", GLFW_KEY_DOWN },
    { "up", GLFW_KEY_UP },
    { "pageup", GLFW_KEY_PAGE_UP },
    { "pagedown", GLFW_KEY_PAGE_DOWN },
    { "home", GLFW_KEY_HOME },
    { "end", GLFW_KEY_END },
    { "capslock", GLFW_KEY_CAPS_LOCK },
    { "scrolllock", GLFW_KEY_SCROLL_LOCK },
    { "numlock", GLFW_KEY_NUM_LOCK },
    { "printscreen", GLFW_KEY_PRINT_SCREEN },
    { "pause", GLFW_KEY_PAUSE },
    { "f1", GLFW_KEY_F1 },
    { "f2", GLFW_KEY_F2 },
    { "f3", GLFW_KEY_F3 },
    { "f4", GLFW_KEY_F4 },
    { "f5", GLFW_KEY_F5 },
    { "f6", GLFW_KEY_F6 },
    { "f7", GLFW_KEY_F7 },
    { "f8", GLFW_KEY_F8 },
    { "f9", GLFW_KEY_F9 },
    { "f10", GLFW_KEY_F10 },
    { "f11", GLFW_KEY_F11 },
    { "f12", GLFW_KEY_F12 },
    { "f13", GLFW_KEY_F13 },
    { "f14", GLFW_KEY_F14 },
    { "f15", GLFW_KEY_F15 },
    { "f16", GLFW_KEY_F16 },
    { "f17", GLFW_KEY_F17 },
    { "f18", GLFW_KEY_F18 },
    { "f19", GLFW_KEY_F19 },
    { "f20", GLFW_KEY_F20 },
    { "f21", GLFW_KEY_F21 },
    { "f22", GLFW_KEY_F22 },
    { "f23", GLFW_KEY_F23 },
    { "f24", GLFW_KEY_F24 },
    { "f25", GLFW_KEY_F25 },
    { "keypad0", GLFW_KEY_KP_0 },
    { "keypad1", GLFW_KEY_KP_1 },
    { "keypad2", GLFW_KEY_KP_2 },
    { "keypad3", GLFW_KEY_KP_3 },
    { "keypad4", GLFW_KEY_KP_4 },
    { "keypad5", GLFW_KEY_KP_5 },
    { "keypad6", GLFW_KEY_KP_6 },
    { "keypad7", GLFW_KEY_KP_7 },
    { "keypad8", GLFW_KEY_KP_8 },
    { "keypad9", GLFW_KEY_KP_9 },
    { "keypaddot", GLFW_KEY_KP_DECIMAL },
    { "keypadslash", GLFW_KEY_KP_DIVIDE },
    { "keypadasterisk", GLFW_KEY_KP_MULTIPLY },
    { "keypadminus", GLFW_KEY_KP_SUBTRACT },
    { "keypadplus", GLFW_KEY_KP_ADD },
    { "keypadenter", GLFW_KEY_KP_ENTER },
    { "keypadequal", GLFW_KEY_KP_EQUAL },
    { "lshift", GLFW_KEY_LEFT_SHIFT },
    { "lctrl", GLFW_KEY_LEFT_CONTROL },
    { "lalt", GLFW_KEY_LEFT_ALT },
    { "lsuper", GLFW_KEY_LEFT_SUPER },
    { "rshift", GLFW_KEY_RIGHT_SHIFT },
    { "rctrl", GLFW_KEY_RIGHT_CONTROL },
    { "ralt", GLFW_KEY_RIGHT_ALT },
    { "rsuper", GLFW_KEY_RIGHT_SUPER },
    { "menu", GLFW_KEY_MENU },

    { NULL }
};

static int get_glfw3_key(char const *keystr, int *key) {
    struct keystr_map *curs = keystr_map;
    while (curs->str) {
        if (strcmp(curs->str, keystr) == 0) {
            *key = curs->keyval;
            return 0;
        }
        curs++;
    }
    return -1;
}
