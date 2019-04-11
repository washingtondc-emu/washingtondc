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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <GLFW/glfw3.h>

#include "washdc/washdc.h"

#include "washdc/config_file.h"
#include "control_bind.h"

#include "window.h"

static void win_glfw_init(unsigned width, unsigned height);
static void win_glfw_cleanup();
static void win_glfw_check_events(void);
static void win_glfw_update(void);
static void win_glfw_make_context_current(void);
static void win_glfw_update_title(void);
static int win_glfw_get_width(void);
static int win_glfw_get_height(void);


struct win_intf const win_intf_glfw = {
    .init = win_glfw_init,
    .cleanup = win_glfw_cleanup,
    .check_events = win_glfw_check_events,
    .update = win_glfw_update,
    .make_context_current = win_glfw_make_context_current,
    .get_width = win_glfw_get_width,
    .get_height = win_glfw_get_height,
    .update_title = win_glfw_update_title
};

enum win_mode {
    WIN_MODE_WINDOWED,
    WIN_MODE_FULLSCREEN
};

static unsigned res_x, res_y;
static unsigned win_res_x, win_res_y;
static GLFWwindow *win;
static enum win_mode win_mode = WIN_MODE_WINDOWED;

static void expose_callback(GLFWwindow *win);
static void resize_callback(GLFWwindow *win, int width, int height);
static void scan_input(void);
static void toggle_fullscreen(void);

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

