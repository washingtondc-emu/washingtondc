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
    ctrl_bind_kbd_key("toggle-overlay", win, GLFW_KEY_F2);

    /*
     * TODO: these controller bindings are based on my Logitech F510.  They will
     * be different for other controllers, so there needs to be a custom binding
     * system
     */
    ctrl_bind_gamepad_btn("p1_1.btn_a", GLFW_JOYSTICK_1, 0);
    ctrl_bind_gamepad_btn("p1_1.btn_b", GLFW_JOYSTICK_1, 1);
    ctrl_bind_gamepad_btn("p1_1.btn_x", GLFW_JOYSTICK_1, 2);
    ctrl_bind_gamepad_btn("p1_1.btn_y", GLFW_JOYSTICK_1, 3);
    ctrl_bind_gamepad_btn("p1_1.btn_start", GLFW_JOYSTICK_1, 7);
    ctrl_bind_axis_btn("p1_1.dpad-up", GLFW_JOYSTICK_1, 7, 1);
    ctrl_bind_axis_btn("p1_1.dpad-left", GLFW_JOYSTICK_1, 7, -1);
    ctrl_bind_axis_btn("p1_1.dpad-down", GLFW_JOYSTICK_1, 7, -1);
    ctrl_bind_axis_btn("p1_1.dpad-right", GLFW_JOYSTICK_1, 7, 1);
    ctrl_bind_axis_btn("p1_1.stick-left", GLFW_JOYSTICK_1, 0, -1);
    ctrl_bind_axis_btn("p1_1.stick-right", GLFW_JOYSTICK_1, 0, 1);
    ctrl_bind_axis_btn("p1_1.stick-up", GLFW_JOYSTICK_1, 1, 1);
    ctrl_bind_axis_btn("p1_1.stick-down", GLFW_JOYSTICK_1, 1, -1);
    ctrl_bind_axis_btn("p1_1.trig-l", GLFW_JOYSTICK_1, 2, 0);
    ctrl_bind_axis_btn("p1_1.trig-r", GLFW_JOYSTICK_1, 5, 0);

    /*
     * p1_1 and p1_2 both refer to the same buttons on player 1's controller.
     * It's there to provide a way to have two different bindings for the same
     * button.
     */
    ctrl_bind_kbd_key("p1_2.dpad-up", win, GLFW_KEY_W);
    ctrl_bind_kbd_key("p1_2.dpad-left", win, GLFW_KEY_A);
    ctrl_bind_kbd_key("p1_2.dpad-down", win, GLFW_KEY_S);
    ctrl_bind_kbd_key("p1_2.dpad-right", win, GLFW_KEY_D);
    ctrl_bind_kbd_key("p1_2.btn_a", win, GLFW_KEY_KP_2);
    ctrl_bind_kbd_key("p1_2.btn_b", win, GLFW_KEY_KP_6);
    ctrl_bind_kbd_key("p1_2.btn_x", win, GLFW_KEY_KP_4);
    ctrl_bind_kbd_key("p1_2.btn_y", win, GLFW_KEY_KP_8);
    ctrl_bind_kbd_key("p1_2.btn_start", win, GLFW_KEY_SPACE);
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

    int trig_l = ctrl_get_axis("p1_1.trig-l") * 128 + 128;
    int trig_r = ctrl_get_axis("p1_1.trig-r") * 128 + 128;
    int stick_vert = (ctrl_get_axis("p1_1.stick-up") -
                      ctrl_get_axis("p1_1.stick-down")) * 128 + 128;
    int stick_hor = (ctrl_get_axis("p1_1.stick-right") -
                     ctrl_get_axis("p1_1.stick-left")) * 128 + 128;

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
