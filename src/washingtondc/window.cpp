/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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
#include <cstdio>
#include <cstring>
#include <algorithm>

#include <GLFW/glfw3.h>

#include "opengl/opengl_output.h"

#include "washdc/washdc.h"

#include "washdc/config_file.h"

#include "control_bind.hpp"
#include "window.hpp"
#include "ui/overlay.hpp"
#include "sound.hpp"

static void win_glfw_init(unsigned width, unsigned height);
static void win_glfw_cleanup();
static void win_glfw_check_events(void);
static void win_glfw_make_context_current(void);
static void win_glfw_update_title(void);
int win_glfw_get_width(void);
int win_glfw_get_height(void);

enum win_mode {
    WIN_MODE_WINDOWED,
    WIN_MODE_FULLSCREEN
};

static unsigned res_x, res_y;
static unsigned win_res_x, win_res_y;
static GLFWwindow *win;
static enum win_mode win_mode = WIN_MODE_WINDOWED;

static const unsigned N_MOUSE_BTNS = 3;
static bool mouse_btns[N_MOUSE_BTNS];

static bool show_overlay;

static double mouse_scroll_x, mouse_scroll_y;

static void expose_callback(GLFWwindow *win);
static void resize_callback(GLFWwindow *win, int width, int height);
static void scan_input(void);
static void toggle_fullscreen(void);
static void toggle_overlay(void);
static void mouse_btn_cb(GLFWwindow *win, int btn, int action, int mods);
static void
mouse_scroll_cb(GLFWwindow *win, double scroll_x, double scroll_y);
static void text_input_cb(GLFWwindow* window, unsigned int codepoint);

static void do_redraw(void) {
    opengl_video_present();
    overlay::draw();
    win_glfw_update();
}

struct win_intf const* get_win_intf_glfw(void) {
    static struct win_intf win_intf_glfw = { };

    win_intf_glfw.init = win_glfw_init;
    win_intf_glfw.cleanup = win_glfw_cleanup;
    win_intf_glfw.check_events = win_glfw_check_events;
    win_intf_glfw.update = win_glfw_update;
    win_intf_glfw.make_context_current = win_glfw_make_context_current;
    win_intf_glfw.get_width = win_glfw_get_width;
    win_intf_glfw.get_height = win_glfw_get_height;
    win_intf_glfw.update_title = win_glfw_update_title;

    return &win_intf_glfw;
}

static int bind_ctrl_from_cfg(char const *name, char const *cfg_node) {
    char const *bindstr = cfg_get_node(cfg_node);
    if (!bindstr)
        return -1;
    struct host_ctrl_bind bind;
    int err;
    if ((err = ctrl_parse_bind(bindstr, &bind)) < 0) {
        return err;
    }

    switch (bind.tp) {
    case HOST_CTRL_TP_KBD:
        bind.ctrl.kbd.win = win;
        ctrl_bind_key(name, bind);
        return 0;
    case HOST_CTRL_TP_JOYSTICK_BTN:
        bind.ctrl.joystick.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    case HOST_CTRL_TP_JOYSTICK_AXIS:
        bind.ctrl.axis.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    case HOST_CTRL_TP_JOYSTICK_HAT:
        bind.ctrl.hat.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    case HOST_CTRL_TP_GAMEPAD_BTN:
        bind.ctrl.gp_btn.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    case HOST_CTRL_TP_GAMEPAD_AXIS:
        bind.ctrl.gp_axis.js += GLFW_JOYSTICK_1;
        ctrl_bind_key(name, bind);
        return 0;
    default:
        return -1;
    }
}