static void win_glfw_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    win_res_x = width;
    win_res_y = height;

    if (!glfwInit())
        err(1, "unable to initialize glfw");

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);

    char const *win_mode_str = cfg_get_node("win.window-mode");

    if (win_mode_str) {
        if (strcmp(win_mode_str, "fullscreen") == 0) {
            win_mode = WIN_MODE_FULLSCREEN;
        } else if (strcmp(win_mode_str, "windowed") == 0) {
            win_mode = WIN_MODE_WINDOWED;
        } else {
            fprintf(stderr, "Unrecognized window mode \"%s\" - "
                    "using \"windowed\" mode instead\n", win_mode_str);
            win_mode = WIN_MODE_WINDOWED;
        }
    } else {
        win_mode = WIN_MODE_WINDOWED;
    }

    if (win_mode == WIN_MODE_FULLSCREEN) {
        printf("Enabling fullscreen mode.\n");
        res_x = vidmode->width;
        res_y = vidmode->height;
        win = glfwCreateWindow(res_x, res_y, washdc_win_get_title(),
                               monitor, NULL);
    } else {
        printf("Enabling windowed mode.\n");
        win = glfwCreateWindow(res_x, res_y, washdc_win_get_title(), NULL, NULL);
    }

    if (!win)
        errx(1, "unable to create window");

    glfwSetWindowRefreshCallback(win, expose_callback);
    glfwSetFramebufferSizeCallback(win, resize_callback);

    bool vsync_en = false;
    if (cfg_get_bool("win.vsync", &vsync_en) == 0 && vsync_en) {
        printf("vsync enabled\n");
        glfwSwapInterval(1);
    } else {
        printf("vsync disabled\n");
        glfwSwapInterval(0);
    }

    ctrl_bind_init();

    // configure default keybinds
    bind_ctrl_from_cfg("toggle-overlay", "wash.ctrl.toggle-overlay");
    bind_ctrl_from_cfg("toggle-fullscreen", "wash.ctrl.toggle-fullscreen");
    bind_ctrl_from_cfg("toggle-filter", "wash.ctrl.toggle-filter");
    bind_ctrl_from_cfg("toggle-wireframe", "wash.ctrl.toggle-wireframe");
    bind_ctrl_from_cfg("screenshot", "wash.ctrl.screenshot");

    /*
     * This bind immediately exits the emulator.  It is unbound in the default
     * config because I don't want people pressing it by mistake, but it's good
     * to have around for dev work.
     */
    bind_ctrl_from_cfg("exit-now", "wash.ctrl.exit");

    bind_ctrl_from_cfg("p1_1.dpad-up", "dc.ctrl.p1.dpad-up");
    bind_ctrl_from_cfg("p1_1.dpad-left", "dc.ctrl.p1.dpad-left");
    bind_ctrl_from_cfg("p1_1.dpad-down", "dc.ctrl.p1.dpad-down");
    bind_ctrl_from_cfg("p1_1.dpad-right", "dc.ctrl.p1.dpad-right");
    bind_ctrl_from_cfg("p1_1.btn_a", "dc.ctrl.p1.btn-a");
    bind_ctrl_from_cfg("p1_1.btn_b", "dc.ctrl.p1.btn-b");
    bind_ctrl_from_cfg("p1_1.btn_x", "dc.ctrl.p1.btn-x");
    bind_ctrl_from_cfg("p1_1.btn_y", "dc.ctrl.p1.btn-y");
    bind_ctrl_from_cfg("p1_1.btn_start", "dc.ctrl.p1.btn-start");
    bind_ctrl_from_cfg("p1_1.stick-left", "dc.ctrl.p1.stick-left");
    bind_ctrl_from_cfg("p1_1.stick-right", "dc.ctrl.p1.stick-right");
    bind_ctrl_from_cfg("p1_1.stick-up", "dc.ctrl.p1.stick-up");
    bind_ctrl_from_cfg("p1_1.stick-down", "dc.ctrl.p1.stick-down");
    bind_ctrl_from_cfg("p1_1.trig-l", "dc.ctrl.p1.trig-l");
    bind_ctrl_from_cfg("p1_1.trig-r", "dc.ctrl.p1.trig-r");

    /*
     * p1_1 and p1_2 both refer to the same buttons on player 1's controller.
     * It's there to provide a way to have two different bindings for the same
     * button.
     */
    bind_ctrl_from_cfg("p1_2.dpad-up", "dc.ctrl.p1.dpad-up(1)");
    bind_ctrl_from_cfg("p1_2.dpad-left", "dc.ctrl.p1.dpad-left(1)");
    bind_ctrl_from_cfg("p1_2.dpad-down", "dc.ctrl.p1.dpad-down(1)");
    bind_ctrl_from_cfg("p1_2.dpad-right", "dc.ctrl.p1.dpad-right(1)");
    bind_ctrl_from_cfg("p1_2.btn_a", "dc.ctrl.p1.btn-a(1)");
    bind_ctrl_from_cfg("p1_2.btn_b", "dc.ctrl.p1.btn-b(1)");
    bind_ctrl_from_cfg("p1_2.btn_x", "dc.ctrl.p1.btn-x(1)");
    bind_ctrl_from_cfg("p1_2.btn_y", "dc.ctrl.p1.btn-y(1)");
    bind_ctrl_from_cfg("p1_2.btn_start", "dc.ctrl.p1.btn-start(1)");
    bind_ctrl_from_cfg("p1_2.stick-left", "dc.ctrl.p1.stick-left(1)");
    bind_ctrl_from_cfg("p1_2.stick-right", "dc.ctrl.p1.stick-right(1)");
    bind_ctrl_from_cfg("p1_2.stick-up", "dc.ctrl.p1.stick-up(1)");
    bind_ctrl_from_cfg("p1_2.stick-down", "dc.ctrl.p1.stick-down(1)");
    bind_ctrl_from_cfg("p1_2.trig-l", "dc.ctrl.p1.trig-l(1)");
    bind_ctrl_from_cfg("p1_2.trig-r", "dc.ctrl.p1.trig-r(1)");
}

static void win_glfw_cleanup() {
    ctrl_bind_cleanup();

    glfwTerminate();
}

static void win_glfw_check_events(void) {
    glfwPollEvents();

    scan_input();

    if (glfwWindowShouldClose(win))
        washdc_kill();
}

static void win_glfw_update() {
    glfwSwapBuffers(win);
}

