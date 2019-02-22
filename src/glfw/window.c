/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <err.h>
#include <ctype.h>
#include <stdbool.h>

#include <GLFW/glfw3.h>

#include "gfx/opengl/opengl_output.h"
#include "dreamcast.h"
#include "hw/maple/maple_controller.h"
#include "gfx/gfx.h"
#include "title.h"
#include "config_file.h"
#include "control_bind.h"

#include "window.h"

static unsigned res_x, res_y;
static GLFWwindow *win;

static void expose_callback(GLFWwindow *win);
static void resize_callback(GLFWwindow *win, int width, int height);
static void scan_input(void);

static int bind_ctrl_from_cfg(char const *name, char const *cfg_node) {
    char const *bindstr = cfg_get_node(cfg_node);
    if (!bindstr)
        return -1;
    struct host_ctrl_bind bind;
    int err;
    if ((err = ctrl_parse_bind(bindstr, &bind)) < 0) {
        return err;
    }
    if (bind.tp == HOST_CTRL_TP_KBD) {
        bind.ctrl.kbd.win = win;
        ctrl_bind_key(name, bind);
        return 0;
    } else if (bind.tp == HOST_CTRL_TP_GAMEPAD) {
        bind.ctrl.gamepad.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    } else if (bind.tp == HOST_CTRL_TP_AXIS) {
        bind.ctrl.gamepad.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    } else if (bind.tp == HOST_CTRL_TP_HAT) {
        bind.ctrl.gamepad.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    }

    return -1;
}

void win_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    if (!glfwInit())
        err(1, "unable to initialize glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    win = glfwCreateWindow(res_x, res_y, title_get(), NULL, NULL);

    if (!win)
        errx(1, "unable to create window");

    glfwSetWindowRefreshCallback(win, expose_callback);
    glfwSetFramebufferSizeCallback(win, resize_callback);

    bool vsync_en = false;
    if (cfg_get_bool("win.vsync", &vsync_en) == 0 && vsync_en) {
        LOG_INFO("vsync enabled\n");
        glfwSwapInterval(1);
    } else {
        LOG_INFO("vsync disabled\n");
        glfwSwapInterval(0);
    }

    ctrl_bind_init();

    // configure default keybinds
    bind_ctrl_from_cfg("toggle-overlay", "wash.ctrl.toggle-overlay");

    bind_ctrl_from_cfg("p1_1.dpad-up", "dc.ctrl.p1_1.dpad-up");
    bind_ctrl_from_cfg("p1_1.dpad-left", "dc.ctrl.p1_1.dpad-left");
    bind_ctrl_from_cfg("p1_1.dpad-down", "dc.ctrl.p1_1.dpad-down");
    bind_ctrl_from_cfg("p1_1.dpad-right", "dc.ctrl.p1_1.dpad-right");
    bind_ctrl_from_cfg("p1_1.btn_a", "dc.ctrl.p1_1.btn-a");
    bind_ctrl_from_cfg("p1_1.btn_b", "dc.ctrl.p1_1.btn-b");
    bind_ctrl_from_cfg("p1_1.btn_x", "dc.ctrl.p1_1.btn-x");
    bind_ctrl_from_cfg("p1_1.btn_y", "dc.ctrl.p1_1.btn-y");
    bind_ctrl_from_cfg("p1_1.btn_start", "dc.ctrl.p1_1.btn-start");
    bind_ctrl_from_cfg("p1_1.stick-left", "dc.ctrl.p1_1.stick-left");
    bind_ctrl_from_cfg("p1_1.stick-right", "dc.ctrl.p1_1.stick-right");
    bind_ctrl_from_cfg("p1_1.stick-up", "dc.ctrl.p1_1.stick-up");
    bind_ctrl_from_cfg("p1_1.stick-down", "dc.ctrl.p1_1.stick-down");
    bind_ctrl_from_cfg("p1_1.trig-l", "dc.ctrl.p1_1.trig-l");
    bind_ctrl_from_cfg("p1_1.trig-r", "dc.ctrl.p1_1.trig-r");

    /*
     * p1_1 and p1_2 both refer to the same buttons on player 1's controller.
     * It's there to provide a way to have two different bindings for the same
     * button.
     */
    bind_ctrl_from_cfg("p1_2.dpad-up", "dc.ctrl.p1_2.dpad-up");
    bind_ctrl_from_cfg("p1_2.dpad-left", "dc.ctrl.p1_2.dpad-left");
    bind_ctrl_from_cfg("p1_2.dpad-down", "dc.ctrl.p1_2.dpad-down");
    bind_ctrl_from_cfg("p1_2.dpad-right", "dc.ctrl.p1_2.dpad-right");
    bind_ctrl_from_cfg("p1_2.btn_a", "dc.ctrl.p1_2.btn-a");
    bind_ctrl_from_cfg("p1_2.btn_b", "dc.ctrl.p1_2.btn-b");
    bind_ctrl_from_cfg("p1_2.btn_x", "dc.ctrl.p1_2.btn-x");
    bind_ctrl_from_cfg("p1_2.btn_y", "dc.ctrl.p1_2.btn-y");
    bind_ctrl_from_cfg("p1_2.btn_start", "dc.ctrl.p1_2.btn-start");
    bind_ctrl_from_cfg("p1_2.stick-left", "dc.ctrl.p1_2.stick-left");
    bind_ctrl_from_cfg("p1_2.stick-right", "dc.ctrl.p1_2.stick-right");
    bind_ctrl_from_cfg("p1_2.stick-up", "dc.ctrl.p1_2.stick-up");
    bind_ctrl_from_cfg("p1_2.stick-down", "dc.ctrl.p1_2.stick-down");
    bind_ctrl_from_cfg("p1_2.trig-l", "dc.ctrl.p1_2.trig-l");
    bind_ctrl_from_cfg("p1_2.trig-r", "dc.ctrl.p1_2.trig-r");
}