static void win_glfw_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    win_res_x = width;
    win_res_y = height;

    std::fill(mouse_btns, mouse_btns + N_MOUSE_BTNS, false);
    mouse_scroll_x = mouse_scroll_y = 0.0;

    if (!glfwInit()) {
        fprintf(stderr, "unable to initialized glfw.\n");
        exit(1);
    }

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

    if (!win) {
        fprintf(stderr, "unable to create window\n");
        exit(1);
    }

    glfwSetWindowRefreshCallback(win, expose_callback);
    glfwSetFramebufferSizeCallback(win, resize_callback);
    glfwSetScrollCallback(win, mouse_scroll_cb);

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
    bind_ctrl_from_cfg("toggle-mute", "wash.ctrl.toggle-mute");
    bind_ctrl_from_cfg("resume-execution", "wash.ctrl.resume-execution");
    bind_ctrl_from_cfg("run-one-frame", "wash.ctrl.run-one-frame");
    bind_ctrl_from_cfg("pause-execution", "wash.ctrl.pause-execution");

    /*
     * This bind immediately exits the emulator.  It is unbound in the default
     * config because I don't want people pressing it by mistake, but it's good
     * to have around for dev work.
     */
    bind_ctrl_from_cfg("exit-now", "wash.ctrl.exit");

    /*
     * pN_1 and pN_2 both refer to the same buttons on player N's controller.
     * It's there to provide a way to have two different bindings for the same
     * button.
     */

    ///////////////////////////////////////////////////////////////////////////
    //
    // Player 1
    //
    ///////////////////////////////////////////////////////////////////////////
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

    ///////////////////////////////////////////////////////////////////////////
    //
    // Player 1 keyboard
    //
    ///////////////////////////////////////////////////////////////////////////
    bind_ctrl_from_cfg("p1_1.kbd_unused_00h", "dc.ctrl.p1_1.kbd-us.unused_00h");
    bind_ctrl_from_cfg("p1_1.kbd_unused_01h", "dc.ctrl.p1_1.kbd-us.unused_01h");
    bind_ctrl_from_cfg("p1_1.kbd_unused_02h", "dc.ctrl.p1_1.kbd-us.unused_02h");
    bind_ctrl_from_cfg("p1_1.kbd_unused_03h", "dc.ctrl.p1_1.kbd-us.unused_03h");
    bind_ctrl_from_cfg("p1_1.kbd_a", "dc.ctrl.p1_1.kbd-us.a");
    bind_ctrl_from_cfg("p1_1.kbd_b", "dc.ctrl.p1_1.kbd-us.b");
    bind_ctrl_from_cfg("p1_1.kbd_c", "dc.ctrl.p1_1.kbd-us.c");
    bind_ctrl_from_cfg("p1_1.kbd_d", "dc.ctrl.p1_1.kbd-us.d");
    bind_ctrl_from_cfg("p1_1.kbd_e", "dc.ctrl.p1_1.kbd-us.e");
    bind_ctrl_from_cfg("p1_1.kbd_f", "dc.ctrl.p1_1.kbd-us.f");
    bind_ctrl_from_cfg("p1_1.kbd_g", "dc.ctrl.p1_1.kbd-us.g");
    bind_ctrl_from_cfg("p1_1.kbd_h", "dc.ctrl.p1_1.kbd-us.h");
    bind_ctrl_from_cfg("p1_1.kbd_i", "dc.ctrl.p1_1.kbd-us.i");
    bind_ctrl_from_cfg("p1_1.kbd_j", "dc.ctrl.p1_1.kbd-us.j");
    bind_ctrl_from_cfg("p1_1.kbd_k", "dc.ctrl.p1_1.kbd-us.k");
    bind_ctrl_from_cfg("p1_1.kbd_l", "dc.ctrl.p1_1.kbd-us.l");
    bind_ctrl_from_cfg("p1_1.kbd_m", "dc.ctrl.p1_1.kbd-us.m");
    bind_ctrl_from_cfg("p1_1.kbd_n", "dc.ctrl.p1_1.kbd-us.n");
    bind_ctrl_from_cfg("p1_1.kbd_o", "dc.ctrl.p1_1.kbd-us.o");
    bind_ctrl_from_cfg("p1_1.kbd_p", "dc.ctrl.p1_1.kbd-us.p");
    bind_ctrl_from_cfg("p1_1.kbd_q", "dc.ctrl.p1_1.kbd-us.q");
    bind_ctrl_from_cfg("p1_1.kbd_r", "dc.ctrl.p1_1.kbd-us.r");
    bind_ctrl_from_cfg("p1_1.kbd_s", "dc.ctrl.p1_1.kbd-us.s");
    bind_ctrl_from_cfg("p1_1.kbd_t", "dc.ctrl.p1_1.kbd-us.t");
    bind_ctrl_from_cfg("p1_1.kbd_u", "dc.ctrl.p1_1.kbd-us.u");
    bind_ctrl_from_cfg("p1_1.kbd_v", "dc.ctrl.p1_1.kbd-us.v");
    bind_ctrl_from_cfg("p1_1.kbd_w", "dc.ctrl.p1_1.kbd-us.w");
    bind_ctrl_from_cfg("p1_1.kbd_x", "dc.ctrl.p1_1.kbd-us.x");
    bind_ctrl_from_cfg("p1_1.kbd_y", "dc.ctrl.p1_1.kbd-us.y");
    bind_ctrl_from_cfg("p1_1.kbd_z", "dc.ctrl.p1_1.kbd-us.z");
    bind_ctrl_from_cfg("p1_1.kbd_1", "dc.ctrl.p1_1.kbd-us.1");
    bind_ctrl_from_cfg("p1_1.kbd_2", "dc.ctrl.p1_1.kbd-us.2");
    bind_ctrl_from_cfg("p1_1.kbd_3", "dc.ctrl.p1_1.kbd-us.3");
    bind_ctrl_from_cfg("p1_1.kbd_4", "dc.ctrl.p1_1.kbd-us.4");
    bind_ctrl_from_cfg("p1_1.kbd_5", "dc.ctrl.p1_1.kbd-us.5");
    bind_ctrl_from_cfg("p1_1.kbd_6", "dc.ctrl.p1_1.kbd-us.6");
    bind_ctrl_from_cfg("p1_1.kbd_7", "dc.ctrl.p1_1.kbd-us.7");
    bind_ctrl_from_cfg("p1_1.kbd_8", "dc.ctrl.p1_1.kbd-us.8");
    bind_ctrl_from_cfg("p1_1.kbd_9", "dc.ctrl.p1_1.kbd-us.9");
    bind_ctrl_from_cfg("p1_1.kbd_0", "dc.ctrl.p1_1.kbd-us.0");
    bind_ctrl_from_cfg("p1_1.kbd_enter", "dc.ctrl.p1_1.kbd-us.enter");
    bind_ctrl_from_cfg("p1_1.kbd_escape", "dc.ctrl.p1_1.kbd-us.escape");
    bind_ctrl_from_cfg("p1_1.kbd_backspace", "dc.ctrl.p1_1.kbd-us.backspace");
    bind_ctrl_from_cfg("p1_1.kbd_tab", "dc.ctrl.p1_1.kbd-us.tab");
    bind_ctrl_from_cfg("p1_1.kbd_space", "dc.ctrl.p1_1.kbd-us.space");
    bind_ctrl_from_cfg("p1_1.kbd_minus", "dc.ctrl.p1_1.kbd-us.minus");
    bind_ctrl_from_cfg("p1_1.kbd_equal", "dc.ctrl.p1_1.kbd-us.equal");
    bind_ctrl_from_cfg("p1_1.kbd_leftbrace", "dc.ctrl.p1_1.kbd-us.leftbrace");
    bind_ctrl_from_cfg("p1_1.kbd_rightbrace", "dc.ctrl.p1_1.kbd-us.rightbrace");
    bind_ctrl_from_cfg("p1_1.kbd_backslash", "dc.ctrl.p1_1.kbd-us.backslash");
    bind_ctrl_from_cfg("p1_1.kbd_unused_32h", "dc.ctrl.p1_1.kbd-us.unused_32h");
    bind_ctrl_from_cfg("p1_1.kbd_semicolon", "dc.ctrl.p1_1.kbd-us.semicolon");
    bind_ctrl_from_cfg("p1_1.kbd_singlequote", "dc.ctrl.p1_1.kbd-us.singlequote");
    bind_ctrl_from_cfg("p1_1.kbd_backquote", "dc.ctrl.p1_1.kbd-us.backquote");
    bind_ctrl_from_cfg("p1_1.kbd_comma", "dc.ctrl.p1_1.kbd-us.comma");
    bind_ctrl_from_cfg("p1_1.kbd_dot", "dc.ctrl.p1_1.kbd-us.dot");
    bind_ctrl_from_cfg("p1_1.kbd_slash", "dc.ctrl.p1_1.kbd-us.slash");
    bind_ctrl_from_cfg("p1_1.kbd_capslock", "dc.ctrl.p1_1.kbd-us.capslock");

    bind_ctrl_from_cfg("p1_1.kbd_f1", "dc.ctrl.p1_1.kbd-us.f1");
    bind_ctrl_from_cfg("p1_1.kbd_f2", "dc.ctrl.p1_1.kbd-us.f2");
    bind_ctrl_from_cfg("p1_1.kbd_f3", "dc.ctrl.p1_1.kbd-us.f3");
    bind_ctrl_from_cfg("p1_1.kbd_f4", "dc.ctrl.p1_1.kbd-us.f4");
    bind_ctrl_from_cfg("p1_1.kbd_f5", "dc.ctrl.p1_1.kbd-us.f5");
    bind_ctrl_from_cfg("p1_1.kbd_f6", "dc.ctrl.p1_1.kbd-us.f6");
    bind_ctrl_from_cfg("p1_1.kbd_f7", "dc.ctrl.p1_1.kbd-us.f7");
    bind_ctrl_from_cfg("p1_1.kbd_f8", "dc.ctrl.p1_1.kbd-us.f8");
    bind_ctrl_from_cfg("p1_1.kbd_f9", "dc.ctrl.p1_1.kbd-us.f9");
    bind_ctrl_from_cfg("p1_1.kbd_f10", "dc.ctrl.p1_1.kbd-us.f10");
    bind_ctrl_from_cfg("p1_1.kbd_f11", "dc.ctrl.p1_1.kbd-us.f11");
    bind_ctrl_from_cfg("p1_1.kbd_f12", "dc.ctrl.p1_1.kbd-us.f12");

    bind_ctrl_from_cfg("p1_1.kbd_printscreen", "dc.ctrl.p1_1.kbd-us.printscreen");
    bind_ctrl_from_cfg("p1_1.kbd_scrollock", "dc.ctrl.p1_1.kbd-us.scrollock");
    bind_ctrl_from_cfg("p1_1.kbd_pause", "dc.ctrl.p1_1.kbd-us.pause");
    bind_ctrl_from_cfg("p1_1.kbd_insert", "dc.ctrl.p1_1.kbd-us.insert");
    bind_ctrl_from_cfg("p1_1.kbd_home", "dc.ctrl.p1_1.kbd-us.home");
    bind_ctrl_from_cfg("p1_1.kbd_pageup", "dc.ctrl.p1_1.kbd-us.pageup");
    bind_ctrl_from_cfg("p1_1.kbd_del", "dc.ctrl.p1_1.kbd-us.del");
    bind_ctrl_from_cfg("p1_1.kbd_end", "dc.ctrl.p1_1.kbd-us.end");
    bind_ctrl_from_cfg("p1_1.kbd_pagedown", "dc.ctrl.p1_1.kbd-us.pagedown");
    bind_ctrl_from_cfg("p1_1.kbd_right", "dc.ctrl.p1_1.kbd-us.right");
    bind_ctrl_from_cfg("p1_1.kbd_left", "dc.ctrl.p1_1.kbd-us.left");
    bind_ctrl_from_cfg("p1_1.kbd_down", "dc.ctrl.p1_1.kbd-us.down");
    bind_ctrl_from_cfg("p1_1.kbd_up", "dc.ctrl.p1_1.kbd-us.up");
    bind_ctrl_from_cfg("p1_1.kbd_numlock", "dc.ctrl.p1_1.kbd-us.numlock");

    bind_ctrl_from_cfg("p1_1.kbd_keypadslash", "dc.ctrl.p1_1.kbd-us.keypadslash");
    bind_ctrl_from_cfg("p1_1.kbd_keypadasterisk", "dc.ctrl.p1_1.kbd-us.keypadasterisk");
    bind_ctrl_from_cfg("p1_1.kbd_keypadminus", "dc.ctrl.p1_1.kbd-us.keypadminus");
    bind_ctrl_from_cfg("p1_1.kbd_keypadplus", "dc.ctrl.p1_1.kbd-us.keypadplus");
    bind_ctrl_from_cfg("p1_1.kbd_keypadenter", "dc.ctrl.p1_1.kbd-us.keypadenter");
    bind_ctrl_from_cfg("p1_1.kbd_keypad1", "dc.ctrl.p1_1.kbd-us.keypad1");
    bind_ctrl_from_cfg("p1_1.kbd_keypad2", "dc.ctrl.p1_1.kbd-us.keypad2");
    bind_ctrl_from_cfg("p1_1.kbd_keypad3", "dc.ctrl.p1_1.kbd-us.keypad3");
    bind_ctrl_from_cfg("p1_1.kbd_keypad4", "dc.ctrl.p1_1.kbd-us.keypad4");
    bind_ctrl_from_cfg("p1_1.kbd_keypad5", "dc.ctrl.p1_1.kbd-us.keypad5");
    bind_ctrl_from_cfg("p1_1.kbd_keypad6", "dc.ctrl.p1_1.kbd-us.keypad6");
    bind_ctrl_from_cfg("p1_1.kbd_keypad7", "dc.ctrl.p1_1.kbd-us.keypad7");
    bind_ctrl_from_cfg("p1_1.kbd_keypad8", "dc.ctrl.p1_1.kbd-us.keypad8");
    bind_ctrl_from_cfg("p1_1.kbd_keypad9", "dc.ctrl.p1_1.kbd-us.keypad9");
    bind_ctrl_from_cfg("p1_1.kbd_keypad0", "dc.ctrl.p1_1.kbd-us.keypad0");
    bind_ctrl_from_cfg("p1_1.kbd_keypaddot", "dc.ctrl.p1_1.kbd-us.keypaddot");
    bind_ctrl_from_cfg("p1_1.kbd_s3", "dc.ctrl.p1_1.kbd-us.s3");
    bind_ctrl_from_cfg("p1_1.kbd_lctrl", "dc.ctrl.p1_1.kbd-us.lctrl");
    bind_ctrl_from_cfg("p1_1.kbd_lshift", "dc.ctrl.p1_1.kbd-us.lshift");
    bind_ctrl_from_cfg("p1_1.kbd_lalt", "dc.ctrl.p1_1.kbd-us.lalt");
    bind_ctrl_from_cfg("p1_1.kbd_s1", "dc.ctrl.p1_1.kbd-us.s1");
    bind_ctrl_from_cfg("p1_1.kbd_rctrl", "dc.ctrl.p1_1.kbd-us.rctrl");
    bind_ctrl_from_cfg("p1_1.kbd_rshift", "dc.ctrl.p1_1.kbd-us.rshift");
    bind_ctrl_from_cfg("p1_1.kbd_ralt", "dc.ctrl.p1_1.kbd-us.ralt");
    bind_ctrl_from_cfg("p1_1.kbd_s2", "dc.ctrl.p1_1.kbd-us.s2");

    bind_ctrl_from_cfg("p1_2.kbd_unused_00h", "dc.ctrl.p1_2.kbd-us.unused_00h");
    bind_ctrl_from_cfg("p1_2.kbd_unused_01h", "dc.ctrl.p1_2.kbd-us.unused_01h");
    bind_ctrl_from_cfg("p1_2.kbd_unused_02h", "dc.ctrl.p1_2.kbd-us.unused_02h");
    bind_ctrl_from_cfg("p1_2.kbd_unused_03h", "dc.ctrl.p1_2.kbd-us.unused_03h");
    bind_ctrl_from_cfg("p1_2.kbd_a", "dc.ctrl.p1_2.kbd-us.a");
    bind_ctrl_from_cfg("p1_2.kbd_b", "dc.ctrl.p1_2.kbd-us.b");
    bind_ctrl_from_cfg("p1_2.kbd_c", "dc.ctrl.p1_2.kbd-us.c");
    bind_ctrl_from_cfg("p1_2.kbd_d", "dc.ctrl.p1_2.kbd-us.d");
    bind_ctrl_from_cfg("p1_2.kbd_e", "dc.ctrl.p1_2.kbd-us.e");
    bind_ctrl_from_cfg("p1_2.kbd_f", "dc.ctrl.p1_2.kbd-us.f");
    bind_ctrl_from_cfg("p1_2.kbd_g", "dc.ctrl.p1_2.kbd-us.g");
    bind_ctrl_from_cfg("p1_2.kbd_h", "dc.ctrl.p1_2.kbd-us.h");
    bind_ctrl_from_cfg("p1_2.kbd_i", "dc.ctrl.p1_2.kbd-us.i");
    bind_ctrl_from_cfg("p1_2.kbd_j", "dc.ctrl.p1_2.kbd-us.j");
    bind_ctrl_from_cfg("p1_2.kbd_k", "dc.ctrl.p1_2.kbd-us.k");
    bind_ctrl_from_cfg("p1_2.kbd_l", "dc.ctrl.p1_2.kbd-us.l");
    bind_ctrl_from_cfg("p1_2.kbd_m", "dc.ctrl.p1_2.kbd-us.m");
    bind_ctrl_from_cfg("p1_2.kbd_n", "dc.ctrl.p1_2.kbd-us.n");
    bind_ctrl_from_cfg("p1_2.kbd_o", "dc.ctrl.p1_2.kbd-us.o");
    bind_ctrl_from_cfg("p1_2.kbd_p", "dc.ctrl.p1_2.kbd-us.p");
    bind_ctrl_from_cfg("p1_2.kbd_q", "dc.ctrl.p1_2.kbd-us.q");
    bind_ctrl_from_cfg("p1_2.kbd_r", "dc.ctrl.p1_2.kbd-us.r");
    bind_ctrl_from_cfg("p1_2.kbd_s", "dc.ctrl.p1_2.kbd-us.s");
    bind_ctrl_from_cfg("p1_2.kbd_t", "dc.ctrl.p1_2.kbd-us.t");
    bind_ctrl_from_cfg("p1_2.kbd_u", "dc.ctrl.p1_2.kbd-us.u");
    bind_ctrl_from_cfg("p1_2.kbd_v", "dc.ctrl.p1_2.kbd-us.v");
    bind_ctrl_from_cfg("p1_2.kbd_w", "dc.ctrl.p1_2.kbd-us.w");
    bind_ctrl_from_cfg("p1_2.kbd_x", "dc.ctrl.p1_2.kbd-us.x");
    bind_ctrl_from_cfg("p1_2.kbd_y", "dc.ctrl.p1_2.kbd-us.y");
    bind_ctrl_from_cfg("p1_2.kbd_z", "dc.ctrl.p1_2.kbd-us.z");
    bind_ctrl_from_cfg("p1_2.kbd_1", "dc.ctrl.p1_2.kbd-us.1");
    bind_ctrl_from_cfg("p1_2.kbd_2", "dc.ctrl.p1_2.kbd-us.2");
    bind_ctrl_from_cfg("p1_2.kbd_3", "dc.ctrl.p1_2.kbd-us.3");
    bind_ctrl_from_cfg("p1_2.kbd_4", "dc.ctrl.p1_2.kbd-us.4");
    bind_ctrl_from_cfg("p1_2.kbd_5", "dc.ctrl.p1_2.kbd-us.5");
    bind_ctrl_from_cfg("p1_2.kbd_6", "dc.ctrl.p1_2.kbd-us.6");
    bind_ctrl_from_cfg("p1_2.kbd_7", "dc.ctrl.p1_2.kbd-us.7");
    bind_ctrl_from_cfg("p1_2.kbd_8", "dc.ctrl.p1_2.kbd-us.8");
    bind_ctrl_from_cfg("p1_2.kbd_9", "dc.ctrl.p1_2.kbd-us.9");
    bind_ctrl_from_cfg("p1_2.kbd_0", "dc.ctrl.p1_2.kbd-us.0");
    bind_ctrl_from_cfg("p1_2.kbd_enter", "dc.ctrl.p1_2.kbd-us.enter");
    bind_ctrl_from_cfg("p1_2.kbd_escape", "dc.ctrl.p1_2.kbd-us.escape");
    bind_ctrl_from_cfg("p1_2.kbd_backspace", "dc.ctrl.p1_2.kbd-us.backspace");
    bind_ctrl_from_cfg("p1_2.kbd_tab", "dc.ctrl.p1_2.kbd-us.tab");
    bind_ctrl_from_cfg("p1_2.kbd_space", "dc.ctrl.p1_2.kbd-us.space");
    bind_ctrl_from_cfg("p1_2.kbd_minus", "dc.ctrl.p1_2.kbd-us.minus");
    bind_ctrl_from_cfg("p1_2.kbd_equal", "dc.ctrl.p1_2.kbd-us.equal");
    bind_ctrl_from_cfg("p1_2.kbd_leftbrace", "dc.ctrl.p1_2.kbd-us.leftbrace");
    bind_ctrl_from_cfg("p1_2.kbd_rightbrace", "dc.ctrl.p1_2.kbd-us.rightbrace");
    bind_ctrl_from_cfg("p1_2.kbd_backslash", "dc.ctrl.p1_2.kbd-us.backslash");
    bind_ctrl_from_cfg("p1_2.kbd_unused_32h", "dc.ctrl.p1_2.kbd-us.unused_32h");
    bind_ctrl_from_cfg("p1_2.kbd_semicolon", "dc.ctrl.p1_2.kbd-us.semicolon");
    bind_ctrl_from_cfg("p1_2.kbd_singlequote", "dc.ctrl.p1_2.kbd-us.singlequote");
    bind_ctrl_from_cfg("p1_2.kbd_backquote", "dc.ctrl.p1_2.kbd-us.backquote");
    bind_ctrl_from_cfg("p1_2.kbd_comma", "dc.ctrl.p1_2.kbd-us.comma");
    bind_ctrl_from_cfg("p1_2.kbd_dot", "dc.ctrl.p1_2.kbd-us.dot");
    bind_ctrl_from_cfg("p1_2.kbd_slash", "dc.ctrl.p1_2.kbd-us.slash");

    bind_ctrl_from_cfg("p1_2.kbd_capslock", "dc.ctrl.p1_2.kbd-us.capslock");

    bind_ctrl_from_cfg("p1_2.kbd_f1", "dc.ctrl.p1_2.kbd-us.f1");
    bind_ctrl_from_cfg("p1_2.kbd_f2", "dc.ctrl.p1_2.kbd-us.f2");
    bind_ctrl_from_cfg("p1_2.kbd_f3", "dc.ctrl.p1_2.kbd-us.f3");
    bind_ctrl_from_cfg("p1_2.kbd_f4", "dc.ctrl.p1_2.kbd-us.f4");
    bind_ctrl_from_cfg("p1_2.kbd_f5", "dc.ctrl.p1_2.kbd-us.f5");
    bind_ctrl_from_cfg("p1_2.kbd_f6", "dc.ctrl.p1_2.kbd-us.f6");
    bind_ctrl_from_cfg("p1_2.kbd_f7", "dc.ctrl.p1_2.kbd-us.f7");
    bind_ctrl_from_cfg("p1_2.kbd_f8", "dc.ctrl.p1_2.kbd-us.f8");
    bind_ctrl_from_cfg("p1_2.kbd_f9", "dc.ctrl.p1_2.kbd-us.f9");
    bind_ctrl_from_cfg("p1_2.kbd_f10", "dc.ctrl.p1_2.kbd-us.f10");
    bind_ctrl_from_cfg("p1_2.kbd_f11", "dc.ctrl.p1_2.kbd-us.f11");
    bind_ctrl_from_cfg("p1_2.kbd_f12", "dc.ctrl.p1_2.kbd-us.f12");

    bind_ctrl_from_cfg("p1_2.kbd_printscreen", "dc.ctrl.p1_2.kbd-us.printscreen");
    bind_ctrl_from_cfg("p1_2.kbd_scrollock", "dc.ctrl.p1_2.kbd-us.scrollock");
    bind_ctrl_from_cfg("p1_2.kbd_pause", "dc.ctrl.p1_2.kbd-us.pause");
    bind_ctrl_from_cfg("p1_2.kbd_insert", "dc.ctrl.p1_2.kbd-us.insert");
    bind_ctrl_from_cfg("p1_2.kbd_home", "dc.ctrl.p1_2.kbd-us.home");
    bind_ctrl_from_cfg("p1_2.kbd_pageup", "dc.ctrl.p1_2.kbd-us.pageup");
    bind_ctrl_from_cfg("p1_2.kbd_del", "dc.ctrl.p1_2.kbd-us.del");
    bind_ctrl_from_cfg("p1_2.kbd_end", "dc.ctrl.p1_2.kbd-us.end");
    bind_ctrl_from_cfg("p1_2.kbd_pagedown", "dc.ctrl.p1_2.kbd-us.pagedown");
    bind_ctrl_from_cfg("p1_2.kbd_right", "dc.ctrl.p1_2.kbd-us.right");
    bind_ctrl_from_cfg("p1_2.kbd_left", "dc.ctrl.p1_2.kbd-us.left");
    bind_ctrl_from_cfg("p1_2.kbd_down", "dc.ctrl.p1_2.kbd-us.down");
    bind_ctrl_from_cfg("p1_2.kbd_up", "dc.ctrl.p1_2.kbd-us.up");
    bind_ctrl_from_cfg("p1_2.kbd_numlock", "dc.ctrl.p1_2.kbd-us.numlock");

    bind_ctrl_from_cfg("p1_2.kbd_keypadslash", "dc.ctrl.p1_2.kbd-us.keypadslash");
    bind_ctrl_from_cfg("p1_2.kbd_keypadasterisk", "dc.ctrl.p1_2.kbd-us.keypadasterisk");
    bind_ctrl_from_cfg("p1_2.kbd_keypadminus", "dc.ctrl.p1_2.kbd-us.keypadminus");
    bind_ctrl_from_cfg("p1_2.kbd_keypadplus", "dc.ctrl.p1_2.kbd-us.keypadplus");
    bind_ctrl_from_cfg("p1_2.kbd_keypadenter", "dc.ctrl.p1_2.kbd-us.keypadenter");
    bind_ctrl_from_cfg("p1_2.kbd_keypad1", "dc.ctrl.p1_2.kbd-us.keypad1");
    bind_ctrl_from_cfg("p1_2.kbd_keypad2", "dc.ctrl.p1_2.kbd-us.keypad2");
    bind_ctrl_from_cfg("p1_2.kbd_keypad3", "dc.ctrl.p1_2.kbd-us.keypad3");
    bind_ctrl_from_cfg("p1_2.kbd_keypad4", "dc.ctrl.p1_2.kbd-us.keypad4");
    bind_ctrl_from_cfg("p1_2.kbd_keypad5", "dc.ctrl.p1_2.kbd-us.keypad5");
    bind_ctrl_from_cfg("p1_2.kbd_keypad6", "dc.ctrl.p1_2.kbd-us.keypad6");
    bind_ctrl_from_cfg("p1_2.kbd_keypad7", "dc.ctrl.p1_2.kbd-us.keypad7");
    bind_ctrl_from_cfg("p1_2.kbd_keypad8", "dc.ctrl.p1_2.kbd-us.keypad8");
    bind_ctrl_from_cfg("p1_2.kbd_keypad9", "dc.ctrl.p1_2.kbd-us.keypad9");
    bind_ctrl_from_cfg("p1_2.kbd_keypad0", "dc.ctrl.p1_2.kbd-us.keypad0");
    bind_ctrl_from_cfg("p1_2.kbd_keypaddot", "dc.ctrl.p1_2.kbd-us.keypaddot");
    bind_ctrl_from_cfg("p1_2.kbd_s3", "dc.ctrl.p1_2.kbd-us.s3");
    bind_ctrl_from_cfg("p1_2.kbd_lctrl", "dc.ctrl.p1_2.kbd-us.lctrl");
    bind_ctrl_from_cfg("p1_2.kbd_lshift", "dc.ctrl.p1_2.kbd-us.lshift");
    bind_ctrl_from_cfg("p1_2.kbd_lalt", "dc.ctrl.p1_2.kbd-us.lalt");
    bind_ctrl_from_cfg("p1_2.kbd_s1", "dc.ctrl.p1_2.kbd-us.s1");
    bind_ctrl_from_cfg("p1_2.kbd_rctrl", "dc.ctrl.p1_2.kbd-us.rctrl");
    bind_ctrl_from_cfg("p1_2.kbd_rshift", "dc.ctrl.p1_2.kbd-us.rshift");
    bind_ctrl_from_cfg("p1_2.kbd_ralt", "dc.ctrl.p1_2.kbd-us.ralt");
    bind_ctrl_from_cfg("p1_2.kbd_s2", "dc.ctrl.p1_2.kbd-us.s2");

    ///////////////////////////////////////////////////////////////////////////
    //
    // Player 2
    //
    ///////////////////////////////////////////////////////////////////////////
    bind_ctrl_from_cfg("p2_1.dpad-up", "dc.ctrl.p2.dpad-up");
    bind_ctrl_from_cfg("p2_1.dpad-left", "dc.ctrl.p2.dpad-left");
    bind_ctrl_from_cfg("p2_1.dpad-down", "dc.ctrl.p2.dpad-down");
    bind_ctrl_from_cfg("p2_1.dpad-right", "dc.ctrl.p2.dpad-right");
    bind_ctrl_from_cfg("p2_1.btn_a", "dc.ctrl.p2.btn-a");
    bind_ctrl_from_cfg("p2_1.btn_b", "dc.ctrl.p2.btn-b");
    bind_ctrl_from_cfg("p2_1.btn_x", "dc.ctrl.p2.btn-x");
    bind_ctrl_from_cfg("p2_1.btn_y", "dc.ctrl.p2.btn-y");
    bind_ctrl_from_cfg("p2_1.btn_start", "dc.ctrl.p2.btn-start");
    bind_ctrl_from_cfg("p2_1.stick-left", "dc.ctrl.p2.stick-left");
    bind_ctrl_from_cfg("p2_1.stick-right", "dc.ctrl.p2.stick-right");
    bind_ctrl_from_cfg("p2_1.stick-up", "dc.ctrl.p2.stick-up");
    bind_ctrl_from_cfg("p2_1.stick-down", "dc.ctrl.p2.stick-down");
    bind_ctrl_from_cfg("p2_1.trig-l", "dc.ctrl.p2.trig-l");
    bind_ctrl_from_cfg("p2_1.trig-r", "dc.ctrl.p2.trig-r");

    bind_ctrl_from_cfg("p2_2.dpad-up", "dc.ctrl.p2.dpad-up(1)");
    bind_ctrl_from_cfg("p2_2.dpad-left", "dc.ctrl.p2.dpad-left(1)");
    bind_ctrl_from_cfg("p2_2.dpad-down", "dc.ctrl.p2.dpad-down(1)");
    bind_ctrl_from_cfg("p2_2.dpad-right", "dc.ctrl.p2.dpad-right(1)");
    bind_ctrl_from_cfg("p2_2.btn_a", "dc.ctrl.p2.btn-a(1)");
    bind_ctrl_from_cfg("p2_2.btn_b", "dc.ctrl.p2.btn-b(1)");
    bind_ctrl_from_cfg("p2_2.btn_x", "dc.ctrl.p2.btn-x(1)");
    bind_ctrl_from_cfg("p2_2.btn_y", "dc.ctrl.p2.btn-y(1)");
    bind_ctrl_from_cfg("p2_2.btn_start", "dc.ctrl.p2.btn-start(1)");
    bind_ctrl_from_cfg("p2_2.stick-left", "dc.ctrl.p2.stick-left(1)");
    bind_ctrl_from_cfg("p2_2.stick-right", "dc.ctrl.p2.stick-right(1)");
    bind_ctrl_from_cfg("p2_2.stick-up", "dc.ctrl.p2.stick-up(1)");
    bind_ctrl_from_cfg("p2_2.stick-down", "dc.ctrl.p2.stick-down(1)");
    bind_ctrl_from_cfg("p2_2.trig-l", "dc.ctrl.p2.trig-l(1)");
    bind_ctrl_from_cfg("p2_2.trig-r", "dc.ctrl.p2.trig-r(1)");

    ///////////////////////////////////////////////////////////////////////////
    //
    // Player 3
    //
    ///////////////////////////////////////////////////////////////////////////
    bind_ctrl_from_cfg("p3_1.dpad-up", "dc.ctrl.p3.dpad-up");
    bind_ctrl_from_cfg("p3_1.dpad-left", "dc.ctrl.p3.dpad-left");
    bind_ctrl_from_cfg("p3_1.dpad-down", "dc.ctrl.p3.dpad-down");
    bind_ctrl_from_cfg("p3_1.dpad-right", "dc.ctrl.p3.dpad-right");
    bind_ctrl_from_cfg("p3_1.btn_a", "dc.ctrl.p3.btn-a");
    bind_ctrl_from_cfg("p3_1.btn_b", "dc.ctrl.p3.btn-b");
    bind_ctrl_from_cfg("p3_1.btn_x", "dc.ctrl.p3.btn-x");
    bind_ctrl_from_cfg("p3_1.btn_y", "dc.ctrl.p3.btn-y");
    bind_ctrl_from_cfg("p3_1.btn_start", "dc.ctrl.p3.btn-start");
    bind_ctrl_from_cfg("p3_1.stick-left", "dc.ctrl.p3.stick-left");
    bind_ctrl_from_cfg("p3_1.stick-right", "dc.ctrl.p3.stick-right");
    bind_ctrl_from_cfg("p3_1.stick-up", "dc.ctrl.p3.stick-up");
    bind_ctrl_from_cfg("p3_1.stick-down", "dc.ctrl.p3.stick-down");
    bind_ctrl_from_cfg("p3_1.trig-l", "dc.ctrl.p3.trig-l");
    bind_ctrl_from_cfg("p3_1.trig-r", "dc.ctrl.p3.trig-r");

    bind_ctrl_from_cfg("p3_2.dpad-up", "dc.ctrl.p3.dpad-up(1)");
    bind_ctrl_from_cfg("p3_2.dpad-left", "dc.ctrl.p3.dpad-left(1)");
    bind_ctrl_from_cfg("p3_2.dpad-down", "dc.ctrl.p3.dpad-down(1)");
    bind_ctrl_from_cfg("p3_2.dpad-right", "dc.ctrl.p3.dpad-right(1)");
    bind_ctrl_from_cfg("p3_2.btn_a", "dc.ctrl.p3.btn-a(1)");
    bind_ctrl_from_cfg("p3_2.btn_b", "dc.ctrl.p3.btn-b(1)");
    bind_ctrl_from_cfg("p3_2.btn_x", "dc.ctrl.p3.btn-x(1)");
    bind_ctrl_from_cfg("p3_2.btn_y", "dc.ctrl.p3.btn-y(1)");
    bind_ctrl_from_cfg("p3_2.btn_start", "dc.ctrl.p3.btn-start(1)");
    bind_ctrl_from_cfg("p3_2.stick-left", "dc.ctrl.p3.stick-left(1)");
    bind_ctrl_from_cfg("p3_2.stick-right", "dc.ctrl.p3.stick-right(1)");
    bind_ctrl_from_cfg("p3_2.stick-up", "dc.ctrl.p3.stick-up(1)");
    bind_ctrl_from_cfg("p3_2.stick-down", "dc.ctrl.p3.stick-down(1)");
    bind_ctrl_from_cfg("p3_2.trig-l", "dc.ctrl.p3.trig-l(1)");
    bind_ctrl_from_cfg("p3_2.trig-r", "dc.ctrl.p3.trig-r(1)");

    ///////////////////////////////////////////////////////////////////////////
    //
    // Player 4
    //
    ///////////////////////////////////////////////////////////////////////////
    bind_ctrl_from_cfg("p4_1.dpad-up", "dc.ctrl.p4.dpad-up");
    bind_ctrl_from_cfg("p4_1.dpad-left", "dc.ctrl.p4.dpad-left");
    bind_ctrl_from_cfg("p4_1.dpad-down", "dc.ctrl.p4.dpad-down");
    bind_ctrl_from_cfg("p4_1.dpad-right", "dc.ctrl.p4.dpad-right");
    bind_ctrl_from_cfg("p4_1.btn_a", "dc.ctrl.p4.btn-a");
    bind_ctrl_from_cfg("p4_1.btn_b", "dc.ctrl.p4.btn-b");
    bind_ctrl_from_cfg("p4_1.btn_x", "dc.ctrl.p4.btn-x");
    bind_ctrl_from_cfg("p4_1.btn_y", "dc.ctrl.p4.btn-y");
    bind_ctrl_from_cfg("p4_1.btn_start", "dc.ctrl.p4.btn-start");
    bind_ctrl_from_cfg("p4_1.stick-left", "dc.ctrl.p4.stick-left");
    bind_ctrl_from_cfg("p4_1.stick-right", "dc.ctrl.p4.stick-right");
    bind_ctrl_from_cfg("p4_1.stick-up", "dc.ctrl.p4.stick-up");
    bind_ctrl_from_cfg("p4_1.stick-down", "dc.ctrl.p4.stick-down");
    bind_ctrl_from_cfg("p4_1.trig-l", "dc.ctrl.p4.trig-l");
    bind_ctrl_from_cfg("p4_1.trig-r", "dc.ctrl.p4.trig-r");

    bind_ctrl_from_cfg("p4_2.dpad-up", "dc.ctrl.p4.dpad-up(1)");
    bind_ctrl_from_cfg("p4_2.dpad-left", "dc.ctrl.p4.dpad-left(1)");
    bind_ctrl_from_cfg("p4_2.dpad-down", "dc.ctrl.p4.dpad-down(1)");
    bind_ctrl_from_cfg("p4_2.dpad-right", "dc.ctrl.p4.dpad-right(1)");
    bind_ctrl_from_cfg("p4_2.btn_a", "dc.ctrl.p4.btn-a(1)");
    bind_ctrl_from_cfg("p4_2.btn_b", "dc.ctrl.p4.btn-b(1)");
    bind_ctrl_from_cfg("p4_2.btn_x", "dc.ctrl.p4.btn-x(1)");
    bind_ctrl_from_cfg("p4_2.btn_y", "dc.ctrl.p4.btn-y(1)");
    bind_ctrl_from_cfg("p4_2.btn_start", "dc.ctrl.p4.btn-start(1)");
    bind_ctrl_from_cfg("p4_2.stick-left", "dc.ctrl.p4.stick-left(1)");
    bind_ctrl_from_cfg("p4_2.stick-right", "dc.ctrl.p4.stick-right(1)");
    bind_ctrl_from_cfg("p4_2.stick-up", "dc.ctrl.p4.stick-up(1)");
    bind_ctrl_from_cfg("p4_2.stick-down", "dc.ctrl.p4.stick-down(1)");
    bind_ctrl_from_cfg("p4_2.trig-l", "dc.ctrl.p4.trig-l(1)");
    bind_ctrl_from_cfg("p4_2.trig-r", "dc.ctrl.p4.trig-r(1)");

    glfwSetMouseButtonCallback(win, mouse_btn_cb);
    glfwSetCharCallback(win, text_input_cb);
}

