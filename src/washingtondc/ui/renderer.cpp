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

#include <iostream>

#include "../window.hpp"
#include "renderer.hpp"

char const * const renderer::vert_shader_glsl =
    "#version 330 core\n"
    "#extension GL_ARB_explicit_uniform_location : enable\n"

    "layout (location = 0) in vec2 vert_pos;\n"
    "layout (location = 1) in vec2 tex_coord;\n"
    "layout (location = 2) in vec4 vert_color;\n"
    "layout (location = 3) uniform mat4 trans_mat;\n"

    "out vec2 st;\n"
    "out vec4 col;\n"

    "void main() {\n"
    "    gl_Position = trans_mat * vec4(vert_pos.x, vert_pos.y, 0.0, 1.0);\n"
    "    st = tex_coord;\n"
    "    col = vert_color;\n"
    "}\n";

char const * const renderer::frag_shader_glsl =
    "#version 330 core\n"
    "in vec2 st;\n"
    "in vec4 col;\n"
    "out vec4 frag_color;\n"

    "uniform sampler2D fb_tex;\n"

    "void main() {\n"
    "    vec4 sample = texture(fb_tex, st);\n"
    "    frag_color = col * sample;\n"
    //    "    frag_color = col;\n"
    "}\n";

renderer::renderer() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    create_program();

    ImGuiIO& io = ImGui::GetIO();

    // TODO: do i need to free pixels here?
    int width, height;
    unsigned char *pixels;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    glGenTextures(1, &tex_obj);
    glBindTexture(GL_TEXTURE_2D, tex_obj);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    io.Fonts->TexID = (ImTextureID)(intptr_t)tex_obj;
}

renderer::~renderer() {
    glDeleteTextures(1, &tex_obj);

    glDeleteProgram(program);
    glDeleteShader(frag_shader);
    glDeleteShader(vert_shader);

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void renderer::do_render(struct ImDrawData *dat) {
    int n_lists = dat->CmdListsCount;
    for (int listno = 0; listno < n_lists; listno++) {
        do_render_draw_list(dat->CmdLists[listno],
                            dat->DisplayPos, dat->DisplaySize);
    }
}

/*
 * vertex format: XYUVRGBA
 */
void renderer::do_render_draw_list(struct ImDrawList *list,
                                   const ImVec2& disp_pos,
                                   const ImVec2& disp_dims) {
    GLboolean done;

    glUseProgram(program);

    glBindVertexArray(vao);

    do {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     8 * sizeof(GLfloat) * list->VtxBuffer.size(),
                     NULL, GL_DYNAMIC_DRAW);
        GLfloat *buf = (GLfloat*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

        for (ImDrawVert const& vert : list->VtxBuffer) {
            buf[0] = vert.pos.x;
            buf[1] = vert.pos.y;
            buf[2] = vert.uv.x;
            buf[3] = vert.uv.y;
            buf[4] = ((vert.col >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
            buf[5] = ((vert.col >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
            buf[6] = ((vert.col >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;
            buf[7] = ((vert.col >> IM_COL32_A_SHIFT) & 0xff) / 255.0f;
            buf += 8;
        }

        done = glUnmapBuffer(GL_ARRAY_BUFFER);
    } while (done != GL_TRUE);

    do {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(GLuint) * list->IdxBuffer.size(),
                     NULL, GL_DYNAMIC_DRAW);
        GLuint *buf = (GLuint*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        for (ImDrawIdx idx : list->IdxBuffer)
            *buf++ = idx;
        done = glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    } while (done != GL_TRUE);

    // position (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                          (GLvoid*)0);
    // texture coordinates (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                          (GLvoid*)(2 * sizeof(GLfloat)));
    // color (r, g, b, a)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat),
                          (GLvoid*)(4 * sizeof(GLfloat)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLfloat mview_mat[16] = {
        2.0f / (disp_dims.x - disp_pos.x), 0.0f, 0.0f, -1.0f * (disp_pos.x + disp_dims.x) / (disp_dims.x - disp_pos.x),
        0.0f, -2.0f / (disp_dims.y - disp_pos.y), 0.0f, disp_pos.y + disp_dims.y / (disp_dims.y - disp_pos.y),
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(3, 1, GL_TRUE, mview_mat);

    unsigned elem_start = 0;
    unsigned cmd_no = 0;
    for (ImDrawCmd const& cmd : list->CmdBuffer) {
        GLuint corners[4] = {
            (GLuint)(cmd.ClipRect.x - disp_pos.x),
            (GLuint)(disp_dims.y - (cmd.ClipRect.y - disp_pos.y)),
            (GLuint)(cmd.ClipRect.z - disp_pos.x),
            (GLuint)(disp_dims.y - (cmd.ClipRect.w - disp_pos.y))
        };

        GLuint scissor[4] = {
            corners[0],
            corners[3],
            corners[2] - corners[0],
            corners[1] - corners[3]
        };
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

        glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)cmd.TextureId);
        glDrawElements(GL_TRIANGLES, cmd.ElemCount, GL_UNSIGNED_INT,
                       (GLvoid*)(elem_start * sizeof(GLuint)));
        elem_start += cmd.ElemCount;
        cmd_no++;
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

void renderer::create_program() {
    static const unsigned LOG_LEN_GLSL = 1024;
    static GLchar shader_log[LOG_LEN_GLSL];

    program = glCreateProgram();

    frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_shader_glsl, NULL);
    glCompileShader(frag_shader);

    vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_shader_glsl, NULL);
    glCompileShader(vert_shader);

    GLint shader_success;
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(frag_shader, LOG_LEN_GLSL, NULL, shader_log);
        fprintf(stderr, "Error compiling fragment shader: %s\n", shader_log);
        exit(1);
    }
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(vert_shader, LOG_LEN_GLSL, NULL, shader_log);
        fprintf(stderr, "Error compiling vertex shader: %s\n", shader_log);
        exit(1);
    }

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &shader_success);
    if (!shader_success) {
        glGetProgramInfoLog(program, LOG_LEN_GLSL, NULL, shader_log);

        // glDeleteShader(out->vert_shader);
        // glDeleteShader(out->frag_shader);
        // glDeleteProgram(shader_obj);

        fprintf(stderr, "Error compiling shader: %s\n", shader_log);
        exit(1);
    }
}

void renderer::update() {
    ImGuiIO& io = ImGui::GetIO();
    for (int btn_no = 0; btn_no < IM_ARRAYSIZE(io.MouseDown); btn_no++)
        io.MouseDown[btn_no] = win_glfw_get_mouse_btn(btn_no);
    double mouse_x, mouse_y;
    win_glfw_get_mouse_pos(&mouse_x, &mouse_y);
    io.MousePos = ImVec2(mouse_x, mouse_y);

    double scroll_x, scroll_y;
    win_glfw_get_mouse_scroll(&scroll_x, &scroll_y);
    io.MouseWheelH += scroll_x;
    io.MouseWheel += scroll_y;
}
