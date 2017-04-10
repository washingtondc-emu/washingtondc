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

#include <cstdlib>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "BaseException.hpp"
#include "window.hpp"

static unsigned res_x, res_y;
static GLFWwindow *win;

void win_init(unsigned width, unsigned height) {
    res_x = width;
    res_y = height;

    if (!glfwInit())
        BOOST_THROW_EXCEPTION(IntegrityError());

    atexit(glfwTerminate); // TODO: this is the lazy way to implement cleanup

    win = glfwCreateWindow(res_x, res_y, "WashingtonDC Dreamcast Emulator", NULL, NULL);
    if (!win)
        BOOST_THROW_EXCEPTION(IntegrityError());

    glfwMakeContextCurrent(win);
}

bool win_check_events() {
    return !glfwWindowShouldClose(win);
}