static void win_glfw_cleanup() {
    ctrl_bind_cleanup();

    glfwTerminate();
}

static void win_glfw_check_events(void) {
    mouse_scroll_x = mouse_scroll_y = 0.0;

    glfwPollEvents();

    scan_input();

    overlay::update();

    if (glfwWindowShouldClose(win))
        washdc_kill();
}

void win_glfw_update() {
    glfwSwapBuffers(win);
}

static void expose_callback(GLFWwindow *win) {
    do_redraw();
}

enum gamepad_btn {
    GAMEPAD_BTN_A = 0,
    GAMEPAD_BTN_B = 1,
    GAMEPAD_BTN_X = 2,
    GAMEPAD_BTN_Y = 3,
    GAMEPAD_BTN_START = 7,

    GAMEPAD_BTN_COUNT
};

enum joystick_hat {
    GAMEPAD_HAT_UP,
    GAMEPAD_HAT_DOWN,
    GAMEPAD_HAT_LEFT,
    GAMEPAD_HAT_RIGHT,

    GAMEPAD_HAT_COUNT
};

static char const *bind_name(unsigned ctrlr, char const *bind) {
    static char name[CTRL_BIND_NAME_LEN];
    snprintf(name, sizeof(name), "p%u%s", ctrlr + 1, bind);
    name[sizeof(name)-1] = '\0';
    return name;
}

