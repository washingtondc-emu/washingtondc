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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iterator>
#include <fstream>
#include <sstream>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "BaseException.hpp"

#include "shader.hpp"

class ShaderError : public BaseException {
public:
    char const *what() const throw() {
        return "ShaderError";
    }
};

typedef boost::error_info<struct tag_shader_log_error_info, std::string>
errinfo_shader_log;

static const size_t LOG_LEN_GLSL = 1024;
GLchar shader_log[LOG_LEN_GLSL];

void shader_init(struct shader *out,
                 char const *vert_shader_src,
                 char const *frag_shader_src) {
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_shader_src, NULL);
    glCompileShader(vert_shader);

    GLint shader_success;
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(vert_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);

        BOOST_THROW_EXCEPTION(ShaderError() <<
                              errinfo_shader_log(shader_log));
    }

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_shader_src, NULL);
    glCompileShader(frag_shader);

    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(frag_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        BOOST_THROW_EXCEPTION(ShaderError() <<
                              errinfo_shader_log(shader_log));
    }

    GLuint shader_obj = glCreateProgram();
    glAttachShader(shader_obj, vert_shader);
    glAttachShader(shader_obj, frag_shader);
    glLinkProgram(shader_obj);

    glGetProgramiv(shader_obj, GL_LINK_STATUS, &shader_success);
    if (!shader_success) {
        glGetProgramInfoLog(shader_obj, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);
        glDeleteProgram(shader_obj);

        BOOST_THROW_EXCEPTION(ShaderError() <<
                              errinfo_shader_log(shader_log));
    }

    out->vert_shader = vert_shader;
    out->frag_shader = frag_shader;
    out->shader_prog_obj = shader_obj;
}

void shader_init_from_file(struct shader *out,
                           char const *vert_shader_path,
                           char const *frag_shader_path) {
    char *vert_shader_src = NULL, *frag_shader_src = NULL;

    try {
        std::ifstream vert_shader_stream(vert_shader_path);
        std::stringstream ss;

        ss << vert_shader_stream.rdbuf();
        vert_shader_src = strdup(ss.str().c_str());

    } catch (BaseException& exc) {
        if (vert_shader_src)
            free(vert_shader_src);
        if (frag_shader_src)
            free(frag_shader_src);
        exc << errinfo_path(vert_shader_path);
        throw exc;
    }

    try {
        std::ifstream frag_shader_stream(frag_shader_path);
        std::stringstream ss;

        ss << frag_shader_stream.rdbuf();
        frag_shader_src = strdup(ss.str().c_str());
    } catch (BaseException& exc) {
        if (vert_shader_src)
            free(vert_shader_src);
        if (frag_shader_src)
            free(frag_shader_src);
        exc << errinfo_path(frag_shader_path);
        throw exc;
    }

    // vert_shader_src = "";
    // frag_shader_src = "";

    shader_init(out, vert_shader_src, frag_shader_src);

    free(vert_shader_src);
    free(frag_shader_src);
}

void shader_cleanup(struct shader const *shader) {
    glDeleteProgram(shader->shader_prog_obj);
    glDeleteShader(shader->frag_shader);
    glDeleteShader(shader->vert_shader);
}
