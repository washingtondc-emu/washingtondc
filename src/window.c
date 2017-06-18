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

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "video/opengl/opengl_output.h"
#include "dreamcast.h"

#include "window.h"

static unsigned res_x, res_y;
static GLFWwindow *win;

static void expose_callback(GLFWwindow *win);

void win_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    if (!glfwInit())
        err(1, "unable to initialize glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    win = glfwCreateWindow(res_x, res_y, "WashingtonDC Dreamcast Emulator", NULL, NULL);

    if (!win)
        errx(1, "unable to create window");

    glfwMakeContextCurrent(win);

    glewExperimental = GL_TRUE;
    glewInit();

    glViewport(0, 0, res_x, res_y);

    glfwSetWindowRefreshCallback(win, expose_callback);
    glfwSwapInterval(0);
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
    opengl_video_present();
    win_update();
}