static void scan_input_for_controller(unsigned which) {
    if (which >= 4 ||
        (washdc_controller_type(which) !=
         WASHDC_CONTROLLER_TP_DREAMCAST_CONTROLLER)) {
        return;
    }

    bool btns[GAMEPAD_BTN_COUNT];
    bool hat[GAMEPAD_HAT_COUNT];

    float trig_l_real_1 = ctrl_get_axis(bind_name(which, "_1.trig-l")) + 1.0f;
    float trig_l_real_2 = ctrl_get_axis(bind_name(which, "_2.trig-l")) + 1.0f;
    float trig_l_real = trig_l_real_1 + trig_l_real_2;
    if (trig_l_real < 0.0f)
        trig_l_real = 0.0f;
    else if (trig_l_real > 1.0f)
        trig_l_real = 1.0f;

    float trig_r_real_1 = ctrl_get_axis(bind_name(which, "_1.trig-r")) + 1.0f;
    float trig_r_real_2 = ctrl_get_axis(bind_name(which, "_2.trig-r")) + 1.0f;
    float trig_r_real = trig_r_real_1 + trig_r_real_2;
    if (trig_r_real < 0.0f)
        trig_r_real = 0.0f;
    else if (trig_r_real > 1.0f)
        trig_r_real = 1.0f;

    int trig_l = trig_l_real * 255;
    int trig_r = trig_r_real * 255;

    float stick_up_real_1 = ctrl_get_axis(bind_name(which, "_1.stick-up"));
    float stick_down_real_1 = ctrl_get_axis(bind_name(which, "_1.stick-down"));
    float stick_left_real_1 = ctrl_get_axis(bind_name(which, "_1.stick-left"));
    float stick_right_real_1 = ctrl_get_axis(bind_name(which, "_1.stick-right"));
    float stick_up_real_2 = ctrl_get_axis(bind_name(which, "_2.stick-up"));
    float stick_down_real_2 = ctrl_get_axis(bind_name(which, "_2.stick-down"));
    float stick_left_real_2 = ctrl_get_axis(bind_name(which, "_2.stick-left"));
    float stick_right_real_2 = ctrl_get_axis(bind_name(which, "_2.stick-right"));

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

    btns[GAMEPAD_BTN_A] = ctrl_get_button(bind_name(which, "_1.btn_a")) ||
        ctrl_get_button(bind_name(which, "_2.btn_a"));
    btns[GAMEPAD_BTN_B] = ctrl_get_button(bind_name(which, "_1.btn_b")) ||
        ctrl_get_button(bind_name(which, "_2.btn_b"));
    btns[GAMEPAD_BTN_X] = ctrl_get_button(bind_name(which, "_1.btn_x")) ||
        ctrl_get_button(bind_name(which, "_2.btn_x"));
    btns[GAMEPAD_BTN_Y] = ctrl_get_button(bind_name(which, "_1.btn_y")) ||
        ctrl_get_button(bind_name(which, "_2.btn_y"));
    btns[GAMEPAD_BTN_START] = ctrl_get_button(bind_name(which, "_1.btn_start")) ||
        ctrl_get_button(bind_name(which, "_2.btn_start"));

    hat[GAMEPAD_HAT_UP] = ctrl_get_button(bind_name(which, "_1.dpad-up")) ||
        ctrl_get_button(bind_name(which, "_2.dpad-up"));
    hat[GAMEPAD_HAT_DOWN] = ctrl_get_button(bind_name(which, "_1.dpad-down")) ||
        ctrl_get_button(bind_name(which, "_2.dpad-down"));
    hat[GAMEPAD_HAT_LEFT] = ctrl_get_button(bind_name(which, "_1.dpad-left")) ||
        ctrl_get_button(bind_name(which, "_2.dpad-left"));
    hat[GAMEPAD_HAT_RIGHT] = ctrl_get_button(bind_name(which, "_1.dpad-right")) ||
        ctrl_get_button(bind_name(which, "_2.dpad-right"));

    if (btns[GAMEPAD_BTN_A])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_A_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_A_MASK);
    if (btns[GAMEPAD_BTN_B])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_B_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_B_MASK);
    if (btns[GAMEPAD_BTN_X])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_X_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_X_MASK);
    if (btns[GAMEPAD_BTN_Y])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_Y_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_Y_MASK);
    if (btns[GAMEPAD_BTN_START])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_START_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_START_MASK);

    if (hat[GAMEPAD_HAT_UP])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_DPAD_UP_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_DPAD_UP_MASK);
    if (hat[GAMEPAD_HAT_DOWN])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_DPAD_DOWN_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_DPAD_DOWN_MASK);
    if (hat[GAMEPAD_HAT_LEFT])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_DPAD_LEFT_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_DPAD_LEFT_MASK);
    if (hat[GAMEPAD_HAT_RIGHT])
        washdc_controller_press_btns(which, WASHDC_CONT_BTN_DPAD_RIGHT_MASK);
    else
        washdc_controller_release_btns(which, WASHDC_CONT_BTN_DPAD_RIGHT_MASK);

    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_R_TRIG, trig_r);
    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_L_TRIG, trig_l);
    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_JOY1_X, stick_hor);
    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_JOY1_Y, stick_vert);
    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_JOY2_X, 0);
    washdc_controller_set_axis(which, WASHDC_CONTROLLER_AXIS_JOY2_Y, 0);
}