static void expose_callback(GLFWwindow *win) {
    washdc_on_expose();
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
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_A_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_A_MASK);
    if (btns[GAMEPAD_BTN_B])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_B_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_B_MASK);
    if (btns[GAMEPAD_BTN_X])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_X_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_X_MASK);
    if (btns[GAMEPAD_BTN_Y])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_Y_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_Y_MASK);
    if (btns[GAMEPAD_BTN_START])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_START_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_START_MASK);

    if (hat[GAMEPAD_HAT_UP])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_DPAD_UP_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_DPAD_UP_MASK);
    if (hat[GAMEPAD_HAT_DOWN])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_DPAD_DOWN_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_DPAD_DOWN_MASK);
    if (hat[GAMEPAD_HAT_LEFT])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_DPAD_LEFT_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_DPAD_LEFT_MASK);
    if (hat[GAMEPAD_HAT_RIGHT])
        washdc_controller_press_btns(0, WASHDC_CONT_BTN_DPAD_RIGHT_MASK);
    else
        washdc_controller_release_btns(0, WASHDC_CONT_BTN_DPAD_RIGHT_MASK);

    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_R_TRIG, trig_r);
    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_L_TRIG, trig_l);
    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_JOY1_X, stick_hor);
    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_JOY1_Y, stick_vert);
    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_JOY2_X, 0);
    washdc_controller_set_axis(0, WASHDC_CONTROLLER_AXIS_JOY2_Y, 0);

    // Allow the user to toggle the overlay by pressing F2
    static bool overlay_key_prev = false;
    bool overlay_key = ctrl_get_button("toggle-overlay");
    if (overlay_key && !overlay_key_prev)
        washdc_gfx_toggle_overlay();
    overlay_key_prev = overlay_key;

    // toggle wireframe rendering
    static bool wireframe_key_prev = false;
    bool wireframe_key = ctrl_get_button("toggle-wireframe");
    if (wireframe_key && !wireframe_key_prev)
        washdc_gfx_toggle_wireframe();
    wireframe_key_prev = wireframe_key;

    // Allow the user to toggle fullscreen
    static bool fullscreen_key_prev = false;
    bool fullscreen_key = ctrl_get_button("toggle-fullscreen");
    if (fullscreen_key && !fullscreen_key_prev)
        toggle_fullscreen();
    fullscreen_key_prev = fullscreen_key;

    static bool filter_key_prev = false;
    bool filter_key = ctrl_get_button("toggle-filter");
    if (filter_key && !filter_key_prev)
        washdc_gfx_toggle_filter();
    filter_key_prev = filter_key;

    static bool screenshot_key_prev = false;
    bool screenshot_key = ctrl_get_button("screenshot");
    if (screenshot_key && !screenshot_key_prev)
        washdc_save_screenshot_dir();
    screenshot_key_prev = screenshot_key;

    bool exit_key = ctrl_get_button("exit-now");
    if (exit_key) {
        printf("emergency exit button pressed - WashingtonDC will exit soon.\n");
        washdc_kill();
    }
}

static void win_glfw_make_context_current(void) {
    glfwMakeContextCurrent(win);
}

static void win_glfw_update_title(void) {
    glfwSetWindowTitle(win, washdc_win_get_title());
}

static void resize_callback(GLFWwindow *win, int width, int height) {
    res_x = width;
    res_y = height;
    washdc_on_resize(width, height);
}

static int win_glfw_get_width(void) {
    return res_x;
}

static int win_glfw_get_height(void) {
    return res_y;
}

static void toggle_fullscreen(void) {
    int old_res_x = res_x;
    int old_res_y = res_y;

    if (win_mode == WIN_MODE_WINDOWED) {
        printf("toggle windowed=>fullscreen\n");

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);
        res_x = vidmode->width;
        res_y = vidmode->height;

        win_mode = WIN_MODE_FULLSCREEN;
        glfwSetWindowMonitor(win, glfwGetPrimaryMonitor(), 0, 0,
                             res_x, res_y, GLFW_DONT_CARE);
    } else {
        printf("toggle fullscreen=>windowed\n");
        win_mode = WIN_MODE_WINDOWED;
        res_x = win_res_x;
        res_y = win_res_y;
        glfwSetWindowMonitor(win, NULL, 0, 0,
                             res_x, res_y, GLFW_DONT_CARE);
    }

    if (res_x != old_res_x || res_y != old_res_y)
        washdc_on_resize(res_x, res_y);
}