void win_cleanup() {
    ctrl_bind_cleanup();

    glfwTerminate();
}

void win_check_events(void) {
    glfwPollEvents();

    scan_input();

    if (glfwWindowShouldClose(win))
        dreamcast_kill();
}

void win_update() {
    glfwSwapBuffers(win);
}

static void expose_callback(GLFWwindow *win) {
    gfx_expose();
}

enum gamepad_btn {
    GAMEPAD_BTN_A = 0,
    GAMEPAD_BTN_B = 1,
    GAMEPAD_BTN_X = 2,
    GAMEPAD_BTN_Y = 3,
    GAMEPAD_BTN_START = 7,

    GAMEPAD_BTN_COUNT
};

enum gamepad_hat {
    GAMEPAD_HAT_UP,
    GAMEPAD_HAT_DOWN,
    GAMEPAD_HAT_LEFT,
    GAMEPAD_HAT_RIGHT,

    GAMEPAD_HAT_COUNT
};

static void scan_input(void) {
    bool btns[GAMEPAD_BTN_COUNT];
    bool hat[GAMEPAD_HAT_COUNT];

    float trig_l_real_1 = ctrl_get_axis("p1_1.trig-l") + 1.0f;
    float trig_l_real_2 = ctrl_get_axis("p1_2.trig-l") + 1.0f;
    float trig_l_real = trig_l_real_1 + trig_l_real_2;
    if (trig_l_real < 0.0f)
        trig_l_real = 0.0f;
    else if (trig_l_real > 1.0f)
        trig_l_real = 1.0f;

    float trig_r_real_1 = ctrl_get_axis("p1_1.trig-r") + 1.0f;
    float trig_r_real_2 = ctrl_get_axis("p1_2.trig-r") + 1.0f;
    float trig_r_real = trig_r_real_1 + trig_r_real_2;
    if (trig_r_real < 0.0f)
        trig_r_real = 0.0f;
    else if (trig_r_real > 1.0f)
        trig_r_real = 1.0f;

    int trig_l = trig_l_real * 255;
    int trig_r = trig_r_real * 255;

    float stick_up_real_1 = ctrl_get_axis("p1_1.stick-up");
    float stick_down_real_1 = ctrl_get_axis("p1_1.stick-down");
    float stick_left_real_1 = ctrl_get_axis("p1_1.stick-left");
    float stick_right_real_1 = ctrl_get_axis("p1_1.stick-right");
    float stick_up_real_2 = ctrl_get_axis("p1_2.stick-up");
    float stick_down_real_2 = ctrl_get_axis("p1_2.stick-down");
    float stick_left_real_2 = ctrl_get_axis("p1_2.stick-left");
    float stick_right_real_2 = ctrl_get_axis("p1_2.stick-right");

    if (stick_up_real_1 < 0.0f)
        stick_up_real_1 = 0.0f;
    if (stick_up_real_2 < 0.0f)
        stick_up_real_2 = 0.0f;
    if (stick_down_real_1 < 0.0f)
        stick_down_real_1 = 0.0f;
    if (stick_down_real_2 < 0.0f)
        stick_down_real_2 = 0.0f;
    if (stick_left_real_1 < 0.0f)
        stick_left_real_1 = 0.0f;
    if (stick_left_real_2 < 0.0f)
        stick_left_real_2 = 0.0f;
    if (stick_right_real_1 < 0.0f)
        stick_right_real_1 = 0.0f;
    if (stick_right_real_2 < 0.0f)
        stick_right_real_2 = 0.0f;

    float stick_up = stick_up_real_1 + stick_up_real_2;
    float stick_down = stick_down_real_1 + stick_down_real_2;
    float stick_left = stick_left_real_1 + stick_left_real_2;
    float stick_right = stick_right_real_1 + stick_right_real_2;

    if (stick_up < 0.0f)
        stick_up = 0.0f;
    if (stick_down < 0.0f)
        stick_down = 0.0f;
    if (stick_left < 0.0f)
        stick_left = 0.0f;
    if (stick_right < 0.0f)
        stick_right = 0.0f;
    if (stick_up > 1.0f)
        stick_up = 1.0f;
    if (stick_down > 1.0f)
        stick_down = 1.0f;
    if (stick_left > 1.0f)
        stick_left = 1.0f;
    if (stick_right > 1.0f)
        stick_right = 1.0f;

    int stick_vert = (stick_down - stick_up) * 128 + 128;
    int stick_hor = (stick_right - stick_left) * 128 + 128;

    if (stick_hor > 255)
        stick_hor = 255;
    if (stick_hor < 0)
        stick_hor = 0;
    if (stick_vert > 255)
        stick_vert = 255;
    if (stick_vert < 0)
        stick_vert = 0;
    if (trig_l > 255)
        trig_l = 255;
    if (trig_l < 0)
        trig_l = 0;
    if (trig_r > 255)
        trig_r = 255;
    if (trig_r < 0)
        trig_r = 0;

    btns[GAMEPAD_BTN_A] = ctrl_get_button("p1_1.btn_a") ||
        ctrl_get_button("p1_2.btn_a");
    btns[GAMEPAD_BTN_B] = ctrl_get_button("p1_1.btn_b") ||
        ctrl_get_button("p1_2.btn_b");
    btns[GAMEPAD_BTN_X] = ctrl_get_button("p1_1.btn_x") ||
        ctrl_get_button("p1_2.btn_x");
    btns[GAMEPAD_BTN_Y] = ctrl_get_button("p1_1.btn_y") ||
        ctrl_get_button("p1_2.btn_y");
    btns[GAMEPAD_BTN_START] = ctrl_get_button("p1_1.btn_start") ||
        ctrl_get_button("p1_2.btn_start");

    hat[GAMEPAD_HAT_UP] = ctrl_get_button("p1_1.dpad-up") ||
        ctrl_get_button("p1_2.dpad-up");
    hat[GAMEPAD_HAT_DOWN] = ctrl_get_button("p1_1.dpad-down") ||
        ctrl_get_button("p1_2.dpad-down");
    hat[GAMEPAD_HAT_LEFT] = ctrl_get_button("p1_1.dpad-left") ||
        ctrl_get_button("p1_2.dpad-left");
    hat[GAMEPAD_HAT_RIGHT] = ctrl_get_button("p1_1.dpad-right") ||
        ctrl_get_button("p1_2.dpad-right");

    if (btns[GAMEPAD_BTN_A])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_A_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_A_MASK);
    if (btns[GAMEPAD_BTN_B])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_B_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_B_MASK);
    if (btns[GAMEPAD_BTN_X])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_X_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_X_MASK);
    if (btns[GAMEPAD_BTN_Y])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_Y_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_Y_MASK);
    if (btns[GAMEPAD_BTN_START])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_START_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_START_MASK);

    if (hat[GAMEPAD_HAT_UP])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_DPAD_UP_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_DPAD_UP_MASK);
    if (hat[GAMEPAD_HAT_DOWN])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_DPAD_DOWN_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_DPAD_DOWN_MASK);
    if (hat[GAMEPAD_HAT_LEFT])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_DPAD_LEFT_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_DPAD_LEFT_MASK);
    if (hat[GAMEPAD_HAT_RIGHT])
        maple_controller_press_btns(0, MAPLE_CONT_BTN_DPAD_RIGHT_MASK);
    else
        maple_controller_release_btns(0, MAPLE_CONT_BTN_DPAD_RIGHT_MASK);

    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_R_TRIG, trig_r);
    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_L_TRIG, trig_l);
    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_JOY1_X, stick_hor);
    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_JOY1_Y, stick_vert);
    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_JOY2_X, 0);
    maple_controller_set_axis(0, MAPLE_CONTROLLER_AXIS_JOY2_Y, 0);

    // Allow the user to toggle the overlay by pressing F2
    static bool overlay_key_prev = false;
    bool overlay_key = ctrl_get_button("toggle-overlay");
    if (overlay_key && !overlay_key_prev)
        dc_toggle_overlay();
    overlay_key_prev = overlay_key;
}

void win_make_context_current(void) {
    glfwMakeContextCurrent(win);
}

void win_update_title(void) {
    glfwSetWindowTitle(win, title_get());
}

static void resize_callback(GLFWwindow *win, int width, int height) {
    res_x = width;
    res_y = height;
    gfx_resize(width, height);
}

int win_get_width(void) {
    return res_x;
}

int win_get_height(void) {
    return res_y;
}