static void scan_input_for_keyboard(unsigned which) {
    if (which >= 4 ||
        (washdc_controller_type(which) !=
         WASHDC_CONTROLLER_TP_DREAMCAST_KEYBOARD)) {
        return;
    }

    static char const* kbd_bind_names[] = {
        "_1.kbd_unused_00h",
        "_1.kbd_unused_01h",
        "_1.kbd_unused_02h",
        "_1.kbd_unused_03h",

        "_1.kbd_a",
        "_1.kbd_b",
        "_1.kbd_c",
        "_1.kbd_d",
        "_1.kbd_e",
        "_1.kbd_f",
        "_1.kbd_g",
        "_1.kbd_h",
        "_1.kbd_i",
        "_1.kbd_j",
        "_1.kbd_k",
        "_1.kbd_l",
        "_1.kbd_m",
        "_1.kbd_n",
        "_1.kbd_o",
        "_1.kbd_p",
        "_1.kbd_q",
        "_1.kbd_r",
        "_1.kbd_s",
        "_1.kbd_t",
        "_1.kbd_u",
        "_1.kbd_v",
        "_1.kbd_w",
        "_1.kbd_x",
        "_1.kbd_y",
        "_1.kbd_z",

        "_1.kbd_1",
        "_1.kbd_2",
        "_1.kbd_3",
        "_1.kbd_4",
        "_1.kbd_5",
        "_1.kbd_6",
        "_1.kbd_7",
        "_1.kbd_8",
        "_1.kbd_9",
        "_1.kbd_0",

        "_1.kbd_enter",
        "_1.kbd_escape",
        "_1.kbd_backspace",
        "_1.kbd_tab",
        "_1.kbd_space",
        "_1.kbd_minus",
        "_1.kbd_equal",
        "_1.kbd_leftbrace",
        "_1.kbd_rightbrace",
        "_1.kbd_backslash",

        "_1.kbd_unused_32h",

        "_1.kbd_semicolon",
        "_1.kbd_singlequote",
        "_1.kbd_backquote",
        "_1.kbd_comma",
        "_1.kbd_dot",
        "_1.kbd_slash",
        "_1.kbd_capslock",

        "_1.kbd_f1",
        "_1.kbd_f2",
        "_1.kbd_f3",
        "_1.kbd_f4",
        "_1.kbd_f5",
        "_1.kbd_f6",
        "_1.kbd_f7",
        "_1.kbd_f8",
        "_1.kbd_f9",
        "_1.kbd_f10",
        "_1.kbd_f11",
        "_1.kbd_f12",

        "_1.kbd_printscreen",
        "_1.kbd_scrollock",
        "_1.kbd_pause",
        "_1.kbd_insert",
        "_1.kbd_home",
        "_1.kbd_pageup",
        "_1.kbd_del",
        "_1.kbd_end",
        "_1.kbd_pagedown",
        "_1.kbd_right",
        "_1.kbd_left",
        "_1.kbd_down",
        "_1.kbd_up",
        "_1.kbd_numlock",

        "_1.kbd_keypadslash",
        "_1.kbd_keypadasterisk",
        "_1.kbd_keypadminus",
        "_1.kbd_keypadplus",
        "_1.kbd_keypadenter",
        "_1.kbd_keypad1",
        "_1.kbd_keypad2",
        "_1.kbd_keypad3",
        "_1.kbd_keypad4",
        "_1.kbd_keypad5",
        "_1.kbd_keypad6",
        "_1.kbd_keypad7",
        "_1.kbd_keypad8",
        "_1.kbd_keypad9",
        "_1.kbd_keypad0",
        "_1.kbd_keypaddot",
        "_1.kbd_s3",

        "_2.kbd_unused_00h",
        "_2.kbd_unused_01h",
        "_2.kbd_unused_02h",
        "_2.kbd_unused_03h",

        "_2.kbd_a",
        "_2.kbd_b",
        "_2.kbd_c",
        "_2.kbd_d",
        "_2.kbd_e",
        "_2.kbd_f",
        "_2.kbd_g",
        "_2.kbd_h",
        "_2.kbd_i",
        "_2.kbd_j",
        "_2.kbd_k",
        "_2.kbd_l",
        "_2.kbd_m",
        "_2.kbd_n",
        "_2.kbd_o",
        "_2.kbd_p",
        "_2.kbd_q",
        "_2.kbd_r",
        "_2.kbd_s",
        "_2.kbd_t",
        "_2.kbd_u",
        "_2.kbd_v",
        "_2.kbd_w",
        "_2.kbd_x",
        "_2.kbd_y",
        "_2.kbd_z",

        "_2.kbd_1",
        "_2.kbd_2",
        "_2.kbd_3",
        "_2.kbd_4",
        "_2.kbd_5",
        "_2.kbd_6",
        "_2.kbd_7",
        "_2.kbd_8",
        "_2.kbd_9",
        "_2.kbd_0",

        "_2.kbd_enter",
        "_2.kbd_escape",
        "_2.kbd_backspace",
        "_2.kbd_tab",
        "_2.kbd_space",
        "_2.kbd_minus",
        "_2.kbd_equal",
        "_2.kbd_leftbrace",
        "_2.kbd_rightbrace",
        "_2.kbd_backslash",

        "_2.kbd_unused_32h",

        "_2.kbd_semicolon",
        "_2.kbd_singlequote",
        "_2.kbd_backquote",
        "_2.kbd_comma",
        "_2.kbd_dot",
        "_2.kbd_slash",
        "_2.kbd_capslock",

        "_2.kbd_f1",
        "_2.kbd_f2",
        "_2.kbd_f3",
        "_2.kbd_f4",
        "_2.kbd_f5",
        "_2.kbd_f6",
        "_2.kbd_f7",
        "_2.kbd_f8",
        "_2.kbd_f9",
        "_2.kbd_f10",
        "_2.kbd_f11",
        "_2.kbd_f12",

        "_2.kbd_printscreen",
        "_2.kbd_scrollock",
        "_2.kbd_pause",
        "_2.kbd_insert",
        "_2.kbd_home",
        "_2.kbd_pageup",
        "_2.kbd_del",
        "_2.kbd_end",
        "_2.kbd_pagedown",
        "_2.kbd_right",
        "_2.kbd_left",
        "_2.kbd_down",
        "_2.kbd_up",
        "_2.kbd_numlock",

        "_2.kbd_keypadslash",
        "_2.kbd_keypadasterisk",
        "_2.kbd_keypadminus",
        "_2.kbd_keypadplus",
        "_2.kbd_keypadenter",
        "_2.kbd_keypad1",
        "_2.kbd_keypad2",
        "_2.kbd_keypad3",
        "_2.kbd_keypad4",
        "_2.kbd_keypad5",
        "_2.kbd_keypad6",
        "_2.kbd_keypad7",
        "_2.kbd_keypad8",
        "_2.kbd_keypad9",
        "_2.kbd_keypad0",
        "_2.kbd_keypaddot",

        NULL
    };

    int idx = 0;
    char const **curs = kbd_bind_names;
    while (*curs) {
        washdc_keyboard_set_btn(which, idx++,
                                ctrl_get_button(bind_name(which, *curs++)));
    }

    int mods = WASHDC_KEYBOARD_NONE;
    if (ctrl_get_button(bind_name(which, "_1.kbd_lctrl")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_lctrl")))
        mods |= WASHDC_KEYBOARD_LEFT_CTRL;
    if (ctrl_get_button(bind_name(which, "_1.kbd_lshift")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_lshift")))
        mods |= WASHDC_KEYBOARD_LEFT_SHIFT;
    if (ctrl_get_button(bind_name(which, "_1.kbd_lalt")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_lalt")))
        mods |= WASHDC_KEYBOARD_LEFT_ALT;
    if (ctrl_get_button(bind_name(which, "_1.kbd_s1")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_s1")))
        mods |= WASHDC_KEYBOARD_S1;
    if (ctrl_get_button(bind_name(which, "_1.kbd_rctrl")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_rctrl")))
        mods |= WASHDC_KEYBOARD_RIGHT_CTRL;
    if (ctrl_get_button(bind_name(which, "_1.kbd_rshift")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_rshift")))
        mods |= WASHDC_KEYBOARD_RIGHT_SHIFT;
    if (ctrl_get_button(bind_name(which, "_1.kbd_ralt")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_ralt")))
        mods |= WASHDC_KEYBOARD_RIGHT_ALT;
    if (ctrl_get_button(bind_name(which, "_1.kbd_s2")) ||
        ctrl_get_button(bind_name(which, "_2.kbd_s2")))
        mods |= WASHDC_KEYBOARD_S1;

    washdc_keyboard_press_special(which, (enum washdc_keyboard_special_keys)mods);
    washdc_keyboard_release_special(which, (enum washdc_keyboard_special_keys)~mods);
}

static void scan_input(void) {
    scan_input_for_controller(0);
    scan_input_for_controller(1);
    scan_input_for_controller(2);
    scan_input_for_controller(3);
    scan_input_for_keyboard(0);
    scan_input_for_keyboard(1);
    scan_input_for_keyboard(2);
    scan_input_for_keyboard(3);

    // Allow the user to toggle the overlay by pressing F2
    static bool overlay_key_prev = false;
    bool overlay_key = ctrl_get_button("toggle-overlay");
    if (overlay_key && !overlay_key_prev)
        toggle_overlay();
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
        opengl_video_toggle_filter();
    filter_key_prev = filter_key;

    static bool screenshot_key_prev = false;
    bool screenshot_key = ctrl_get_button("screenshot");
    if (screenshot_key && !screenshot_key_prev)
        washdc_save_screenshot_dir();
    screenshot_key_prev = screenshot_key;

    static bool mute_key_prev = false;
    bool mute_key = ctrl_get_button("toggle-mute");
    if (mute_key && !mute_key_prev)
        sound::mute(!sound::is_muted());
    mute_key_prev = mute_key;

    static bool resume_key_prev = false;
    bool resume_key = ctrl_get_button("resume-execution");
    if (resume_key && !resume_key_prev) {
        if (washdc_is_paused())
            washdc_resume();
    }
    resume_key_prev = resume_key;

    static bool run_frame_prev = false;
    bool run_frame_key = ctrl_get_button("run-one-frame");
    if (run_frame_key && !run_frame_prev) {
        if (washdc_is_paused())
            washdc_run_one_frame();
    }
    run_frame_prev = run_frame_key;

    static bool pause_key_prev = false;
    bool pause_key = ctrl_get_button("pause-execution");
    if (pause_key && !pause_key_prev) {
        if (!washdc_is_paused())
            washdc_pause();
    }
    pause_key_prev = pause_key;

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

    do_redraw();
}

int win_glfw_get_width(void) {
    return res_x;
}

int win_glfw_get_height(void) {
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
        do_redraw();
}

static void toggle_overlay(void) {
    show_overlay = !show_overlay;
    overlay::show(show_overlay);
}

static void mouse_btn_cb(GLFWwindow *win, int btn, int action, int mods) {
    if (btn >= 0 && btn < N_MOUSE_BTNS)
        mouse_btns[btn] = (action == GLFW_PRESS);
}

bool win_glfw_get_mouse_btn(unsigned btn) {
    if (btn < N_MOUSE_BTNS)
        return mouse_btns[btn];
    return false;
}

void win_glfw_get_mouse_pos(double *mouse_x, double *mouse_y) {
    glfwGetCursorPos(win, mouse_x, mouse_y);
}

void win_glfw_get_mouse_scroll(double *mouse_x, double *mouse_y) {
    *mouse_x = mouse_scroll_x;
    *mouse_y = mouse_scroll_y;
}

static void
mouse_scroll_cb(GLFWwindow *win, double scroll_x, double scroll_y) {
    mouse_scroll_x = scroll_x;
    mouse_scroll_y = scroll_y;
}

static void text_input_cb(GLFWwindow* window, unsigned int codepoint) {
    overlay::input_text(codepoint);
}
