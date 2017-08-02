/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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
#include <stdbool.h>

#include <GLFW/glfw3.h>

#include "gfx/opengl/opengl_output.h"
#include "dreamcast.h"
#include "hw/maple/maple_controller.h"
#include "gfx/gfx_thread.h"

#include "window.h"

static unsigned res_x, res_y;
static GLFWwindow *win;

static void expose_callback(GLFWwindow *win);
static void win_on_key_press(GLFWwindow *win_ptr, int key, int scancode,
                             int action, int mods);

void win_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    if (!glfwInit())
        err(1, "unable to initialize glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    win = glfwCreateWindow(res_x, res_y, "WashingtonDC Dreamcast Emulator", NULL, NULL);

    if (!win)
        errx(1, "unable to create window");

    glfwSetWindowRefreshCallback(win, expose_callback);
    glfwSwapInterval(0);

    glfwSetKeyCallback(win, win_on_key_press);
}

void win_cleanup() {
    glfwTerminate();
}

void win_check_events(void) {
    glfwWaitEvents();

    if (glfwWindowShouldClose(win))
        dreamcast_kill();
}

void win_update() {
    glfwSwapBuffers(win);
}

static void expose_callback(GLFWwindow *win) {
    gfx_thread_expose();
}

static void win_on_key_press(GLFWwindow *win_ptr, int key, int scancode,
                             int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_W:
            maple_controller_press_btns(MAPLE_CONT_BTN_DPAD_UP_MASK);
            printf("Up pressed\n");
            break;
        case GLFW_KEY_S:
            maple_controller_press_btns(MAPLE_CONT_BTN_DPAD_DOWN_MASK);
            printf("Down pressed\n");
            break;
        case GLFW_KEY_A:
            maple_controller_press_btns(MAPLE_CONT_BTN_DPAD_LEFT_MASK);
            printf("Left pressed\n");
            break;
        case GLFW_KEY_D:
            maple_controller_press_btns(MAPLE_CONT_BTN_DPAD_RIGHT_MASK);
            printf("Right pressed\n");
            break;
        case GLFW_KEY_KP_2:
            maple_controller_press_btns(MAPLE_CONT_BTN_A_MASK);
            printf("A pressed\n");
            break;
        case GLFW_KEY_KP_6:
            maple_controller_press_btns(MAPLE_CONT_BTN_B_MASK);
            printf("B pressed\n");
            break;
        case GLFW_KEY_KP_4:
            maple_controller_press_btns(MAPLE_CONT_BTN_X_MASK);
            printf("X pressed\n");
            break;
        case GLFW_KEY_KP_8:
            maple_controller_press_btns(MAPLE_CONT_BTN_Y_MASK);
            printf("Y pressed\n");
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_W:
            maple_controller_release_btns(MAPLE_CONT_BTN_DPAD_UP_MASK);
            printf("Up released\n");
            break;
        case GLFW_KEY_S:
            maple_controller_release_btns(MAPLE_CONT_BTN_DPAD_DOWN_MASK);
            printf("Down released\n");
            break;
        case GLFW_KEY_A:
            maple_controller_release_btns(MAPLE_CONT_BTN_DPAD_LEFT_MASK);
            printf("Left released\n");
            break;
        case GLFW_KEY_D:
            maple_controller_release_btns(MAPLE_CONT_BTN_DPAD_RIGHT_MASK);
            printf("Right released\n");
            break;
        case GLFW_KEY_KP_2:
            maple_controller_release_btns(MAPLE_CONT_BTN_A_MASK);
            printf("A released\n");
            break;
        case GLFW_KEY_KP_6:
            maple_controller_release_btns(MAPLE_CONT_BTN_B_MASK);
            printf("B released\n");
            break;
        case GLFW_KEY_KP_4:
            maple_controller_release_btns(MAPLE_CONT_BTN_X_MASK);
            printf("X released\n");
            break;
        case GLFW_KEY_KP_8:
            maple_controller_release_btns(MAPLE_CONT_BTN_Y_MASK);
            printf("Y released\n");
            break;
        }
    }
}

void win_make_context_current(void) {
    glfwMakeContextCurrent(win);
}
