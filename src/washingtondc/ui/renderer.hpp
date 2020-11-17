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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include <string>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "imgui.h"

class renderer {
    GLuint vbo, vao, ebo;
    GLuint frag_shader, vert_shader;
    GLuint program;
    GLuint tex_obj;

    static char const * const vert_shader_glsl;
    static char const * const frag_shader_glsl;

    void do_render_draw_list(struct ImDrawList *list, const ImVec2& disp_pos,
                             const ImVec2& disp_dims);

    void create_program();

public:
    renderer();
    ~renderer();

    void do_render(struct ImDrawData *dat);

    void update();
};

#endif
